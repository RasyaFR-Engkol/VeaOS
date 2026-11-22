/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : mousemain.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PURPOSE
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "mouse.h"

/* Revision History ------------------------------------------------------
 * DATE       : 22-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file mousemain.c
 * --------------------------------------------------------------------- */

PVOID MouNotificationEntry;

VEASTATUS
VEAPI
MouObIntHandler(
    IN PVOID Object,
    IN POIP Oip
)
{
    PDEVICE_OBJECT DevObj = (PDEVICE_OBJECT)Object;
    PMOUSE_DEVICE_EXTENSION DevExt = (PMOUSE_DEVICE_EXTENSION)DevObj->DeviceExtension;

    if (DevExt != NULL && DevExt->IsPdo)
    {
        return MouPdoHandle(DevObj, Oip);
    }
    else
    {
        return MouFdoHandle(DevObj, Oip);
    }
}

VEASTATUS
VEAPI
DriverEntry(IN PDRIVER_OBJECT DriverObject)
{
    VEASTATUS Status;

    //
    // Register them to PNP Notification and ObInt
    //
    Status = ObioRegisterPlugPlayNotification(
        EventCategoryDeviceInterfaceChange,
        PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
        (PVOID)&VEA_GUID_DEVINTERFACE_MOUSE,
        DriverObject,
        MouDeviceChange,
        (PVOID)DriverObject,
        &MouNotificationEntry
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[MOU] Register PnP Notification failed: 0x%x\n", Status);
        return Status;
    }

    Status = ObioAttachInterruptToThisObject(
        DriverObject,
        MouObIntHandler
    );

    if (!VEA_SUCCESS(Status)) 
    {
        DPRINT("[MOU] Attach Interrupt Object failed: 0x%x\n", Status);
        return Status;
    }

    return STATUS_SUCCESS;
}