/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : mounotify.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Mouse Notify
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "mouse.h"

/* Revision History ------------------------------------------------------
 * DATE       : 22-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file mounotify.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
MouHandleArrivalMouse(
    IN PDEVICE_INTERFACE_CHANGE_NOTIFICATION Notification,
    IN PVOID Context
)
{
    VEASTATUS Status;
    PDRIVER_OBJECT DriverObject;
    PDEVICE_OBJECT PortDeviceObject = NULL;
    PFILE_OBJECT FileObject = NULL;
    PDEVICE_OBJECT Pdo = NULL;

    if (Notification == NULL || Context == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    DriverObject = (PDRIVER_OBJECT)Context;

    Status = ObioGetDeviceObjectPointer(
        Notification->SymbolicLinkName,
        FILE_READ_DATA,
        &FileObject,
        &PortDeviceObject
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[MOU] Failed to get port device pointer: 0x%x\n", Status);
        return Status;
    }

    Status = MouCreatePdoMouse(
        DriverObject,
        PortDeviceObject,
        FileObject,
        &Pdo
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[MOU] Failed to create PDO Mouse: 0x%x\n", Status);
        if (FileObject != NULL)
        {
            ObDereferenceObject(FileObject);
        }
        return Status;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
MouHandleRemovalMouse(
    IN PDEVICE_INTERFACE_CHANGE_NOTIFICATION Notification,
    IN PVOID Context
)
{
    UNREFERENCED_PARAMETER(Notification);
    UNREFERENCED_PARAMETER(Context);

    UNIMPLEMENTED_DBGBREAK;
    return STATUS_NOT_IMPLEMENTED;
}

VEASTATUS
VEAPI
MouDeviceChange(
    IN PVOID NotificationStructure,
    IN PVOID Context
)
{
    PDEVICE_INTERFACE_CHANGE_NOTIFICATION NotifStruct =
        (PDEVICE_INTERFACE_CHANGE_NOTIFICATION)NotificationStructure;

    if (IsEqualGUID(&NotifStruct->Event, &GUID_DEVICE_INTERFACE_ARRIVAL))
    {
        //
        // Mouse PLUGGED (PS2 or XHCI hot path)
        //
        return MouHandleArrivalMouse(NotifStruct, Context);
    }
    else if (IsEqualGUID(&NotifStruct->Event, &GUID_DEVICE_INTERFACE_REMOVAL))
    {
        //
        // Mouse UNPLUGGED (XHCI hot path)
        //
        return MouHandleRemovalMouse(NotifStruct, Context);
    }

    return STATUS_INVALID_DEVICE_REQUEST;
}