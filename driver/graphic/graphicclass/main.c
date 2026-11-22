/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : main.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PURPOSE
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "graphic.h"

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file main.c
 * --------------------------------------------------------------------- */

PVOID GrpNotificationEntry = NULL;
ULONG GrpNextDisplayIndex = 0;

VEASTATUS
VEAPI
GrpObIntHandler(
    IN PVOID Object,
    IN POIP Oip
)
{
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    PGRAPHIC_DEVICE_EXTENSION DevExt = (PGRAPHIC_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    POIP_STACK Stack = ObioGetCurrentOipStackLocation(Oip);
    VEASTATUS Status;

    if (!DevExt || DevExt->IsRemoved)
    {
        Oip->StatusInformation.Status = STATUS_DEVICE_REMOVED;
        Oip->StatusInformation.Information = 0;
        ObioCompleteRequest(Oip);
        return STATUS_DEVICE_REMOVED;
    }

    switch (Stack->MajorFunction)
    {
        case OIP_MJ_CREATE:
        {
            KIRQL OldIrql;
            KsAcquireSpinLock(&DevExt->DeviceLock, &OldIrql);
            DevExt->OpenCount++;
            KsReleaseSpinLock(&DevExt->DeviceLock, OldIrql);

            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = 0;
            ObioCompleteRequest(Oip);
            return STATUS_SUCCESS;
        }

        case OIP_MJ_CLOSE:
        {
            KIRQL OldIrql;
            KsAcquireSpinLock(&DevExt->DeviceLock, &OldIrql);
            if (DevExt->OpenCount > 0)
            {
                DevExt->OpenCount--;
            }
            KsReleaseSpinLock(&DevExt->DeviceLock, OldIrql);

            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = 0;
            ObioCompleteRequest(Oip);
            return STATUS_SUCCESS;
        }

        case OIP_MJ_DEVICE_CONTROL:
        {
            /* Forward langsung IOCTL ke driver miniport/VBE di bawahnya */
            if (!DevExt->LowerDeviceObject)
            {
                Oip->StatusInformation.Status = STATUS_INVALID_DEVICE_STATE;
                Oip->StatusInformation.Information = 0;
                ObioCompleteRequest(Oip);
                return STATUS_INVALID_DEVICE_STATE;
            }

            ObioSkipCurrentOipStackLocation(Oip);
            Status = ObioCallObject(DevExt->LowerDeviceObject, Oip);
            return Status;
        }

        default:
            goto Unimplemented;
    }

Unimplemented:
    Oip->StatusInformation.Status = STATUS_NOT_IMPLEMENTED;
    Oip->StatusInformation.Information = 0;
    ObioCompleteRequest(Oip);
    return STATUS_NOT_IMPLEMENTED;
}

VEASTATUS
VEAPI
GrpCreateNewDevice(
    IN PDEVICE_INTERFACE_CHANGE_NOTIFICATION NotificationStructure,
    IN PDRIVER_OBJECT DriverObject
)
{
    VEASTATUS Status;
    PFILE_OBJECT LowerFileObject = NULL;
    PDEVICE_OBJECT LowerDeviceObject = NULL;
    PDEVICE_OBJECT DisplayDeviceObject = NULL;
    PGRAPHIC_DEVICE_EXTENSION DevExt = NULL;
    CHAR DeviceNameBuf[64];
    ULONG DisplayIdx;

    if (!NotificationStructure || !NotificationStructure->SymbolicLinkName || !DriverObject)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = ObioGetDeviceObjectPointer(
        NotificationStructure->SymbolicLinkName,
        FILE_ALL_ACCESS,
        &LowerFileObject,
        &LowerDeviceObject
    );
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[GRAPHIC] Gagal membuka target interface %Z: 0x%08X\n",
                 NotificationStructure->SymbolicLinkName, Status);
        return Status;
    }

    DisplayIdx = GrpNextDisplayIndex++;
    RtlStringCbPrintfA(DeviceNameBuf, sizeof(DeviceNameBuf), "/Device/Display%lu", DisplayIdx);

    Status = ObioCreateNewDevice(
        DriverObject,
        sizeof(GRAPHIC_DEVICE_EXTENSION),
        DeviceNameBuf,
        FILE_DEVICE_VIDEO,
        &DisplayDeviceObject
    );
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[GRAPHIC] Gagal membuat %s: 0x%08X\n", DeviceNameBuf, Status);
        ObDereferenceObject(LowerFileObject);
        return Status;
    }

    DevExt = (PGRAPHIC_DEVICE_EXTENSION)DisplayDeviceObject->DeviceExtension;
    RtlZeroMemory(DevExt, sizeof(GRAPHIC_DEVICE_EXTENSION));

    DevExt->Self                 = DisplayDeviceObject;
    DevExt->PhysicalDeviceObject = LowerDeviceObject;
    DevExt->LowerFileObject      = LowerFileObject;
    DevExt->DisplayIndex         = DisplayIdx;
    DevExt->IsStarted            = TRUE;
    DevExt->IsRemoved            = FALSE;
    KsInitializeSpinlock(&DevExt->DeviceLock);

    if (NotificationStructure->SymbolicLinkName->Buffer &&
        NotificationStructure->SymbolicLinkName->Length > 0)
    {
        ULONG CopyLen = NotificationStructure->SymbolicLinkName->Length;
        if (CopyLen >= sizeof(DevExt->InterfaceName))
        {
            CopyLen = sizeof(DevExt->InterfaceName) - 1;
        }
        RtlCopyMemory(DevExt->InterfaceName,
                      NotificationStructure->SymbolicLinkName->Buffer,
                      CopyLen);
        DevExt->InterfaceName[CopyLen] = '\0';
    }

    DevExt->LowerDeviceObject = ObioAttachObject(
        DisplayDeviceObject,
        LowerDeviceObject
    );
    if (!DevExt->LowerDeviceObject)
    {
        KdPrintf("[GRAPHIC] Gagal attach %s ke device stack bawah!\n", DeviceNameBuf);
        ObioDeleteDevice(DisplayDeviceObject);
        ObDereferenceObject(LowerFileObject);
        return STATUS_NO_SUCH_DEVICE;
    }

    DisplayDeviceObject->Flags |= (DevExt->LowerDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO));
    DisplayDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    KdPrintf("[GRAPHIC] Berhasil inisialisasi %s -> LowerDevice=0x%p\n",
             DeviceNameBuf, DevExt->LowerDeviceObject);

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
GrpHandleDevInterfaceChange(
    IN PVOID NotificationStructure,
    IN PVOID Context
)
{
    PDEVICE_INTERFACE_CHANGE_NOTIFICATION NotifStruct = 
        (PDEVICE_INTERFACE_CHANGE_NOTIFICATION)NotificationStructure;
    PDRIVER_OBJECT DriverObject = (PDRIVER_OBJECT)Context;

    //
    // Is it possible that Graphic Adapter may unplugged?
    //

    if (IsEqualGUID(&NotifStruct->Event, &GUID_DEVICE_INTERFACE_ARRIVAL))
    {
        return GrpCreateNewDevice(NotifStruct, DriverObject);
    }
    else if (IsEqualGUID(&NotifStruct->Event, &GUID_DEVICE_INTERFACE_REMOVAL))
    {
        UNIMPLEMENTED_DBGBREAK;
        return STATUS_NOT_IMPLEMENTED;
    }

    return STATUS_INVALID_DEVICE_REQUEST;
}

VEASTATUS
VEAPI
DriverEntry(IN PDRIVER_OBJECT DriverObject)
{
    VEASTATUS Status;

    //
    // Register PNP Notification for type GUID GRAPHICS
    //

    Status = ObioRegisterPlugPlayNotification(
        EventCategoryDeviceInterfaceChange,
        PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
        (PVOID)&VEA_GUID_DEVINTERFACE_GRAPHIC,
        DriverObject,
        GrpHandleDevInterfaceChange,
        (PVOID)DriverObject,
        &GrpNotificationEntry
    );

    if (!VEA_SUCCESS(Status)) 
    {
        DPRINT("[GRP DRIVER] Failed to initialize: 0x%x\n", Status);
        return Status;
    }

    Status = ObioAttachInterruptToThisObject(
        DriverObject,
        GrpObIntHandler
    );

    if (!VEA_SUCCESS(Status)) 
    {
        DPRINT("[GRP DRIVER] Failed to initialize: 0x%x\n", Status);
        return Status;
    }

    return Status;
}