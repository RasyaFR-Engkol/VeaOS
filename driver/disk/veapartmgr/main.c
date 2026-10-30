/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : main.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VeaPM OIP and Entry Function
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "veapm.h"

/* Revision History ------------------------------------------------------
 * DATE       : 10-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file main.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
PmObIntHandler(
    IN PVOID Object,
    IN POIP Oip
)
{
    POIP_STACK OipStack = ObioGetCurrentOipStackLocation(Oip);
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    PPM_DEVICE_EXTENSION DevExt = (PPM_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    DPRINT("PmObIntHandler\n");
    if (DevExt->DeviceType == PmDeviceTypeDiskFilter)
    {
        DPRINT("Request for FDO. Skipping\n");
        ObioSkipCurrentOipStackLocation(Oip);
        return ObioGoInterruptObject(DevExt->LowerDeviceObject, Oip);
    }

    if (DevExt->DeviceType == PmDeviceTypePartition)
    {
        DPRINT("Request for PDO.\n");
        return PmPartitionInt(DeviceObject, Oip, OipStack, DevExt);
    }

    Oip->StatusInformation.Status = STATUS_INVALID_DEVICE_REQUEST;
    return STATUS_INVALID_DEVICE_REQUEST;
}

VEASTATUS
VEAPI
PmAddDevice(IN PDRIVER_OBJECT DriverObject, IN PDEVICE_OBJECT TargetDevice)
{
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
DriverEntry(IN PDRIVER_OBJECT DriverObject)
{
    VEASTATUS Status;

    // Just incase
    DriverObject->AddDeviceRoutine = PmAddDevice;

    Status = ObioAttachInterruptToThisObject(
        (PVOID)DriverObject, PmObIntHandler
    );
    if (!VEA_SUCCESS(Status)) {
        PM_ERROR("Failed to attach interrupt for driver: 0x%x", Status);
        return Status;
    }

    Status = ObioRegisterPlugPlayNotification(
        EventCategoryDeviceInterfaceChange,
        PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
        (PVOID)&VEA_GUID_DEVINTERFACE_DISK,
        DriverObject,
        PmPnpNotificationCallback,
        (PVOID)DriverObject,
        &PmNotificationEntry
    );
    if (!VEA_SUCCESS(Status)) {
        PM_ERROR("Failed to register PnP notification: 0x%x\n", Status);
        return Status;
    }

    PM_INFO("Succesfully initializing Pm Driver\n");

    return STATUS_SUCCESS;
}