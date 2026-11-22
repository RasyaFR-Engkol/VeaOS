/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : moupdo.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PDO Handling
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "mouse.h"

/* Revision History ------------------------------------------------------
 * DATE       : 22-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file moupdo.c
 * --------------------------------------------------------------------- */

static volatile LONG MouNextUnitId = 0;

VEASTATUS VEAPI MouArmPortRead(IN PMOUSE_DEVICE_EXTENSION DevExt);

VOID
VEAPI
MouInputDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);

VEASTATUS
VEAPI
MouCreatePdoMouse(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN PFILE_OBJECT PortFileObject,
    OUT PDEVICE_OBJECT *OutPdo OPTIONAL
)
{
    VEASTATUS Status;
    PDEVICE_OBJECT Pdo = NULL;
    PMOUSE_DEVICE_EXTENSION DevExt;
    CHAR NameBuffer[64];
    ULONG UnitId;

    UnitId = (ULONG)(InterlockedIncrement((PLONG)&MouNextUnitId) - 1);

    RtlStringCbPrintfA(NameBuffer, 64, "Mouse%lu", UnitId);

    Status = ObioCreateNewDevice(
        DriverObject,
        sizeof(MOUSE_DEVICE_EXTENSION),
        NameBuffer,
        FILE_DEVICE_MOUSE,
        &Pdo
    );

    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    DevExt = (PMOUSE_DEVICE_EXTENSION)Pdo->DeviceExtension;
    RtlZeroMemory(DevExt, sizeof(MOUSE_DEVICE_EXTENSION));

    DevExt->IsPdo = TRUE;
    DevExt->Self = Pdo;
    DevExt->PdoData.UnitId = UnitId;
    DevExt->PdoData.PortDevice = PortDeviceObject;
    DevExt->PdoData.PortFileObject = PortFileObject;
    DevExt->PdoData.ReadIndex = 0;
    DevExt->PdoData.WriteIndex = 0;

    KsInitializeSpinlock(&DevExt->PdoData.MouseLock);
    KsInitializeDpc(
        &DevExt->PdoData.InputDpc,
        MouInputDpcRoutine,
        DevExt
    );

    Pdo->Flags |= DO_BUFFERED_IO;
    Pdo->Flags &= ~DO_DEVICE_INITIALIZING;

    if (PortDeviceObject != NULL)
    {
        DevExt->PdoData.ReaderOip = ObioAllocateOip(
            (((OBJECT_TO_OBJECT_HEADER(PortDeviceObject))->StackDepth)) + 1
        );

        if (DevExt->PdoData.ReaderOip != NULL)
        {
            MouArmPortRead(DevExt);
        }
    }

    if (OutPdo != NULL)
    {
        *OutPdo = Pdo;
    }

    return STATUS_SUCCESS;
}

VOID
MouPushInputDataLocked(
    IN PMOUSE_DEVICE_EXTENSION DevExt,
    IN PMOUSE_INPUT_DATA Data
)
{
    ULONG NextWrite;

    NextWrite = (DevExt->PdoData.WriteIndex + 1) % MOU_BUFFER_SIZE;
    if (NextWrite != DevExt->PdoData.ReadIndex)
    {
        DevExt->PdoData.InputBuffer[DevExt->PdoData.WriteIndex] = *Data;
        DevExt->PdoData.InputBuffer[DevExt->PdoData.WriteIndex].UnitId = (USHORT)DevExt->PdoData.UnitId;
        DevExt->PdoData.WriteIndex = NextWrite;
    }
}

ULONG
MouPopInputDataLocked(
    IN PMOUSE_DEVICE_EXTENSION DevExt,
    OUT PMOUSE_INPUT_DATA OutBuffer,
    IN ULONG MaxPackets
)
{
    ULONG Count = 0;

    while (Count < MaxPackets &&
           DevExt->PdoData.ReadIndex != DevExt->PdoData.WriteIndex)
    {
        OutBuffer[Count++] = DevExt->PdoData.InputBuffer[DevExt->PdoData.ReadIndex];
        DevExt->PdoData.ReadIndex = (DevExt->PdoData.ReadIndex + 1) % MOU_BUFFER_SIZE;
    }

    return Count;
}

ULONG
MouPopInputData(
    IN PMOUSE_DEVICE_EXTENSION DevExt,
    OUT PMOUSE_INPUT_DATA OutBuffer,
    IN ULONG MaxPackets
)
{
    KIRQL OldIrql;
    ULONG Count;

    KsAcquireSpinLock(&DevExt->PdoData.MouseLock, &OldIrql);
    Count = MouPopInputDataLocked(DevExt, OutBuffer, MaxPackets);
    KsReleaseSpinLock(&DevExt->PdoData.MouseLock, OldIrql);

    return Count;
}

VEASTATUS
VEAPI
MouPortReadCompletion(
    IN PVOID Object,
    IN POIP Oip,
    IN PVOID Context
)
{
    UNREFERENCED_PARAMETER(Object);

    PMOUSE_DEVICE_EXTENSION DevExt = (PMOUSE_DEVICE_EXTENSION)Context;

    if (VEA_SUCCESS(Oip->StatusInformation.Status) &&
        Oip->StatusInformation.Information >= sizeof(MOUSE_INPUT_DATA))
    {
        KIRQL OldIrql;

        KsAcquireSpinLock(&DevExt->PdoData.MouseLock, &OldIrql);
        MouPushInputDataLocked(DevExt, &DevExt->PdoData.RawPacket);
        KsReleaseSpinLock(&DevExt->PdoData.MouseLock, OldIrql);
    }

    KsInsertQueueDpc(&DevExt->PdoData.InputDpc, NULL, NULL);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

VOID
VEAPI
MouInputDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
    PMOUSE_DEVICE_EXTENSION DevExt = (PMOUSE_DEVICE_EXTENSION)DeferredContext;
    KIRQL OldIrql;
    POIP UserOip = NULL;
    ULONG Copied = 0;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    KsAcquireSpinLock(&DevExt->PdoData.MouseLock, &OldIrql);

    if (DevExt->PdoData.PendingReadOip != NULL)
    {
        POIP_STACK UserStack =
            ObioGetCurrentOipStackLocation(DevExt->PdoData.PendingReadOip);
        ULONG MaxPackets =
            UserStack->Parameters.ReadWrite.Length / sizeof(MOUSE_INPUT_DATA);

        if (MaxPackets != 0 &&
            DevExt->PdoData.PendingReadOip->AssociatedOip.SystemBuffer != NULL)
        {
            Copied = MouPopInputDataLocked(
                DevExt,
                (PMOUSE_INPUT_DATA)DevExt->PdoData.PendingReadOip
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
                Copied * sizeof(MOUSE_INPUT_DATA);
        }
    }

    KsReleaseSpinLock(&DevExt->PdoData.MouseLock, OldIrql);

    if (UserOip != NULL)
    {
        ObioCompleteRequest(UserOip);
    }

    MouArmPortRead(DevExt);
}

VEASTATUS
VEAPI
MouArmPortRead(
    IN PMOUSE_DEVICE_EXTENSION DevExt
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
    NextStack->Parameters.ReadWrite.Length = sizeof(MOUSE_INPUT_DATA);
    NextStack->CompletionRoutine = MouPortReadCompletion;
    NextStack->Context = DevExt;

    return ObioCallObject(DevExt->PdoData.PortDevice, Oip);
}

VEASTATUS
VEAPI
MouFdoHandle(
    IN PDEVICE_OBJECT Fdo,
    IN POIP Oip
)
{
    PMOUSE_DEVICE_EXTENSION DevExt = (PMOUSE_DEVICE_EXTENSION)Fdo->DeviceExtension;
    POIP_STACK Stack = ObioGetCurrentOipStackLocation(Oip);

    switch (Stack->MajorFunction)
    {
        case OIP_MJ_READ:
        case OIP_MJ_WRITE:
        case OIP_MJ_CREATE:
        case OIP_MJ_CLOSE:
        case OIP_MJ_CLEANUP:
        case OIP_MJ_PNP:
            goto Unimplemented;

        default:
            goto RetToNextDevice;
    }

RetToNextDevice:
    if (DevExt->FdoData.LowerDevice)
    {
        ObioSkipCurrentOipStackLocation(Oip);
        return ObioCallObject(DevExt->FdoData.LowerDevice, Oip);
    }

Unimplemented:
    Oip->StatusInformation.Status = STATUS_NOT_IMPLEMENTED;
    ObioCompleteRequest(Oip);
    return STATUS_NOT_IMPLEMENTED;
}

VEASTATUS
VEAPI
MouPdoHandle(
    IN PDEVICE_OBJECT Pdo,
    IN POIP Oip
)
{
    PMOUSE_DEVICE_EXTENSION DevExt = (PMOUSE_DEVICE_EXTENSION)Pdo->DeviceExtension;
    POIP_STACK Stack = ObioGetCurrentOipStackLocation(Oip);
    VEASTATUS Status = STATUS_SUCCESS;

    switch (Stack->MajorFunction)
    {
        case OIP_MJ_CREATE:
        case OIP_MJ_CLOSE:
            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = 0;
            ObioCompleteRequest(Oip);
            return STATUS_SUCCESS;

        case OIP_MJ_CLEANUP:
        {
            KIRQL OldIrql;
            POIP Pending;

            KsAcquireSpinLock(&DevExt->PdoData.MouseLock, &OldIrql);
            Pending = DevExt->PdoData.PendingReadOip;
            DevExt->PdoData.PendingReadOip = NULL;
            KsReleaseSpinLock(&DevExt->PdoData.MouseLock, OldIrql);

            if (Pending != NULL)
            {
                Pending->StatusInformation.Status = STATUS_CANCELLED;
                Pending->StatusInformation.Information = 0;
                ObioCompleteRequest(Pending);
            }

            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = 0;
            ObioCompleteRequest(Oip);
            return STATUS_SUCCESS;
        }

        case OIP_MJ_READ:
        {
            ULONG MaxPackets = Stack->Parameters.ReadWrite.Length / sizeof(MOUSE_INPUT_DATA);
            if (MaxPackets == 0 || Oip->AssociatedOip.SystemBuffer == NULL)
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            KIRQL OldIrql;
            ULONG Copied;

            KsAcquireSpinLock(&DevExt->PdoData.MouseLock, &OldIrql);
            Copied = MouPopInputDataLocked(
                DevExt,
                (PMOUSE_INPUT_DATA)Oip->AssociatedOip.SystemBuffer,
                MaxPackets
            );

            if (Copied > 0)
            {
                KsReleaseSpinLock(&DevExt->PdoData.MouseLock, OldIrql);
                Oip->StatusInformation.Status = STATUS_SUCCESS;
                Oip->StatusInformation.Information = Copied * sizeof(MOUSE_INPUT_DATA);
                ObioCompleteRequest(Oip);
                return STATUS_SUCCESS;
            }

            if (DevExt->PdoData.PendingReadOip != NULL)
            {
                DPRINT("[MOU] Busy! PendingOip=%p, Self=%p, PortDev=%p\n",
                    DevExt->PdoData.PendingReadOip,
                    DevExt->Self,
                    DevExt->PdoData.PortDevice);
                KsReleaseSpinLock(&DevExt->PdoData.MouseLock, OldIrql);
                Status = STATUS_DEVICE_BUSY;
                break;
            }

            DevExt->PdoData.PendingReadOip = Oip;
            KsReleaseSpinLock(&DevExt->PdoData.MouseLock, OldIrql);
            return STATUS_PENDING;
        }

        default:
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    Oip->StatusInformation.Status = Status;
    Oip->StatusInformation.Information = 0;
    ObioCompleteRequest(Oip);
    return Status;
}

/* EOF */