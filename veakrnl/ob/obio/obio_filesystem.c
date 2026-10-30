/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_filesystem.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : File System
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 14-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_filesystem.c
 * --------------------------------------------------------------------- */

LIST_ENTRY ObioFsDiskListHead, ObioFsCdRomListHead, ObioFsNetworkListHead;
ULONG ObioFsRegistrationOps;

VOID
VEAPI
ObioRegisterFileSystem(IN PDEVICE_OBJECT FsDevice)
{
    PLIST_ENTRY FsList = NULL;

    if (FsDevice->DeviceType == FILE_DEVICE_DISK_FILE_SYSTEM)
    {
        FsList = &ObioFsDiskListHead;
    }
    else if (FsDevice->DeviceType == FILE_DEVICE_NETWORK_FILE_SYSTEM)
    {
        FsList = &ObioFsNetworkListHead;
    }
    else if (FsDevice->DeviceType == FILE_DEVICE_CD_ROM_FILE_SYSTEM)
    {
        FsList = &ObioFsCdRomListHead;
    }

    if (FsList)
    {
        if (FsDevice->Flags & DO_FILESYSTEM_HIGH_PRIO)
        {
            InsertHeadList(FsList->Blink, &FsDevice->Queue.ListEntry);
        }
        else
        {
            InsertTailList(FsList, &FsDevice->Queue.ListEntry);
        }
    }

    FsDevice->Flags &= ~DO_DEVICE_INITIALIZING;

    /* TODO: Notify system */
}

PVPB
VEAPI
ObioMountInitializeVpb(
    IN PDEVICE_OBJECT DeviceObject,
    IN PDEVICE_OBJECT AttachedDeviceObject
)
{
    KIRQL OldIrql;
    PVPB Vpb;
    POBJECT_HEADER ObjHeader, ADOHeader;

    OldIrql = KsAcquireDispatcherLock();
    Vpb = DeviceObject->Vpb;

    Vpb->Flags |= VPB_MOUNTED;
    
    ObjHeader = OBJECT_TO_OBJECT_HEADER(Vpb->DeviceObject);
    ADOHeader = OBJECT_TO_OBJECT_HEADER(AttachedDeviceObject);
    
    ObjHeader->StackDepth = ADOHeader->StackDepth;
    ObjHeader->StackDepth++;    

    Vpb->ReferenceCount++;

    KsReleaseDispatcherLock(OldIrql);

    return Vpb;
}

VEASTATUS
VEAPI
ObioMountVolume(
    IN PDEVICE_OBJECT OriginalDevice,
    OUT PVPB *Vpb
)
{
    PLIST_ENTRY FsList, ListEntry;
    LIST_ENTRY LocalList;
    KEVENT Event;
    PDEVICE_OBJECT AttachedDeviceObject = OriginalDevice, FileSystemDeviceObject,
        ParentFileSystemDeviceObject;
    ULONG FsStackOverhead;
    POBJECT_HEADER ObjHeader;
    POIP Oip;
    OIP_INFORMATION IoStatusBlock;
    POIP_STACK StackPtr;
    ULONG RegistrationOps;
    VEASTATUS Status;

    if (!(OriginalDevice->Vpb->Flags & (VPB_MOUNTED)))
    {
        KsInitializeEvent(&Event, NotificationEvent, FALSE);

        OriginalDevice->Flags &= DO_VERIFY_VOLUME;
        AttachedDeviceObject = ObioGetTopDeviceObject(OriginalDevice);

        ObReferenceObject(AttachedDeviceObject);

        if ((OriginalDevice->DeviceType == FILE_DEVICE_DISK) || 
            (OriginalDevice->DeviceType == FILE_DEVICE_VIRTUAL_DISK))
        {
            FsList = &ObioFsDiskListHead;
        }
        else if (OriginalDevice->DeviceType == FILE_DEVICE_CD_ROM)
        {
            FsList = &ObioFsCdRomListHead;
        }
        
        Status = STATUS_UNSUCCESSFUL; // Maybe unsuccess
        ListEntry = FsList->Flink;

        while ((ListEntry != FsList) && !(VEA_SUCCESS(Status)))
        {
            FileSystemDeviceObject = CONTAINING_RECORD(ListEntry, DEVICE_OBJECT, Queue.ListEntry);
            ParentFileSystemDeviceObject = FileSystemDeviceObject;

            FsStackOverhead = 1;
            ObjHeader = OBJECT_TO_OBJECT_HEADER(FileSystemDeviceObject);
            while (ObjHeader->UpperAttachedObject)
            {
                FileSystemDeviceObject = ObjHeader->UpperAttachedObject;
                FsStackOverhead++;
            }

            KsClearEvent(&Event);

            ObjHeader = OBJECT_TO_OBJECT_HEADER(AttachedDeviceObject);
            Oip = ObioAllocateOip(ObjHeader->StackDepth + FsStackOverhead);

            if (!Oip)
            {
                Status = STATUS_INSUFFICIENT_MEMORY;
                break;
            }

            Oip->UserInfoBlock = &IoStatusBlock;
            Oip->UserEvent = &Event;
            Oip->Flags = OIP_MOUNT_COMPLETION | OIP_SYNCHRONOUS_PAGING_IO;
            Oip->RequestorMode = KernelMode;

            StackPtr = ObioGetNextOipStackLocation(Oip);
            StackPtr->MajorFunction = OIP_MJ_FILE_SYSTEM_CONTROL;
            StackPtr->MinorFunction = OIP_MN_MOUNT_VOLUME;
            StackPtr->Flags = 0; /* TODO: Learn about Raw Mount */
            StackPtr->Parameters.MountVolume.Vpb = OriginalDevice->Vpb;
            StackPtr->Parameters.MountVolume.DeviceObject = AttachedDeviceObject;

            RegistrationOps = ObioFsRegistrationOps;

            Status = ObioGoInterruptObject(FileSystemDeviceObject, Oip);
            if (Status == STATUS_PENDING)
            {
                KsWaitForSingleObject(&Event);
                Status = IoStatusBlock.Status;
            }

            if (VEA_SUCCESS(Status))
            {
                DPRINT("Following FS %p successfullly mounted to %p\n", FileSystemDeviceObject, AttachedDeviceObject);
                *Vpb = ObioMountInitializeVpb(OriginalDevice, AttachedDeviceObject);
            }
            else
            {
                if (RegistrationOps != ObioFsRegistrationOps)
                {
                    LocalList.Flink = FsList->Flink;
                    ListEntry = &LocalList;

                    DPRINT("FS Registration ops changed while Mounting FS attempt\n");
                    Status = STATUS_UNRECOGNIZED_VOLUME;
                }

                DPRINT("Failed attempt to mount FS: 0x%x\n", Status);
            }

            ListEntry = ListEntry->Flink;
        }

        if (!VEA_SUCCESS(Status))
        {
            ObDereferenceObject(AttachedDeviceObject);
            DPRINT("Failed to mount all FS: 0x%x\n", Status);
        }
    }
    else if (OriginalDevice->Vpb->Flags & VPB_REMOVE_PENDING)
    {
        Status = STATUS_DEVICE_DOES_NOT_EXIST;
    }
    else
    {
        Status = STATUS_SUCCESS;
    }

    if (!(VEA_SUCCESS(Status)) && 
        (OriginalDevice->Flags & DO_SYSTEM_BOOT_PARTITION) && 
        UlpInitializationPhase < 2)
    {
        KsBugCheckEx(
            BOOT_DEVICE_HANDLE_MISSING,
            (ULONG_PTR)OriginalDevice,
            Status,
            0,
            0
        );
    }

    return Status;
}