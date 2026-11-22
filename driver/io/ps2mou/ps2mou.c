/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : ps2mou.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PS 2 Mouse
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "mou.h"

/* Revision History ------------------------------------------------------
 * DATE       : 22-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file ps2mou.c
 * --------------------------------------------------------------------- */

static
ULONG_PTR
PS2MouAcquireDeviceLock(
    IN  PKSPIN_LOCK Lock,
    OUT PKIRQL      OldIrql
)
{
    ULONG_PTR Flags;

    __asm__ __volatile__(
        "pushf\n\t"
        "pop %0\n\t"
        "cli"
        : "=r"(Flags)
        :
        : "memory"
    );

    KsAcquireSpinLock(Lock, OldIrql);
    return Flags;
}

static
VOID
PS2MouReleaseDeviceLock(
    IN PKSPIN_LOCK Lock,
    IN KIRQL       OldIrql,
    IN ULONG_PTR   Flags
)
{
    KsReleaseSpinLock(Lock, OldIrql);

    if (Flags & 0x200)
    {
        __asm__ __volatile__("sti" ::: "memory");
    }
}

static
UCHAR
PS2MouReadPort(IN USHORT Port)
{
    UCHAR Value;

    Value = __In8(Port);
    return Value;
}

static
VOID
PS2MouWritePort(IN USHORT Port, IN UCHAR Value)
{
    __Out8(Port, Value);
}

static
BOOLEAN
PS2MouWaitForInputBufferEmpty(IN ULONG CommandPort)
{
    for (ULONG Retry = 0; Retry < PS2MOU_POLL_LIMIT; Retry++)
    {
        if (!(PS2MouReadPort((USHORT)CommandPort) & 0x02))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static
BOOLEAN
PS2MouWaitForOutputBufferFull(IN ULONG CommandPort)
{
    for (ULONG Retry = 0; Retry < PS2MOU_POLL_LIMIT; Retry++)
    {
        if (PS2MouReadPort((USHORT)CommandPort) & 0x01)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static
VOID
PS2MouFlushOutputBuffer(
    IN ULONG DataPort,
    IN ULONG CommandPort
)
{
    for (ULONG Retry = 0; Retry < 64; Retry++)
    {
        if (!(PS2MouReadPort((USHORT)CommandPort) & 0x01))
        {
            break;
        }
        (VOID)PS2MouReadPort((USHORT)DataPort);
    }
}

static
BOOLEAN
PS2MouSendMouseCommand(
    IN ULONG DataPort,
    IN ULONG CommandPort,
    IN UCHAR Command
)
{
    for (ULONG Attempt = 0; Attempt < 3; Attempt++)
    {
        PS2MouFlushOutputBuffer(DataPort, CommandPort);

        if (!PS2MouWaitForInputBufferEmpty(CommandPort))
        {
            DPRINT("PS2MOU: Timeout tunggu IBF sebelum 0xD4 (cmd=0x%02X)\n", Command);
            return FALSE;
        }

        /* 0xD4: Arahkan byte berikutnya di DataPort ke PS/2 Auxiliary (Mouse) */
        PS2MouWritePort((USHORT)CommandPort, 0xD4);

        if (!PS2MouWaitForInputBufferEmpty(CommandPort))
        {
            DPRINT("PS2MOU: Timeout tunggu IBF sebelum cmd=0x%02X\n", Command);
            return FALSE;
        }

        PS2MouWritePort((USHORT)DataPort, Command);

        if (!PS2MouWaitForOutputBufferFull(CommandPort))
        {
            DPRINT("PS2MOU: Timeout tunggu ACK untuk cmd=0x%02X\n", Command);
            return FALSE;
        }

        UCHAR Resp = PS2MouReadPort((USHORT)DataPort);
        if (Resp == 0xFA)
        {
            return TRUE;
        }

        if (Resp == 0xFE)
        {
            /* Device minta Resend */
            continue;
        }

        DPRINT("PS2MOU: Unexpected resp 0x%02X untuk cmd=0x%02X\n", Resp, Command);
        return FALSE;
    }

    return FALSE;
}

static
BOOLEAN
PS2MouSetSampleRate(
    IN ULONG DataPort,
    IN ULONG CommandPort,
    IN UCHAR Rate
)
{
    if (!PS2MouSendMouseCommand(DataPort, CommandPort, 0xF3))
    {
        return FALSE;
    }

    return PS2MouSendMouseCommand(DataPort, CommandPort, Rate);
}

static
BOOLEAN
PS2MouDetectIntelliMouse(IN PPS2MOU_DEVICE_EXTENSION Extension)
{
    ULONG DataPort = Extension->DataPort;
    ULONG CommandPort = Extension->CommandPort;
    UCHAR DeviceId = 0x00;

    /* Magic knock sequence untuk mengaktifkan ekstensi IntelliMouse:
       Set Sample Rate 200 -> 100 -> 80, lalu baca Device ID (0xF2) */
    if (!PS2MouSetSampleRate(DataPort, CommandPort, 200) ||
        !PS2MouSetSampleRate(DataPort, CommandPort, 100) ||
        !PS2MouSetSampleRate(DataPort, CommandPort, 80))
    {
        return FALSE;
    }

    if (!PS2MouSendMouseCommand(DataPort, CommandPort, 0xF2))
    {
        return FALSE;
    }

    if (!PS2MouWaitForOutputBufferFull(CommandPort))
    {
        return FALSE;
    }

    DeviceId = PS2MouReadPort((USHORT)DataPort);
    Extension->DeviceId = DeviceId;

    /* 0x03 = IntelliMouse (3 tombol + scroll wheel, paket 4-byte)
       0x04 = IntelliMouse Explorer (5 tombol + scroll wheel, paket 4-byte) */
    if (DeviceId == 0x03 || DeviceId == 0x04)
    {
        Extension->HasWheel = TRUE;
        Extension->PacketLength = 4;
        return TRUE;
    }

    Extension->HasWheel = FALSE;
    Extension->PacketLength = 3;
    return FALSE;
}

static
BOOLEAN
PS2MouInitializeController(IN PPS2MOU_DEVICE_EXTENSION Extension)
{
    ULONG DataPort = Extension->DataPort;
    ULONG CommandPort = Extension->CommandPort;
    UCHAR OrigConfig;
    UCHAR WorkConfig;
    BOOLEAN KbdWasEnabled;

    /* 1. Disable sementara Port 1 (Keyboard) & Port 2 (Mouse) agar tidak tabrakan di 0x60 */
    if (!PS2MouWaitForInputBufferEmpty(CommandPort)) return FALSE;
    PS2MouWritePort((USHORT)CommandPort, 0xAD);

    if (!PS2MouWaitForInputBufferEmpty(CommandPort)) return FALSE;
    PS2MouWritePort((USHORT)CommandPort, 0xA7);

    PS2MouFlushOutputBuffer(DataPort, CommandPort);

    /* 2. Baca 8042 Controller Configuration Byte (0x20) */
    if (!PS2MouWaitForInputBufferEmpty(CommandPort)) return FALSE;
    PS2MouWritePort((USHORT)CommandPort, 0x20);

    if (!PS2MouWaitForOutputBufferFull(CommandPort))
    {
        DPRINT("PS2MOU: Gagal baca i8042 config byte\n");
        return FALSE;
    }
    OrigConfig = PS2MouReadPort((USHORT)DataPort);
    KbdWasEnabled = ((OrigConfig & 0x10) == 0);

    /* Matikan IRQ1 & IRQ12 selama proses probing, tapi nyalakan clock Port 2 (~0x20) */
    WorkConfig = OrigConfig;
    WorkConfig &= ~0x03; /* Clear bit 0 (IRQ1) & bit 1 (IRQ12) sementara */
    WorkConfig &= ~0x20; /* Clear bit 5 (Enable Mouse Clock) */

    if (!PS2MouWaitForInputBufferEmpty(CommandPort)) return FALSE;
    PS2MouWritePort((USHORT)CommandPort, 0x60);
    if (!PS2MouWaitForInputBufferEmpty(CommandPort)) return FALSE;
    PS2MouWritePort((USHORT)DataPort, WorkConfig);

    /* 3. Aktifkan Second PS/2 Port (Auxiliary / Mouse) */
    if (!PS2MouWaitForInputBufferEmpty(CommandPort)) return FALSE;
    PS2MouWritePort((USHORT)CommandPort, 0xA8);
    PS2MouFlushOutputBuffer(DataPort, CommandPort);

    /* 4. Reset ke pengaturan default (0xF6) */
    if (!PS2MouSendMouseCommand(DataPort, CommandPort, 0xF6))
    {
        DPRINT("PS2MOU: Gagal kirim 0xF6 (Set Defaults)\n");
        if (KbdWasEnabled && PS2MouWaitForInputBufferEmpty(CommandPort))
            PS2MouWritePort((USHORT)CommandPort, 0xAE);
        return FALSE;
    }

    /* 5. Probe IntelliMouse 4-byte wheel (fallback ke 3-byte jika tidak didukung) */
    Extension->DeviceId = 0x00;
    Extension->HasWheel = FALSE;
    Extension->PacketLength = 3;
    (VOID)PS2MouDetectIntelliMouse(Extension);

    /* Set sample rate kerja ke 100 Hz */
    (VOID)PS2MouSetSampleRate(DataPort, CommandPort, 100);

    /* 6. Enable Data Reporting (0xF4) */
    if (!PS2MouSendMouseCommand(DataPort, CommandPort, 0xF4))
    {
        DPRINT("PS2MOU: Gagal kirim 0xF4 (Enable Streaming)\n");
        if (KbdWasEnabled && PS2MouWaitForInputBufferEmpty(CommandPort))
            PS2MouWritePort((USHORT)CommandPort, 0xAE);
        return FALSE;
    }

    /* 7. Kembalikan Port 1 (jika sebelumnya aktif) dan aktifkan IRQ12 (+ IRQ1 jika aktif) */
    if (KbdWasEnabled)
    {
        if (PS2MouWaitForInputBufferEmpty(CommandPort))
        {
            PS2MouWritePort((USHORT)CommandPort, 0xAE);
        }
    }

    WorkConfig |= (OrigConfig & 0x01); /* Pertahankan IRQ1 keyboard */
    WorkConfig |= 0x02;                /* Aktifkan IRQ12 mouse */
    WorkConfig &= ~0x20;               /* Pastikan clock Port 2 tetap aktif */

    if (PS2MouWaitForInputBufferEmpty(CommandPort))
    {
        PS2MouWritePort((USHORT)CommandPort, 0x60);
        if (PS2MouWaitForInputBufferEmpty(CommandPort))
        {
            PS2MouWritePort((USHORT)DataPort, WorkConfig);
        }
    }

    return TRUE;
}

static
VOID
PS2MouDisableController(
    IN ULONG DataPort,
    IN ULONG CommandPort
)
{
    (VOID)PS2MouSendMouseCommand(DataPort, CommandPort, 0xF5);

    if (PS2MouWaitForInputBufferEmpty(CommandPort))
    {
        PS2MouWritePort((USHORT)CommandPort, 0xA7);
    }
}

BOOLEAN
VEAPI
PS2MouIrqHandler(IN PKINTERRUPT Interrupt, IN PVOID ServiceContext)
{
    PPS2MOU_DEVICE_EXTENSION Extension;
    KIRQL OldIrql;
    UCHAR Status;
    UCHAR DataByte;
    UCHAR FlagsByte;
    UCHAR CurrButtons;
    UCHAR ChangedButtons;
    LONG DeltaX;
    LONG DeltaY;
    CHAR WheelDelta = 0;
    MOUSE_INPUT_DATA Packet;
    POIP OipToComplete = NULL;
    ULONG_PTR LockFlags;

    UNREFERENCED_PARAMETER(Interrupt);

    Extension = (PPS2MOU_DEVICE_EXTENSION)ServiceContext;
    if (!Extension || !Extension->DataPort || !Extension->CommandPort)
    {
        return FALSE;
    }

    Status = PS2MouReadPort((USHORT)Extension->CommandPort);

    /* Bit 0 (0x01) = Output buffer full, Bit 5 (0x20) = Data dari Aux/Mouse */
    if ((Status & 0x21) != 0x21)
    {
        return FALSE;
    }

    DataByte = PS2MouReadPort((USHORT)Extension->DataPort);

    /* Validasi bit 3 (always 1) pada byte pertama untuk menjaga sinkronisasi paket */
    if (Extension->PacketIndex == 0 && !(DataByte & 0x08))
    {
        return TRUE;
    }

    Extension->PacketBytes[Extension->PacketIndex++] = DataByte;

    if (Extension->PacketIndex < Extension->PacketLength)
    {
        return TRUE;
    }

    /* Paket lengkap (3 byte atau 4 byte) */
    Extension->PacketIndex = 0;
    FlagsByte = Extension->PacketBytes[0];

    /* Abaikan paket jika terjadi overflow X atau Y */
    if (FlagsByte & 0xC0)
    {
        return TRUE;
    }

    DeltaX = (FlagsByte & 0x10)
        ? (LONG)((CHAR)Extension->PacketBytes[1])
        : (LONG)Extension->PacketBytes[1];

    DeltaY = (FlagsByte & 0x20)
        ? (LONG)((CHAR)Extension->PacketBytes[2])
        : (LONG)Extension->PacketBytes[2];

    CurrButtons = FlagsByte & 0x07;

    RtlZeroMemory(&Packet, sizeof(MOUSE_INPUT_DATA));
    Packet.Flags = MOUSE_MOVE_RELATIVE;
    Packet.LastX = DeltaX;
    Packet.LastY = -DeltaY; /* Konvensi layar: Y positif ke bawah */

    /* Ekstrak byte ke-4 jika IntelliMouse aktif */
    if (Extension->HasWheel)
    {
        UCHAR ExtraByte = Extension->PacketBytes[3];

        if (Extension->DeviceId == 0x04)
        {
            /* Mode 5-tombol (0x04): bit 0..3 signed wheel, bit 4=Btn4, bit 5=Btn5 */
            WheelDelta = (CHAR)((ExtraByte & 0x0F) << 4) >> 4;
            CurrButtons |= (ExtraByte & 0x30) >> 1; /* Map ke bit 3 & 4 */
        }
        else
        {
            /* Mode IntelliMouse standar (0x03): full signed 8-bit Z delta */
            WheelDelta = (CHAR)ExtraByte;
        }
    }

    ChangedButtons = CurrButtons ^ Extension->PreviousButtons;
    Packet.RawButtons = CurrButtons;

    if (ChangedButtons & 0x01)
    {
        Packet.ButtonFlags |= (CurrButtons & 0x01)
            ? MOUSE_LEFT_BUTTON_DOWN
            : MOUSE_LEFT_BUTTON_UP;
    }
    if (ChangedButtons & 0x02)
    {
        Packet.ButtonFlags |= (CurrButtons & 0x02)
            ? MOUSE_RIGHT_BUTTON_DOWN
            : MOUSE_RIGHT_BUTTON_UP;
    }
    if (ChangedButtons & 0x04)
    {
        Packet.ButtonFlags |= (CurrButtons & 0x04)
            ? MOUSE_MIDDLE_BUTTON_DOWN
            : MOUSE_MIDDLE_BUTTON_UP;
    }
    if (ChangedButtons & 0x08)
    {
        Packet.ButtonFlags |= (CurrButtons & 0x08)
            ? MOUSE_BUTTON_4_DOWN
            : MOUSE_BUTTON_4_UP;
    }
    if (ChangedButtons & 0x10)
    {
        Packet.ButtonFlags |= (CurrButtons & 0x10)
            ? MOUSE_BUTTON_5_DOWN
            : MOUSE_BUTTON_5_UP;
    }

    if (WheelDelta != 0)
    {
        Packet.ButtonFlags |= MOUSE_WHEEL;
        /* PS/2 melaporkan scroll ke bawah sebagai +1, sedangkan konvensi DDK
           menggunakan nilai positif untuk scroll ke atas (away from user) */
        Packet.ButtonData = (USHORT)(SHORT)(-WheelDelta * WHEEL_DELTA);
    }

    Extension->PreviousButtons = CurrButtons;

    LockFlags = PS2MouAcquireDeviceLock(&Extension->RingLock, &OldIrql);

    if (Extension->PendingReadOip != NULL)
    {
        /* Langsung serahkan ke continuous reader milik mouclass yang sedang menunggu */
        OipToComplete = Extension->PendingReadOip;
        Extension->PendingReadOip = NULL;
    }
    else if (Extension->RingCount < PS2MOU_RING_BUFFER_SIZE)
    {
        Extension->RingBuffer[Extension->RingWriteIndex] = Packet;
        Extension->RingWriteIndex =
            (Extension->RingWriteIndex + 1) % PS2MOU_RING_BUFFER_SIZE;
        Extension->RingCount++;
    }

    PS2MouReleaseDeviceLock(&Extension->RingLock, OldIrql, LockFlags);

    if (OipToComplete != NULL)
    {
        RtlCopyMemory(
            OipToComplete->AssociatedOip.SystemBuffer,
            &Packet,
            sizeof(MOUSE_INPUT_DATA)
        );
        OipToComplete->StatusInformation.Status = STATUS_SUCCESS;
        OipToComplete->StatusInformation.Information = sizeof(MOUSE_INPUT_DATA);
        ObioCompleteRequest(OipToComplete);
    }

    return TRUE;
}

VEASTATUS
VEAPI
PS2MouStartDevice(
    IN PDEVICE_OBJECT Fdo,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    POBJECT_HEADER ObjectHeader;
    PPS2MOU_DEVICE_EXTENSION Extension;
    POIP RequiredOip = NULL;
    PVOID TargetObject;
    VEASTATUS Status;
    KIRQL InterruptIrql;
    ULONG IsrVector;
    POBIO_REQUIRED_RESOURCES_LIST_EX ListEx;
    POBIO_REQUIRED_RESOURCES_LIST CurrentList;
    ULONG Index;
    BOOLEAN HasDataPort = FALSE;
    BOOLEAN HasCommandPort = FALSE;
    BOOLEAN HasInterrupt = FALSE;

    UNREFERENCED_PARAMETER(Oip);
    UNREFERENCED_PARAMETER(OipStack);

    if (!Fdo)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Extension = (PPS2MOU_DEVICE_EXTENSION)Fdo->DeviceExtension;
    if (!Extension)
    {
        Status = STATUS_INVALID_PARAMETER;
        goto cleanup;
    }

    KsInitializeSpinlock(&Extension->RingLock);
    Extension->RingReadIndex = 0;
    Extension->RingWriteIndex = 0;
    Extension->RingCount = 0;
    Extension->PacketIndex = 0;
    Extension->PacketLength = 3;
    Extension->HasWheel = FALSE;
    Extension->DeviceId = 0x00;
    Extension->PreviousButtons = 0;
    Extension->Interrupt = NULL;

    ObjectHeader = OBJECT_TO_OBJECT_HEADER(Fdo);
    RequiredOip = ObioAllocateOip(ObjectHeader->StackDepth);
    if (!RequiredOip)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto cleanup;
    }

    TargetObject = ObjectHeader->LowerAttachedObject;
    if (!TargetObject)
    {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        goto cleanup;
    }

    POIP_STACK NextStack = ObioGetNextOipStackLocation(RequiredOip);
    NextStack->MajorFunction = OIP_MJ_PNP;
    NextStack->MinorFunction = OIP_MN_QUERY_RESOURCE_REQUIREMENTS;

    Status = ObioGoInterruptObject(
        TargetObject,
        RequiredOip
    );
    if (!VEA_SUCCESS(Status))
    {
        goto cleanup;
    }

    ListEx = (POBIO_REQUIRED_RESOURCES_LIST_EX)
        RequiredOip->StatusInformation.Information;
    if (!ListEx || !ListEx->AlternativeLists)
    {
        DPRINT("PS2MOU: STATUS NOT FOUND FOR OBIO\n");
        Status = STATUS_NOT_FOUND;
        goto cleanup;
    }

    CurrentList = &ListEx->List[0];
    for (Index = 0; Index < CurrentList->Count; Index++)
    {
        POBIO_REQUIRED_RESOURCES Desc = &CurrentList->Descriptors[Index];

        if (Desc->Type == PnpResourceTypePort)
        {
            ULONG PortStart = (ULONG)Desc->Port.MinimumAddress.QuadPart;
            ULONG PortEnd = (ULONG)Desc->Port.MaximumAddress.QuadPart;

            if (PortEnd == PortStart && Desc->Port.Length > 1)
            {
                PortEnd = PortStart + Desc->Port.Length - 1;
            }

            if (PortStart <= 0x60 && PortEnd >= 0x60)
            {
                Extension->DataPort = 0x60;
                HasDataPort = TRUE;
            }

            if (PortStart <= 0x64 && PortEnd >= 0x64)
            {
                Extension->CommandPort = 0x64;
                HasCommandPort = TRUE;
            }
        }
        else if (Desc->Type == PnpResourceTypeInterrupt && !HasInterrupt)
        {
            if (Desc->Interrupt.MinimumVector > Desc->Interrupt.MaximumVector)
            {
                DPRINT("PS2MOU: IRQ INVALID\n");
                Status = STATUS_INVALID_PARAMETER;
                goto cleanup;
            }

            Extension->IrqVector = Desc->Interrupt.MinimumVector;
            HasInterrupt = TRUE;
        }
    }

    if (!HasDataPort)
    {
        Extension->DataPort = 0x60;
        HasDataPort = TRUE;
    }
    if (!HasCommandPort)
    {
        Extension->CommandPort = 0x64;
        HasCommandPort = TRUE;
    }

    if (!HasDataPort || !HasCommandPort || !HasInterrupt)
    {
        DPRINT("PS2MOU: DATA OR COMMAND PORT NOT FOUND\n");
        Status = STATUS_NOT_FOUND;
        goto cleanup;
    }

    if (!PS2MouInitializeController(Extension))
    {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        DPRINT("PS2MOU: MOUSE NOT CONNECTED\n");
        PS2MouDisableController(Extension->DataPort, Extension->CommandPort);
        goto cleanup;
    }

    IsrVector = HctIrqVectorToIsrVector(Extension->IrqVector);
    if (IsrVector > 0xBF)
    {
        Status = STATUS_INVALID_PARAMETER;
        DPRINT("PS2MOU: IRQ IS NOT VALID: 0x%x\n", IsrVector);
        PS2MouDisableController(Extension->DataPort, Extension->CommandPort);
        goto cleanup;
    }

    InterruptIrql = HctConvertVectorToIrql((UCHAR)IsrVector);
    DPRINT("IRQL FOR PS2MOU: %u\n", InterruptIrql);
    Status = ObioConnectInterrupt(
        &Extension->Interrupt,
        PS2MouIrqHandler,
        Extension,
        NULL,
        Extension->IrqVector,
        InterruptIrql,
        InterruptIrql,
        Latched,
        0
    );
    if (!VEA_SUCCESS(Status))
    {
        Extension->Interrupt = NULL;
        PS2MouDisableController(Extension->DataPort, Extension->CommandPort);
        goto cleanup;
    }

    while (PS2MouReadPort((USHORT)Extension->CommandPort) & 0x01)
    {
        (VOID)PS2MouReadPort((USHORT)Extension->DataPort);
    }

    if (Extension->InterfaceRegistered)
    {
        Status = ObioSetDeviceInterfaceState(&Extension->SymbolicLinkName, TRUE);
        if (!VEA_SUCCESS(Status))
        {
            ObioDisconnectInterrupt(Extension->Interrupt);
            Extension->Interrupt = NULL;
            PS2MouDisableController(Extension->DataPort, Extension->CommandPort);
            goto cleanup;
        }
    }

    DPRINT("PS2MOU started: data=0x%x command=0x%x irq=%u devId=0x%x wheel=%u\n",
           Extension->DataPort,
           Extension->CommandPort,
           Extension->IrqVector,
           Extension->DeviceId,
           Extension->HasWheel);
    Status = STATUS_SUCCESS;

cleanup:
    Fdo->Flags &= ~DO_DEVICE_INITIALIZING;

    if (RequiredOip)
    {
        ObioFreeOip(RequiredOip);
    }

    if (!VEA_SUCCESS(Status) && Extension && Extension->Interrupt)
    {
        ObioDisconnectInterrupt(Extension->Interrupt);
        Extension->Interrupt = NULL;
    }

    return Status;
}

VEASTATUS
VEAPI
PS2MouObjectInterruptHandler(PVOID Object, POIP Oip)
{
    POIP_STACK Stack = ObioGetCurrentOipStackLocation(Oip);

    switch (Stack->MajorFunction)
    {
        case OIP_MJ_READ:
        {
            PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
            PPS2MOU_DEVICE_EXTENSION Extension;
            ULONG MaxPackets;
            ULONG PacketsRead = 0;
            KIRQL OldIrql;
            ULONG_PTR LockFlags;

            if (!DeviceObject || !DeviceObject->DeviceExtension)
            {
                Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
                Oip->StatusInformation.Information = 0;
                break;
            }

            Extension = (PPS2MOU_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
            MaxPackets = Stack->Parameters.ReadWrite.Length / sizeof(MOUSE_INPUT_DATA);

            if (MaxPackets == 0 || !Oip->AssociatedOip.SystemBuffer)
            {
                Oip->StatusInformation.Status = STATUS_BUFFER_TOO_SMALL;
                Oip->StatusInformation.Information = 0;
                break;
            }

            LockFlags = PS2MouAcquireDeviceLock(&Extension->RingLock, &OldIrql);

            if (Extension->RingCount == 0)
            {
                if (Extension->PendingReadOip != NULL)
                {
                    PS2MouReleaseDeviceLock(&Extension->RingLock, OldIrql, LockFlags);
                    Oip->StatusInformation.Status = STATUS_DEVICE_BUSY;
                    Oip->StatusInformation.Information = 0;
                    break;
                }

                /* Tahan OIP sebagai pending sampai IRQ12 masuk */
                Extension->PendingReadOip = Oip;
                PS2MouReleaseDeviceLock(&Extension->RingLock, OldIrql, LockFlags);
                return STATUS_PENDING;
            }

            PMOUSE_INPUT_DATA OutBuf =
                (PMOUSE_INPUT_DATA)Oip->AssociatedOip.SystemBuffer;

            while (PacketsRead < MaxPackets && Extension->RingCount > 0)
            {
                OutBuf[PacketsRead++] = Extension->RingBuffer[Extension->RingReadIndex];
                Extension->RingReadIndex =
                    (Extension->RingReadIndex + 1) % PS2MOU_RING_BUFFER_SIZE;
                Extension->RingCount--;
            }

            PS2MouReleaseDeviceLock(&Extension->RingLock, OldIrql, LockFlags);

            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = PacketsRead * sizeof(MOUSE_INPUT_DATA);
            ObioCompleteRequest(Oip);
            return STATUS_SUCCESS;
        }

        case OIP_MJ_CREATE:
        case OIP_MJ_CLOSE:
        {
            Oip->StatusInformation.Status = STATUS_SUCCESS;
            break;
        }

        case OIP_MJ_CLEANUP:
        {
            PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
            if (DeviceObject && DeviceObject->DeviceExtension)
            {
                PPS2MOU_DEVICE_EXTENSION Extension =
                    (PPS2MOU_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
                KIRQL OldIrql;
                POIP Pending;
                ULONG_PTR LockFlags;

                LockFlags = PS2MouAcquireDeviceLock(&Extension->RingLock, &OldIrql);
                Pending = Extension->PendingReadOip;
                Extension->PendingReadOip = NULL;
                PS2MouReleaseDeviceLock(&Extension->RingLock, OldIrql, LockFlags);

                if (Pending != NULL)
                {
                    Pending->StatusInformation.Status = STATUS_CANCELLED;
                    Pending->StatusInformation.Information = 0;
                    ObioCompleteRequest(Pending);
                }
            }
            Oip->StatusInformation.Status = STATUS_SUCCESS;
            break;
        }

        case OIP_MJ_PNP:
        {
            PDEVICE_OBJECT DeviceObject;
            PPS2MOU_DEVICE_EXTENSION Extension;

            if (Stack->MinorFunction == OIP_MN_START_DEVICE)
            {
                return PS2MouStartDevice(Object, Oip, Stack);
            }

            DeviceObject = (PDEVICE_OBJECT)Object;
            Extension = (PPS2MOU_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
            if (!Extension || !Extension->LowerDevice)
            {
                Oip->StatusInformation.Status = STATUS_DEVICE_NOT_READY;
                Oip->StatusInformation.Information = 0;
                return STATUS_DEVICE_NOT_READY;
            }

            ObioSkipCurrentOipStackLocation(Oip);
            return ObioGoInterruptObject(Extension->LowerDevice, Oip);
        }

        default:
        {
            Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
            break;
        }
    }

    return Oip->StatusInformation.Status;
}

VEASTATUS
VEAPI
PS2MouAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT TargetDevice 
)
{
    VEASTATUS Status;
    PDEVICE_OBJECT Fdo = NULL;
    PPS2MOU_DEVICE_EXTENSION Extension;

    DPRINT("PS2MOU AddDevice called for PDO %p\n", TargetDevice);

    if (!DriverObject || !TargetDevice)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = ObioCreateNewDevice(
        DriverObject,
        sizeof(PS2MOU_DEVICE_EXTENSION),
        NULL,
        FILE_DEVICE_MOUSE,
        &Fdo
    );

    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    Extension = (PPS2MOU_DEVICE_EXTENSION)Fdo->DeviceExtension;
    RtlZeroMemory(Extension, sizeof(PS2MOU_DEVICE_EXTENSION));

    Extension->Pdo = TargetDevice;
    Extension->LowerDevice = ObioAttachObject(Fdo, TargetDevice);
    if (!Extension->LowerDevice)
    {
        ObioDeleteDevice(Fdo);
        return STATUS_UNSUCCESSFUL;
    }

    ObioAttachInterruptToThisObject(Fdo, PS2MouObjectInterruptHandler);

    Fdo->Flags |= DO_BUFFERED_IO;

    Status = ObioRegisterDeviceInterface(
        TargetDevice,
        &VEA_GUID_DEVINTERFACE_MOUSE,
        NULL,
        &Extension->SymbolicLinkName
    );
    if (VEA_SUCCESS(Status))
    {
        Extension->InterfaceRegistered = TRUE;

        ObioSetDeviceInterfaceState(&Extension->SymbolicLinkName, TRUE);
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
DriverEntry(PVOID DriverObject)
{
    PDRIVER_OBJECT PDriverObject = (PDRIVER_OBJECT)DriverObject;
    DPRINT("PS2MOU Driver Loaded\n");

    PDriverObject->AddDeviceRoutine = PS2MouAddDevice;

    return ObioAttachInterruptToThisObject(DriverObject, PS2MouObjectInterruptHandler);
}

/* EOF */