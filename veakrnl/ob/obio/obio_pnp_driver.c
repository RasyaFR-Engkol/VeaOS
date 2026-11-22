/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_pnp_driver.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : As PNP Drive
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 02-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_pnp_driver.c
 * --------------------------------------------------------------------- */

LIST_ENTRY ObioPnpRootPdoList;
KSPIN_LOCK ObioPnpRootLock;

static
VEASTATUS
ObiopPnpRootQueryDeviceRelations(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP_STACK Stack,
    OUT PULONG_PTR Information
)
{
    PPNPROOT_PDO_EXTENSION PdoExt;
    PDEVICE_RELATIONS Relations;
    PLIST_ENTRY Entry;
    KIRQL OldIrql;
    ULONG Count = 0;
    ULONG Index = 0;
    ULONG AllocSize;

    PdoExt = (PPNPROOT_PDO_EXTENSION)DeviceObject->DeviceExtension;
    if (!PdoExt || !PdoExt->IsRootBus ||
        Stack->Parameters.QueryDeviceRelation.Type != BusRelations)
    {
        return STATUS_NOT_SUPPORTED;
    }

    KsAcquireSpinLock(&ObioPnpRootLock, &OldIrql);

    for (Entry = ObioPnpRootPdoList.Flink;
         Entry != &ObioPnpRootPdoList;
         Entry = Entry->Flink)
    {
        Count++;
    }

    AllocSize = sizeof(DEVICE_RELATIONS) +
                (Count > 0 ? (Count - 1) * sizeof(PDEVICE_OBJECT) : 0);

    Relations = (PDEVICE_RELATIONS)UlAllocatePoolZero(
        NonPagedPool,
        AllocSize,
        TAG_OBIO
    );
    if (!Relations)
    {
        KsReleaseSpinLock(&ObioPnpRootLock, OldIrql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Relations->Count = Count;
    for (Entry = ObioPnpRootPdoList.Flink;
         Entry != &ObioPnpRootPdoList;
         Entry = Entry->Flink)
    {
        PPNPROOT_PDO_EXTENSION ChildExt;
        PDEVICE_OBJECT ChildPdo;

        ChildExt = CONTAINING_RECORD(Entry, PNPROOT_PDO_EXTENSION, ListEntry);
        ChildPdo = (PDEVICE_OBJECT)((PUCHAR)ChildExt - sizeof(DEVICE_OBJECT));

        ObReferenceObject(ChildPdo);
        Relations->Objects[Index++] = ChildPdo;
    }

    KsReleaseSpinLock(&ObioPnpRootLock, OldIrql);

    *Information = (ULONG_PTR)Relations;
    return STATUS_SUCCESS;
}

static
VEASTATUS
ObiopPnpRootQueryId(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP_STACK Stack,
    OUT PULONG_PTR Information
)
{
    PPNPROOT_PDO_EXTENSION PdoExt;

    PdoExt = (PPNPROOT_PDO_EXTENSION)DeviceObject->DeviceExtension;
    if (!PdoExt)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    switch (Stack->Parameters.QueryId.IdType)
    {
        case BusQueryDeviceID:
            *Information = (ULONG_PTR)PdoExt->DeviceId;
            return STATUS_SUCCESS;

        case BusQueryInstanceID:
            *Information = (ULONG_PTR)PdoExt->InstanceId;
            return STATUS_SUCCESS;

        default:
            return STATUS_NOT_SUPPORTED;
    }
}

VEASTATUS
VEAPI
ObioPnpRootInterruptHandler(
    PVOID Object,
    POIP Oip
)
{
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    POIP_STACK Stack = Oip->CurrentStackLocation;
    VEASTATUS Status = STATUS_NOT_SUPPORTED;
    ULONG_PTR Info = 0;

    switch (Stack->MajorFunction)
    {
        case OIP_MJ_CREATE:
        case OIP_MJ_CLOSE:
            Status = STATUS_SUCCESS;
            break;

        case OIP_MJ_PNP:
            switch (Stack->MinorFunction)
            {
                case OIP_MN_START_DEVICE:
                    Status = STATUS_SUCCESS;
                    break;

                case OIP_MN_QUERY_DEVICE_RELATIONS:
                    Status = ObiopPnpRootQueryDeviceRelations(DeviceObject, Stack, &Info);
                    break;

                case OIP_MN_QUERY_ID:
                    Status = ObiopPnpRootQueryId(DeviceObject, Stack, &Info);
                    break;

                case OIP_MN_QUERY_RESOURCES:
                case OIP_MN_QUERY_RESOURCE_REQUIREMENTS:
                    Status = STATUS_NOT_SUPPORTED;
                    break;

                default:
                    Status = STATUS_SUCCESS;
                    break;
            }
            break;

        default:
            Status = STATUS_NOT_SUPPORTED;
            break;
    }

    Oip->StatusInformation.Status = Status;
    Oip->StatusInformation.Information = Info;
    ObioCompleteRequest(Oip);
    return Status;
}

VEASTATUS
VEAPI
ObioPnpRootDriverEntry(PDRIVER_OBJECT DriverObject)
{
    if (!DriverObject) return STATUS_INVALID_PARAMETER;

    // Daftarin handler ke DRIVER_OBJECT ini -- ini setara "MajorFunction[]" NT,
    // cuma bentuknya 1 fungsi + switch internal, bukan array dispatch table
    return ObioAttachInterruptToThisObject(DriverObject, ObioPnpRootInterruptHandler);
}