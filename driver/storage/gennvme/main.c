/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : main.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PURPOSE
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "nvmeheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 06-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file main.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
NVMeMjCreateClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    Oip->StatusInformation.Status = STATUS_SUCCESS;
    Oip->StatusInformation.Information = 0;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
NVMeMjPnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PNVME_DEVICE_EXTENSION DevExt;
    PNVME_DEVICE_EXTENSION ControllerExt;

    switch (OipStack->MinorFunction)
    {
        case OIP_MN_START_DEVICE:
            DevExt = (PNVME_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
            if (DevExt->PdoDisk)
            {
                ControllerExt = DevExt->ControllerExtension;
                if (!ControllerExt || ControllerExt->State != NvmeStateStarted)
                {
                    Oip->StatusInformation.Status = STATUS_DEVICE_NOT_READY;
                    return STATUS_DEVICE_NOT_READY;
                }

                Oip->StatusInformation.Status = STATUS_SUCCESS;
                Oip->StatusInformation.Information = 0;
                return STATUS_SUCCESS;
            }
            return NVMeStartDevice(DeviceObject, Oip, OipStack);

        case OIP_MN_QUERY_DEVICE_RELATIONS:
            DevExt = (PNVME_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
            if (OipStack->Parameters.QueryDeviceRelation.Type == BusRelations)
            {
                PDEVICE_RELATIONS DeviceRelations = NULL;
                VEASTATUS Status;

                Status = NVMeQueryDeviceRelations(
                    DevExt,
                    OipStack->Parameters.QueryDeviceRelation.Type,
                    &DeviceRelations
                );
                Oip->StatusInformation.Status = Status;
                Oip->StatusInformation.Information = (ULONG_PTR)DeviceRelations;
                return Status;
            }

            if (DevExt->PdoDisk || !DevExt->NextLowerDevice)
            {
                Oip->StatusInformation.Status = STATUS_SUCCESS;
                Oip->StatusInformation.Information = 0;
                return STATUS_SUCCESS;
            }

            ObioSkipCurrentOipStackLocation(Oip);
            return ObioGoInterruptObject(DevExt->NextLowerDevice, Oip);

        case OIP_MN_QUERY_ID:
            DevExt = (PNVME_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
            if (DevExt->PdoDisk)
            {
                return NVMeQueryNamespaceId(
                    DevExt,
                    Oip,
                    OipStack->Parameters.QueryId.IdType
                );
            }

            if (!DevExt->NextLowerDevice)
            {
                Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
                Oip->StatusInformation.Information = 0;
                return STATUS_NOT_SUPPORTED;
            }

            ObioSkipCurrentOipStackLocation(Oip);
            return ObioGoInterruptObject(DevExt->NextLowerDevice, Oip);
    }

    DPRINT("NVMEUNHANDLED: Unhandled OIP_MJ_PNP: 0x%x\n", OipStack->MinorFunction);

    Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
    return STATUS_NOT_SUPPORTED;
}

VEASTATUS
VEAPI
NVMeObIntHandler(
    IN PVOID Object,
    IN POIP Oip
)
{
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    PNVME_DEVICE_EXTENSION DevExt = (PNVME_DEVICE_EXTENSION)DeviceObject;
    POIP_STACK OipStack = ObioGetCurrentOipStackLocation(Oip);

    switch (OipStack->MajorFunction)
    {
        case OIP_MJ_PNP:
        {
            return NVMeMjPnp(DeviceObject, Oip, OipStack);
        }

        case OIP_MJ_CREATE:
        case OIP_MJ_CLOSE:
        {
            return NVMeMjCreateClose(DeviceObject, Oip);
        }

        case OIP_MJ_DEVICE_CONTROL:
        {
            return NVMeMjDeviceControl(DeviceObject, Oip, OipStack);
            break;
        }

        case OIP_MJ_READ:
        case OIP_MJ_WRITE:
        {
            return NVMeMjReadWrite(DeviceObject, Oip, OipStack);
            break;
        }

        case OIP_MJ_FLUSH_BUFFERS:
        {
            break;
        }

        case OIP_MJ_INTERNAL_DEVICE_CTL:
        {
            break;
        }

        case OIP_MJ_SHUTDOWN:
        {
            break;
        }
    }

    
    Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
    return STATUS_NOT_SUPPORTED;
}

VEASTATUS
VEAPI
NVMeAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT Target
)
{
    VEASTATUS Status;
    PDEVICE_OBJECT DeviceObject;
    PNVME_DEVICE_EXTENSION Extension;

    Status = ObioCreateNewDevice(
        DriverObject,
        sizeof(NVME_DEVICE_EXTENSION),
        NULL,
        FILE_DEVICE_CONTROLLER,
        &DeviceObject
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("NVMe: Failed to add device. Status: 0x%x\n", Status);
        return Status;
    }

    Extension = (PNVME_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    RtlZeroMemory(Extension, sizeof(NVME_DEVICE_EXTENSION));

    InitializeListHead(&Extension->ChildList);
    KsInitializeSpinlock(&Extension->ChildLock);

    Extension->NextLowerDevice = ObioAttachObject(DeviceObject, Target);
    if (!Extension->NextLowerDevice)
    {
        DPRINT("NVMe: Failed to attach device\n");
        ObioDeleteDevice(DeviceObject);
        return STATUS_OBJECT_ATTACH_FAIL;
    }

    Extension->PhysicalDeviceObject = Target;
    Extension->DeviceObject = DeviceObject;
    Extension->DriverObject = DriverObject;
    Extension->State = NvmeStateStopped;
    Extension->InterruptType = InterruptLineBased;
    Extension->NextCommandId = 0;
    Extension->PdoDisk = FALSE;
    KsInitializeSpinlock(&Extension->InterruptLock);

    NVMeInitializeQueueState(
        &Extension->AdminQueue,
        NVME_ADMIN_QUEUE_ID,
        NVME_ADMIN_QUEUE_DEPTH);
    NVMeInitializeQueueState(
        &Extension->IoQueue,
        NVME_IO_QUEUE_ID,
        NVME_IO_QUEUE_DEPTH);

    KsInitializeDpc(&Extension->CompletionDpc, NVMeCompletionDpcRoutine, Extension);

    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    DPRINT("NVMe: AddDevice OK, DeviceObject=%p\n", DeviceObject);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
DriverEntry(IN PDRIVER_OBJECT DriverObject)
{
    DPRINT("Sucessfully added GenNVME driver\n");

    DriverObject->AddDeviceRoutine = NVMeAddDevice;

    return ObioAttachInterruptToThisObject((PVOID)DriverObject, NVMeObIntHandler);
}

/* EOF */