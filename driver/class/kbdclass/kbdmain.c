/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : kbdmain.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PURPOSE
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "kbdheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 17-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file kbdmain.c
 * --------------------------------------------------------------------- */

PVOID KbdNotifyEntry;

VEASTATUS
VEAPI
KbdObIntHandler(
    IN PVOID Object,
    IN POIP Oip
)
{
    PDEVICE_OBJECT DevObj = (PDEVICE_OBJECT)Object;
    PKBD_DEVICE_EXTENSION DevExt = (PKBD_DEVICE_EXTENSION)DevObj->DeviceExtension;

    if (DevExt != NULL && DevExt->IsPdo)
    {
        return KbdPdoHandle(DevObj, Oip);
    }
    else
    {
        return KbdFdoHandle(DevObj, Oip);
    }
}

VEASTATUS
VEAPI
DriverEntry(IN PDRIVER_OBJECT DriverObject)
{
    VEASTATUS Status;

    //
    // We hear Keyboard through PnP notification instead.
    // The corresponding keyboard should be exist even tho it's loaded
    // at System Service Start
    //

    Status = ObioRegisterPlugPlayNotification(
        EventCategoryDeviceInterfaceChange,
        PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
        (PVOID)&VEA_GUID_DEVINTERFACE_KEYBOARD,
        DriverObject,
        KbdPnpNotifyCallback,
        (PVOID)DriverObject,
        &KbdNotifyEntry
    );

    if (!VEA_SUCCESS(Status)) return Status;

    Status = ObioAttachInterruptToThisObject(
        (PVOID)DriverObject,
        KbdObIntHandler
    );

    if (!VEA_SUCCESS(Status)) return Status;

    return STATUS_SUCCESS;
}

/* EOF */