/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : process.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Processing PnP Notification for Pm
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "veapm.h"

/* Revision History ------------------------------------------------------
 * DATE       : 10-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file process.c
 * --------------------------------------------------------------------- */

PVOID PmNotificationEntry = NULL;

VEASTATUS
VEAPI
PmpScanAndCreatePartitions(IN PPM_DEVICE_EXTENSION DevExt)
{
    VEASTATUS Status;
    PVOID Buffer;

    Status = PmpAskHarddiskNumber(DevExt);
    if (!VEA_SUCCESS(Status))
    {
        PM_ERROR("Failed to query harddisk number: 0x%x\n", Status);
        return Status;
    }

    Status = PmpAskDiskGeometry(DevExt);
    if (!VEA_SUCCESS(Status)) {
        PM_ERROR("Failed to query disk geometry: 0x%x\n", Status);
        return Status;
    }

    Buffer = UlAllocatePoolZero(NonPagedPool, DevExt->BytesPerSector, TAG_OBIO);
    if (!Buffer) {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    /* Read sectors */
    Status = PmpReadSectors(DevExt, 0, 1, Buffer);
    if (!VEA_SUCCESS(Status))
    {
        PM_ERROR("Read error: 0x%x\n", Status);
        UlFreePoolWithTag(Buffer, TAG_OBIO);
        return Status;
    }

    Status = PmpDetectAndCreatePartition(DevExt, Buffer);
    if (!VEA_SUCCESS(Status))
    {
        PM_ERROR("Failed to create partition: 0x%x\n", Status);
        UlFreePoolWithTag(Buffer, TAG_OBIO);
        return Status;
    }

    UlFreePoolWithTag(Buffer, TAG_OBIO);
    return STATUS_SUCCESS;
}

VOID
VEAPI
PmpWorkItem(IN PDEVICE_OBJECT FilterDevice, IN PVOID Context)
{
    PPM_DEVICE_EXTENSION DevExt = (PPM_DEVICE_EXTENSION)FilterDevice->DeviceExtension;
    PIO_WORKITEM WorkItem = (PIO_WORKITEM)Context;
    VEASTATUS Status;

    PM_INFO("Executing PmWorkItem for FilterDevice %p...\n", FilterDevice);

    Status = PmpScanAndCreatePartitions(DevExt);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("PmpScanAndCreatePartitions() failed: 0x%x\n", Status);
        goto Cleanup;
    }

Cleanup:
    if (WorkItem)
    {
        ObioFreeWorkItem(WorkItem);
    }
}

VEASTATUS
VEAPI
PmHandleArrivalNotification(
    IN PVOID NotificationStructure,
    IN PVOID Context
)
{
    VEASTATUS Status;
    PDEVICE_INTERFACE_CHANGE_NOTIFICATION Notification = NotificationStructure;
    PDRIVER_OBJECT DriverObject = Context;
    PPM_DEVICE_EXTENSION DevExt;
    PDEVICE_OBJECT FilterDevice = NULL;
    PDEVICE_OBJECT TargetDevice = NULL;
    PIO_WORKITEM WorkItem = NULL;

    PM_INFO("Received new GUID_DEVICE_INTERFACE_ARRIVAL\n");

    Status = ObioCreateNewDevice(  
        DriverObject,  
        sizeof(PM_DEVICE_EXTENSION),  
        NULL,  
        FILE_DEVICE_DISK,  
        &FilterDevice  
    );

    if (!VEA_SUCCESS(Status))  
    {  
        PM_ERROR("Failed to create FilterDevice\n");  
        return Status;  
    }

    DevExt = (PPM_DEVICE_EXTENSION)FilterDevice->DeviceExtension;  
    RtlZeroMemory(DevExt, sizeof(PM_DEVICE_EXTENSION));

    Status = ObioReferenceDeviceObjectByName(  
        Notification->SymbolicLinkName,  
        &TargetDevice  
    );  
    if (!VEA_SUCCESS(Status))  
    {  
        PM_ERROR("Failed to get DeviceObject to attach: 0x%x\n", Status);  
        ObioDeleteDevice(FilterDevice);  
        return Status;  
    }

    DevExt->LowerDeviceObject = ObioAttachObject(FilterDevice, TargetDevice);  
    if (!DevExt->LowerDeviceObject)  
    {  
        PM_ERROR("Failed to attach object to device\n");  
        ObDereferenceObject(TargetDevice); /* FIX 1: Cleanup TargetDevice */
        ObioDeleteDevice(FilterDevice);  
        return STATUS_OBJECT_ATTACH_FAIL;  
    }

    DevExt->PhysicalDeviceObject = TargetDevice;  
    DevExt->DeviceType = PmDeviceTypeDiskFilter;  
    DevExt->SelfDeviceObject = FilterDevice;   
    DevExt->HarddiskNum = 0;
    DevExt->PnpState = PmPnpStateNotStarted;

    InitializeListHead(&DevExt->PartitionListHead);
    KsInitializeSpinlock(&DevExt->PartitionListLock);
    DevExt->PartitionCount = 0;
    FilterDevice->Flags |= DevExt->LowerDeviceObject->Flags & (DO_DIRECT_IO | DO_BUFFERED_IO);

    ObDereferenceObject(TargetDevice);

    FilterDevice->Flags &= ~DO_DEVICE_INITIALIZING;

    /* Alokasi Work Item */
    WorkItem = ObioAllocateWorkItem(FilterDevice);  
    if (!WorkItem)  
    {  
        PM_ERROR("WorkItem allocation failed\n");  
        /* FIX 2: Cleanup FilterDevice kalau alokasi WorkItem gagal */
        ObioDeleteDevice(FilterDevice);
        return STATUS_INSUFFICIENT_MEMORY;  
    }

    /* FIX 3: Kirim WorkItem sebagai Context & Pake DelayedWorkQueue */
    ObioQueueWorkItem(WorkItem, PmpWorkItem, DelayedWorkQueue, WorkItem);

    return STATUS_SUCCESS;  
}

VEASTATUS
VEAPI
PmHandleRemovalNotification(
    IN PVOID NotificationStructure,
    IN PVOID Context
)
{
    UNIMPLEMENTED_DBGBREAK
    return STATUS_NOT_IMPLEMENTED;
}

VEASTATUS
VEAPI
PmPnpNotificationCallback(
    IN PVOID NotificationStructure,
    IN PVOID Context
)
{
    PDEVICE_INTERFACE_CHANGE_NOTIFICATION Notification = NotificationStructure;

    if (IsEqualGUID(&Notification->Event, &GUID_DEVICE_INTERFACE_ARRIVAL))
    {
        return PmHandleArrivalNotification(Notification, Context);
    }
    else if (IsEqualGUID(&Notification->Event, &GUID_DEVICE_INTERFACE_REMOVAL))
    {
        return PmHandleRemovalNotification(Notification, Context);
    }
    else
    {
        return STATUS_NOT_SUPPORTED;
    }
}