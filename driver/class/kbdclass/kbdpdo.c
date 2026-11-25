/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : kbdpdo.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Keyboard PDO
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "kbdheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 18-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file kbdpdo.c
 * --------------------------------------------------------------------- */

static volatile LONG KbdNextUnitId = 0;
static volatile LONG KbdQueuedPacketCount = 0;
static volatile LONG KbdDroppedPacketCount = 0;
static volatile LONG KbdDeliveredPacketCount = 0;

VEASTATUS VEAPI KbdArmPortRead(IN PKBD_DEVICE_EXTENSION DevExt);

VOID
VEAPI
KbdRecordDeliveredPackets(
    IN PKBD_DEVICE_EXTENSION DevExt,
    IN ULONG Count
)
{
    ULONG Index;
#if 0
    for (Index = 0; Index < Count; Index++)
    {
        LONG Total = InterlockedIncrement((PLONG)&KbdDeliveredPacketCount);
        if (Total == 1 || (Total & 0x0F) == 0)
        {
            DPRINT("[KBD] unit=%u delivered=%d\n",
                   DevExt->PdoData.UnitId,
                   Total);
        }
    }
#endif
}

VOID
VEAPI
KbdInputDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);

VEASTATUS
VEAPI
KbdCreatePdoKeyboard(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN PFILE_OBJECT PortFileObject,
    OUT PDEVICE_OBJECT *OutPdo OPTIONAL
)
{
    VEASTATUS Status;
    PDEVICE_OBJECT Pdo = NULL;
    PKBD_DEVICE_EXTENSION DevExt;
    CHAR NameBuffer[64];
    ULONG UnitId;

    UnitId = (ULONG)(InterlockedIncrement((PLONG)&KbdNextUnitId) - 1);

    RtlStringCbPrintfA(NameBuffer, 64, "Keyboard%lu", UnitId);

    Status = ObioCreateNewDevice(
        DriverObject,
        sizeof(KBD_DEVICE_EXTENSION),
        NameBuffer,
        FILE_DEVICE_KEYBOARD,
        &Pdo
    );

    if (!VEA_SUCCESS(Status)) return Status;

    DevExt = (PKBD_DEVICE_EXTENSION)Pdo->DeviceExtension;
    RtlZeroMemory(DevExt, sizeof(KBD_DEVICE_EXTENSION));

    DevExt->IsPdo = TRUE;
    DevExt->Self = Pdo;
    DevExt->PdoData.UnitId = UnitId;
    DevExt->PdoData.PortDevice = PortDeviceObject;
    DevExt->PdoData.ReadIndex = 0;
    DevExt->PdoData.WriteIndex = 0;
    DevExt->PdoData.PortFileObject = PortFileObject;
    KsInitializeSpinlock(&DevExt->PdoData.KeyboardLock);
    KsInitializeDpc(
        &DevExt->PdoData.InputDpc,
        KbdInputDpcRoutine,
        DevExt
    );

    Pdo->Flags |= DO_BUFFERED_IO;
    Pdo->Flags &= ~DO_DEVICE_INITIALIZING;

    if (PortDeviceObject != NULL) {
        DevExt->PdoData.ReaderOip = ObioAllocateOip((((OBJECT_TO_OBJECT_HEADER(PortDeviceObject))->StackDepth)) + 1);
        if (DevExt->PdoData.ReaderOip != NULL) {
            KbdArmPortRead(DevExt);
        }
    }

    if (OutPdo != NULL) {
        *OutPdo = Pdo;
    }

    return STATUS_SUCCESS;
}

BOOLEAN
KbdPushInputDataLocked(
    IN PKBD_DEVICE_EXTENSION DevExt,
    IN PKEYBOARD_INPUT_DATA Data
)
{
    ULONG NextWrite;

    NextWrite = (DevExt->PdoData.WriteIndex + 1) % KBD_BUFFER_SIZE;
    if (NextWrite != DevExt->PdoData.ReadIndex) {
        DevExt->PdoData.InputBuffer[DevExt->PdoData.WriteIndex] = *Data;
        DevExt->PdoData.InputBuffer[DevExt->PdoData.WriteIndex].UnitId = (USHORT)DevExt->PdoData.UnitId;
        DevExt->PdoData.WriteIndex = NextWrite;
        return TRUE;
    }

    return FALSE;
}

ULONG
KbdPopInputDataLocked(
    IN PKBD_DEVICE_EXTENSION DevExt,
    OUT PKEYBOARD_INPUT_DATA OutBuffer,
    IN ULONG MaxPackets
)
{
    ULONG Count = 0;

    while (Count < MaxPackets &&
           DevExt->PdoData.ReadIndex != DevExt->PdoData.WriteIndex)
    {
        OutBuffer[Count++] = DevExt->PdoData.InputBuffer[DevExt->PdoData.ReadIndex];
        DevExt->PdoData.ReadIndex = (DevExt->PdoData.ReadIndex + 1) % KBD_BUFFER_SIZE;
    }

    return Count;
}

ULONG
KbdPopInputData(
    IN PKBD_DEVICE_EXTENSION DevExt,
    OUT PKEYBOARD_INPUT_DATA OutBuffer,
    IN ULONG MaxPackets
)
{
    KIRQL OldIrql;
    ULONG Count;

    KsAcquireSpinLock(&DevExt->PdoData.KeyboardLock, &OldIrql);
    Count = KbdPopInputDataLocked(DevExt, OutBuffer, MaxPackets);
    KsReleaseSpinLock(&DevExt->PdoData.KeyboardLock, OldIrql);
    return Count;
}

VEASTATUS
VEAPI
KbdPortReadCompletion(
    IN PVOID Object,
    IN POIP Oip,
    IN PVOID Context
)
{
    UNREFERENCED_PARAMETER(Object);

    PKBD_DEVICE_EXTENSION DevExt = (PKBD_DEVICE_EXTENSION)Context;

    if (VEA_SUCCESS(Oip->StatusInformation.Status) &&
        Oip->StatusInformation.Information >= sizeof(KEYBOARD_INPUT_DATA))
    {
        KIRQL OldIrql;
        BOOLEAN Queued;

        KsAcquireSpinLock(&DevExt->PdoData.KeyboardLock, &OldIrql);
        KbdPushInputDataLocked(DevExt, &DevExt->PdoData.RawPacket);
        KsReleaseSpinLock(&DevExt->PdoData.KeyboardLock, OldIrql);
    }

    KsInsertQueueDpc(&DevExt->PdoData.InputDpc, NULL, NULL);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

VOID
VEAPI
KbdInputDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
    PKBD_DEVICE_EXTENSION DevExt = (PKBD_DEVICE_EXTENSION)DeferredContext;
    KIRQL OldIrql;
    POIP UserOip = NULL;
    ULONG Copied = 0;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    KsAcquireSpinLock(&DevExt->PdoData.KeyboardLock, &OldIrql);

    if (DevExt->PdoData.PendingReadOip != NULL)
    {
        POIP_STACK UserStack =
            ObioGetCurrentOipStackLocation(DevExt->PdoData.PendingReadOip);
        ULONG MaxPackets =
            UserStack->Parameters.ReadWrite.Length / sizeof(KEYBOARD_INPUT_DATA);

        if (MaxPackets != 0 &&
            DevExt->PdoData.PendingReadOip->AssociatedOip.SystemBuffer != NULL)
        {
            Copied = KbdPopInputDataLocked(
                DevExt,
                (PKEYBOARD_INPUT_DATA)DevExt->PdoData.PendingReadOip
                    ->AssociatedOip.SystemBuffer,
                MaxPackets
            );
        }

        if (Copied != 0)
        {
            UserOip = DevExt->PdoData.PendingReadOip;
            DevExt->PdoData.PendingReadOip = NULL;
            UserOip->StatusInformation.Status = STATUS_SUCCESS;
            UserOip->StatusInformation.Information =
                Copied * sizeof(KEYBOARD_INPUT_DATA);
        }
    }

    KsReleaseSpinLock(&DevExt->PdoData.KeyboardLock, OldIrql);

    if (UserOip != NULL)
    {
        KbdRecordDeliveredPackets(DevExt, Copied);
        ObioCompleteRequest(UserOip);
    }

    KbdArmPortRead(DevExt);
}

VEASTATUS
VEAPI
KbdArmPortRead(
    IN PKBD_DEVICE_EXTENSION DevExt
)
{
    POIP Oip = DevExt->PdoData.ReaderOip;
    POIP_STACK NextStack;

    Oip->CurrentLocation = Oip->StackCount + 1;
    Oip->CurrentStackLocation = (POIP_STACK)(Oip + 1) + Oip->StackCount;
    NextStack = ObioGetNextOipStackLocation(Oip);

    Oip->StatusInformation.Status = STATUS_PENDING;
    Oip->StatusInformation.Information = 0;
    Oip->AssociatedOip.SystemBuffer = &DevExt->PdoData.RawPacket;
    Oip->Flags = OIP_BUFFERED_IO | OIP_INPUT_OPERATION;

    RtlZeroMemory(NextStack, sizeof(OIP_STACK));
    NextStack->MajorFunction = OIP_MJ_READ;
    NextStack->TargetObject = DevExt->PdoData.PortDevice;
    NextStack->Parameters.ReadWrite.Length = sizeof(KEYBOARD_INPUT_DATA);
    NextStack->CompletionRoutine = KbdPortReadCompletion;
    NextStack->Context = DevExt;

    return ObioCallObject(DevExt->PdoData.PortDevice, Oip);
}

/* EOF */