#include <veaddk.h>

#define PS2KEY_RING_BUFFER_SIZE 256
#define PS2KEY_IO_TIMEOUT       100000

typedef struct _PS2KEY_DEVICE_EXTENSION {
    PVOID Pdo;              
    PVOID LowerDevice;      
    ULONG Flags;
    ULONG DataPort;
    ULONG CommandPort;
    ULONG IrqVector;
    PKINTERRUPT Interrupt;

    ANSI_STRING SymbolicLinkName;
    BOOLEAN InterfaceRegistered;

    KSPIN_LOCK RingLock;
    KEYBOARD_INPUT_DATA RingBuffer[PS2KEY_RING_BUFFER_SIZE];
    ULONG RingReadIndex;
    ULONG RingWriteIndex;
    ULONG RingCount;
    BOOLEAN ExpectE0;
    POIP PendingReadOip;
    KDPC IsrDpc;
} PS2KEY_DEVICE_EXTENSION, *PPS2KEY_DEVICE_EXTENSION;

static
UCHAR
PS2KeyReadPort(IN USHORT Port)
{
    UCHAR Value;

    __asm__ volatile("inb %1, %0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

static
VOID
PS2KeyWritePort(IN USHORT Port, IN UCHAR Value)
{
    __asm__ volatile("outb %0, %1" : : "a"(Value), "Nd"(Port));
}

static
BOOLEAN
PS2KeyWaitForInputBufferEmpty(IN ULONG CommandPort)
{
    ULONG Retry;

    for (Retry = 0; Retry < PS2KEY_IO_TIMEOUT; Retry++)
    {
        if (!(PS2KeyReadPort((USHORT)CommandPort) & 0x02))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static
BOOLEAN
PS2KeyReadControllerOutput(
    IN ULONG DataPort,
    IN ULONG CommandPort,
    OUT PUCHAR Value
)
{
    ULONG Retry;
    UCHAR Status;

    for (Retry = 0; Retry < PS2KEY_IO_TIMEOUT; Retry++)
    {
        Status = PS2KeyReadPort((USHORT)CommandPort);
        if (!(Status & 0x01))
        {
            continue;
        }

        *Value = PS2KeyReadPort((USHORT)DataPort);
        if (!(Status & 0x20))
            return TRUE;
    }

    return FALSE;
}

static
BOOLEAN
PS2KeyWaitForKeyboardAck(
    IN ULONG DataPort,
    IN ULONG CommandPort
)
{
    ULONG Retry;
    UCHAR Status;
    UCHAR Response;

    for (Retry = 0; Retry < PS2KEY_IO_TIMEOUT; Retry++)
    {
        Status = PS2KeyReadPort((USHORT)CommandPort);
        if (!(Status & 0x01))
        {
            continue;
        }

        Response = PS2KeyReadPort((USHORT)DataPort);
        if (!(Status & 0x20) && Response == 0xFA)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static
BOOLEAN
PS2KeyInitializeController(
    IN ULONG DataPort,
    IN ULONG CommandPort
)
{
    ULONG Retry;
    UCHAR Status;
    UCHAR Config;
    BOOLEAN OutputBufferEmpty = FALSE;

    /* Stop keyboard traffic while draining the shared controller output. */
    if (!PS2KeyWaitForInputBufferEmpty(CommandPort))
        return FALSE;
    PS2KeyWritePort((USHORT)CommandPort, 0xAD);

    for (Retry = 0; Retry < PS2KEY_IO_TIMEOUT; Retry++)
    {
        Status = PS2KeyReadPort((USHORT)CommandPort);
        if (!(Status & 0x01))
        {
            OutputBufferEmpty = TRUE;
            break;
        }
        (VOID)PS2KeyReadPort((USHORT)DataPort);
    }

    if (!OutputBufferEmpty)
    {
        return FALSE;
    }

    /* 2. Read controller config, ignoring bytes currently arriving from the mouse. */
    if (!PS2KeyWaitForInputBufferEmpty(CommandPort))
        return FALSE;
    PS2KeyWritePort((USHORT)CommandPort, 0x20);
    if (!PS2KeyReadControllerOutput(DataPort, CommandPort, &Config))
        return FALSE;

    Config |= 0x01;   /* Enable IRQ1 */
    Config &= ~0x10;  /* Enable Port 1 Clock */

    if (!PS2KeyWaitForInputBufferEmpty(CommandPort))
    {
        return FALSE;
    }
    PS2KeyWritePort((USHORT)CommandPort, 0x60);
    if (!PS2KeyWaitForInputBufferEmpty(CommandPort))
    {
        return FALSE;
    }
    PS2KeyWritePort((USHORT)DataPort, Config);

    /* 3. Enable Port 1 and reset scanning before enabling it. */
    if (!PS2KeyWaitForInputBufferEmpty(CommandPort))
        return FALSE;
    PS2KeyWritePort((USHORT)CommandPort, 0xAE);

    if (!PS2KeyWaitForInputBufferEmpty(CommandPort))
        return FALSE;
    PS2KeyWritePort((USHORT)DataPort, 0xF5);
    if (!PS2KeyWaitForInputBufferEmpty(CommandPort) ||
        !PS2KeyWaitForKeyboardAck(DataPort, CommandPort))
        return FALSE;

    if (!PS2KeyWaitForInputBufferEmpty(CommandPort))
        return FALSE;
    PS2KeyWritePort((USHORT)DataPort, 0xF4);
    return PS2KeyWaitForInputBufferEmpty(CommandPort) &&
           PS2KeyWaitForKeyboardAck(DataPort, CommandPort);
}

static
VOID
PS2KeyDisableController(IN ULONG CommandPort)
{
    if (PS2KeyWaitForInputBufferEmpty(CommandPort))
    {
        PS2KeyWritePort((USHORT)CommandPort, 0xAD);
    }
}

VEASTATUS 
VEAPI
PS2KeyObjectInterruptHandler(PVOID Object, POIP Oip);

VOID
VEAPI
PS2KeyDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
    PPS2KEY_DEVICE_EXTENSION Extension = (PPS2KEY_DEVICE_EXTENSION)DeferredContext;
    KIRQL OldIrql;
    POIP OipToComplete = NULL;
    ULONG PacketsRead = 0;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    if (!Extension)
    {
        return;
    }

    KsAcquireSpinLock(&Extension->RingLock, &OldIrql);

    if (Extension->PendingReadOip != NULL && Extension->RingCount > 0)
    {
        POIP Oip = Extension->PendingReadOip;
        POIP_STACK Stack = ObioGetCurrentOipStackLocation(Oip);
        ULONG MaxPackets = Stack->Parameters.ReadWrite.Length / sizeof(KEYBOARD_INPUT_DATA);
        PKEYBOARD_INPUT_DATA OutBuf = (PKEYBOARD_INPUT_DATA)Oip->AssociatedOip.SystemBuffer;

        if (MaxPackets > 0 && OutBuf != NULL)
        {
            while (PacketsRead < MaxPackets && Extension->RingCount > 0)
            {
                OutBuf[PacketsRead++] = Extension->RingBuffer[Extension->RingReadIndex];
                Extension->RingReadIndex =
                    (Extension->RingReadIndex + 1) % PS2KEY_RING_BUFFER_SIZE;
                Extension->RingCount--;
            }

            Extension->PendingReadOip = NULL;
            OipToComplete = Oip;
        }
    }

    KsReleaseSpinLock(&Extension->RingLock, OldIrql);

    if (OipToComplete != NULL)
    {
        OipToComplete->StatusInformation.Status = STATUS_SUCCESS;
        OipToComplete->StatusInformation.Information = PacketsRead * sizeof(KEYBOARD_INPUT_DATA);
        ObioCompleteRequest(OipToComplete);
    }
}

VEASTATUS
VEAPI
PS2KeyHandleMnAddDevice(
    IN PVOID DriverObject,
    IN POIP Oip,
    IN POIP_STACK Stack
)
{
    /* Legacy */
    return STATUS_SUCCESS;
}

BOOLEAN
VEAPI
PS2KeyIrqHandler(IN PKINTERRUPT Interrupt, IN PVOID ServiceContext)
{
    PPS2KEY_DEVICE_EXTENSION Extension = (PPS2KEY_DEVICE_EXTENSION)ServiceContext;
    KIRQL OldIrql;
    UCHAR Status;
    UCHAR ScanCode;
    KEYBOARD_INPUT_DATA Packet;
    BOOLEAN NeedDpc = FALSE;

    UNREFERENCED_PARAMETER(Interrupt);

    if (!Extension || !Extension->DataPort || !Extension->CommandPort)
    {
        return FALSE;
    }

    Status = PS2KeyReadPort((USHORT)Extension->CommandPort);
    if ((Status & 0x21) != 0x01)
    {
        return FALSE;
    }

    ScanCode = PS2KeyReadPort((USHORT)Extension->DataPort);

    if (ScanCode == 0xE0)
    {
        Extension->ExpectE0 = TRUE;
        return TRUE;
    }

    RtlZeroMemory(&Packet, sizeof(KEYBOARD_INPUT_DATA));
    Packet.MakeCode = (USHORT)(ScanCode & 0x7F);
    Packet.Flags = (ScanCode & 0x80) ? KEY_BREAK : KEY_MAKE;
    if (Extension->ExpectE0)
    {
        Packet.Flags |= KEY_E0;
        Extension->ExpectE0 = FALSE;
    }

    KsAcquireSpinLock(&Extension->RingLock, &OldIrql);

    if (Extension->RingCount < PS2KEY_RING_BUFFER_SIZE)
    {
        Extension->RingBuffer[Extension->RingWriteIndex] = Packet;
        Extension->RingWriteIndex =
            (Extension->RingWriteIndex + 1) % PS2KEY_RING_BUFFER_SIZE;
        Extension->RingCount++;
    }

    if (Extension->PendingReadOip != NULL)
    {
        NeedDpc = TRUE;
    }

    KsReleaseSpinLock(&Extension->RingLock, OldIrql);

    if (NeedDpc)
    {
        KsInsertQueueDpc(&Extension->IsrDpc, NULL, NULL);
    }

    return TRUE;
}

VEASTATUS
VEAPI
PS2KeyStartDevice(
    IN PDEVICE_OBJECT Fdo,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    POBJECT_HEADER ObjectHeader;
    PPS2KEY_DEVICE_EXTENSION Extension;
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

    if (!Fdo)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Extension = (PPS2KEY_DEVICE_EXTENSION)Fdo->DeviceExtension;
    if (!Extension)
    {
        Status = STATUS_INVALID_PARAMETER;
        goto cleanup;
    }

    KsInitializeSpinlock(&Extension->RingLock);
    KsInitializeDpc(&Extension->IsrDpc, PS2KeyDpcRoutine, Extension);
    Extension->RingReadIndex = 0;
    Extension->RingWriteIndex = 0;
    Extension->RingCount = 0;
    Extension->Interrupt = NULL;

    ObjectHeader = OBJECT_TO_OBJECT_HEADER(Fdo);
    RequiredOip = ObioAllocateOip(ObjectHeader->StackDepth);
    if (!RequiredOip) {
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
                Status = STATUS_INVALID_PARAMETER;
                goto cleanup;
            }

            Extension->IrqVector = Desc->Interrupt.MinimumVector;
            HasInterrupt = TRUE;
        }
    }

    if (!HasDataPort || !HasCommandPort || !HasInterrupt)
    {
        Status = STATUS_NOT_FOUND;
        goto cleanup;
    }

    if (!PS2KeyInitializeController(Extension->DataPort, Extension->CommandPort))
    {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        PS2KeyDisableController(Extension->CommandPort);
        goto cleanup;
    }

    IsrVector = HctIrqVectorToIsrVector(Extension->IrqVector);
    if (IsrVector > 0xBF)
    {
        Status = STATUS_INVALID_PARAMETER;
        PS2KeyDisableController(Extension->CommandPort);
        goto cleanup;
    }

    InterruptIrql = HctConvertVectorToIrql((UCHAR)IsrVector);
    DPRINT("IRQL FOR PS2: %u\n", InterruptIrql);
    Status = ObioConnectInterrupt(
        &Extension->Interrupt,
        PS2KeyIrqHandler,
        Extension,
        NULL,
        Extension->IrqVector,
        InterruptIrql,
        InterruptIrql,
        Latched,
        0);
    if (!VEA_SUCCESS(Status))
    {
        Extension->Interrupt = NULL;
        PS2KeyDisableController(Extension->CommandPort);
        goto cleanup;
    }

    while (PS2KeyReadPort((USHORT)Extension->CommandPort) & 0x01)
    {
        (VOID)PS2KeyReadPort((USHORT)Extension->DataPort);
    }

    if (Extension->InterfaceRegistered)
    {
        Status = ObioSetDeviceInterfaceState(&Extension->SymbolicLinkName, TRUE);
        if (!VEA_SUCCESS(Status))
        {
            ObioDisconnectInterrupt(Extension->Interrupt);
            Extension->Interrupt = NULL;
            PS2KeyDisableController(Extension->CommandPort);
            goto cleanup;
        }
    }

    DPRINT("PS2KEY started: data=0x%x command=0x%x irq=%u\n",
           Extension->DataPort, Extension->CommandPort, Extension->IrqVector);
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
PS2KeyObjectInterruptHandler(PVOID Object, POIP Oip)
{
    POIP_STACK Stack = ObioGetCurrentOipStackLocation(Oip);

    switch (Stack->MajorFunction)
    {
        case OIP_MJ_READ:
        {
            PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
            PPS2KEY_DEVICE_EXTENSION Extension;
            ULONG MaxPackets;
            ULONG PacketsRead = 0;
            KIRQL OldIrql;

            if (!DeviceObject || !DeviceObject->DeviceExtension)
            {
                Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
                Oip->StatusInformation.Information = 0;
                break;
            }

            Extension = (PPS2KEY_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
            MaxPackets = Stack->Parameters.ReadWrite.Length / sizeof(KEYBOARD_INPUT_DATA);

            if (MaxPackets == 0 || !Oip->AssociatedOip.SystemBuffer)
            {
                Oip->StatusInformation.Status = STATUS_BUFFER_TOO_SMALL;
                Oip->StatusInformation.Information = 0;
                break;
            }

            KsAcquireSpinLock(&Extension->RingLock, &OldIrql);

            if (Extension->RingCount == 0)
            {
                if (Extension->PendingReadOip != NULL)
                {
                    KsReleaseSpinLock(&Extension->RingLock, OldIrql);
                    Oip->StatusInformation.Status = STATUS_DEVICE_BUSY;
                    Oip->StatusInformation.Information = 0;
                    break;
                }

                // Tahan OIP sebagai pending sampai IRQ1 masuk
                Extension->PendingReadOip = Oip;
                KsReleaseSpinLock(&Extension->RingLock, OldIrql);
                return STATUS_PENDING;
            }

            PKEYBOARD_INPUT_DATA OutBuf =
                (PKEYBOARD_INPUT_DATA)Oip->AssociatedOip.SystemBuffer;

            while (PacketsRead < MaxPackets && Extension->RingCount > 0)
            {
                OutBuf[PacketsRead++] = Extension->RingBuffer[Extension->RingReadIndex];
                Extension->RingReadIndex =
                    (Extension->RingReadIndex + 1) % PS2KEY_RING_BUFFER_SIZE;
                Extension->RingCount--;
            }

            KsReleaseSpinLock(&Extension->RingLock, OldIrql);

            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = PacketsRead * sizeof(KEYBOARD_INPUT_DATA);
            ObioCompleteRequest(Oip);
            return STATUS_SUCCESS;
        }

        case OIP_MJ_CREATE:
        {
            Oip->StatusInformation.Status = STATUS_SUCCESS;
            break;
        }

        case OIP_MJ_PNP:
        {
            PDEVICE_OBJECT DeviceObject;
            PPS2KEY_DEVICE_EXTENSION Extension;

            if (Stack->MinorFunction == OIP_MN_ADD_DEVICE)
            {
                return PS2KeyHandleMnAddDevice(Object, Oip, Stack);
            }
            else if (Stack->MinorFunction == OIP_MN_START_DEVICE)
            {
                return PS2KeyStartDevice(Object, Oip, Stack);
            }

            DeviceObject = (PDEVICE_OBJECT)Object;
            Extension = (PPS2KEY_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
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
PS2KeyAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT TargetDevice // Ini PDO dari Obio (ACPI/PNP0303/0)
)
{
    VEASTATUS Status;
    PDEVICE_OBJECT Fdo = NULL;
    PPS2KEY_DEVICE_EXTENSION Extension;
    PVOID TargetObject;

    if (!DriverObject || !TargetDevice)
    {
        return STATUS_INVALID_PARAMETER;
    }

    /* 1. Bikin Functional Device Object (FDO) buat ps2key */
    Status = ObioCreateNewDevice(
        DriverObject,
        sizeof(PS2KEY_DEVICE_EXTENSION),
        NULL, /* Nama device opsional, misal "\\Device\\KeyboardClass0" */
        FILE_DEVICE_KEYBOARD,
        &Fdo
    );

    if (!VEA_SUCCESS(Status)) return Status;

    Extension = (PPS2KEY_DEVICE_EXTENSION)Fdo->DeviceExtension;
    Extension->Pdo = TargetDevice;
    Extension->LowerDevice = ObioAttachObject(Fdo, TargetDevice);
    if (!Extension->LowerDevice)
    {
        ObioDeleteDevice(Fdo);
        return STATUS_UNSUCCESSFUL;
    }

    /* Attach Object Interrupt handler ke FDO */
    ObioAttachInterruptToThisObject(Fdo, PS2KeyObjectInterruptHandler);

    Fdo->Flags |= DO_BUFFERED_IO;

    Status = ObioRegisterDeviceInterface(
        TargetDevice,                       // PDO (ACPI/PNP0303/0)
        &VEA_GUID_DEVINTERFACE_KEYBOARD,
        NULL,
        &Extension->SymbolicLinkName
    );
    if (VEA_SUCCESS(Status))
    {
        Extension->InterfaceRegistered = TRUE;
    }

    return STATUS_SUCCESS;
}

VEASTATUS 
VEAPI
DriverEntry(PVOID DriverObject)
{
    PDRIVER_OBJECT PDriverObject = (PDRIVER_OBJECT)DriverObject;
    DPRINT("PS2KEY Driver Loaded\n");

    PDriverObject->AddDeviceRoutine = PS2KeyAddDevice;
    
    return ObioAttachInterruptToThisObject(DriverObject, PS2KeyObjectInterruptHandler);
}