/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : kbdnotify.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Keyboard PnP Notify
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "kbdheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 17-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file kbdnotify.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
KbdHandleArrivalKeyboard(
    IN PDEVICE_INTERFACE_CHANGE_NOTIFICATION Notification,
    IN PVOID Context
)
{
    VEASTATUS Status;
    PDRIVER_OBJECT DriverObject;
    PDEVICE_OBJECT PortDeviceObject = NULL;
    PFILE_OBJECT FileObject = NULL;
    PDEVICE_OBJECT Pdo = NULL;

    if (Notification == NULL || Context == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    DriverObject = (PDRIVER_OBJECT)Context;
    
    Status = ObioGetDeviceObjectPointer(
        Notification->SymbolicLinkName,
        FILE_READ_DATA,
        &FileObject,
        &PortDeviceObject
    );

    if (!VEA_SUCCESS(Status)) {
        return Status;
    }

    Status = KbdCreatePdoKeyboard(
        DriverObject,
        PortDeviceObject,
        FileObject,
        &Pdo
    );

    if (!VEA_SUCCESS(Status)) {
        if (FileObject != NULL) {
            ObDereferenceObject(FileObject);
        }
        return Status;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
KbdHandleRemovalKeyboard(
    IN PDEVICE_INTERFACE_CHANGE_NOTIFICATION Notification,
    IN PVOID Context
)
{
    UNIMPLEMENTED_DBGBREAK;
    return STATUS_NOT_IMPLEMENTED;
}

VEASTATUS
VEAPI
KbdPnpNotifyCallback(
    IN PVOID NotificationStructure,
    IN PVOID Context
)
{
    PDEVICE_INTERFACE_CHANGE_NOTIFICATION NotifStruct = 
        (PDEVICE_INTERFACE_CHANGE_NOTIFICATION)NotificationStructure;

    if (IsEqualGUID(&NotifStruct->Event, &GUID_DEVICE_INTERFACE_ARRIVAL))
    {
        //
        // Keyboard PLUGGED (PS2 or XHCI hot path)
        //

        return KbdHandleArrivalKeyboard(NotifStruct, Context);
    }
    else if (IsEqualGUID(&NotifStruct->Event, &GUID_DEVICE_INTERFACE_REMOVAL))
    {
        //
        // Keyboard UNPLUGGED (XHCI hot path)
        //

        return KbdHandleRemovalKeyboard(NotifStruct, Context);
    }

    return STATUS_INVALID_DEVICE_REQUEST;
}

/* EOF */