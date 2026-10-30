/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_device.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Device handling
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 14-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_device.c
 * --------------------------------------------------------------------- */

PDEVICE_OBJECT
VEAPI
ObioGetRelatedDeviceObject(
    IN PFILE_OBJECT FileObject
)
{
    PDEVICE_OBJECT DeviceObject;
    PDEVICE_OBJECT TopDeviceObject;

    if (!FileObject || !FileObject->DeviceObject)
    {
        return NULL;
    }

    DeviceObject = FileObject->DeviceObject;
    if (FileObject->Vpb && FileObject->Vpb->DeviceObject)
    {
        DeviceObject = FileObject->Vpb->DeviceObject;
    }

    TopDeviceObject = ObioGetTopDeviceObject(DeviceObject);
    if (!TopDeviceObject)
    {
        return NULL;
    }

    /* Match IoGetRelatedDeviceObject's borrowed-pointer return semantics. */
    ObDereferenceObject(TopDeviceObject);
    return TopDeviceObject;
}

PVPB
VEAPI
ObioAllocateVpb(IN PDEVICE_OBJECT DeviceObject)
{
    PVPB Vpb;

    Vpb = UlAllocatePoolWithTag(NonPagedPool, sizeof(VPB), 'bpvV');
    if (!Vpb)
    {
        return NULL;
    }

    RtlZeroMemory(Vpb, sizeof(VPB));
    Vpb->Type = IO_TYPE_VPB;
    Vpb->Size = sizeof(VPB);
    Vpb->Flags = 0;
    Vpb->RealDevice = DeviceObject;

    return Vpb;
}

PVPB
VEAPI
ObioCheckVpbMounted(
    IN POPEN_PACKET OpenPacket,
    IN PDEVICE_OBJECT OriginalDevice,
    IN PANSI_STRING RemainingName,
    OUT PVEASTATUS Status
)
{
    PVPB Vpb = OriginalDevice->Vpb;

    if (!Vpb)
    {
        Vpb = ObioAllocateVpb(OriginalDevice);
        if (!Vpb)
        {
            *Status = STATUS_INSUFFICIENT_MEMORY;
            return NULL;
        }

        OriginalDevice->Vpb = Vpb;
    }

    while (!(Vpb->Flags & VPB_MOUNTED))
    {
        // Trigger pencarian FSD (vefat.ko) buat mount partisi ini
        *Status = ObioMountVolume(OriginalDevice, &Vpb);
        if (!VEA_SUCCESS(*Status))
        {
            return NULL;
        }
    }

    if (Vpb->Flags & VPB_LOCKED)
    {
        *Status = STATUS_ACCESS_DENIED;
        return NULL;
    }

    Vpb->ReferenceCount++;
    *Status = STATUS_SUCCESS;
    return Vpb;
}

VEASTATUS
VEAPI
ObioParseDevice(
    IN PVOID ParseObject,
    IN POB_LOOKUP_CONTEXT LookupContext,
    IN PVOID Context,
    OUT PVOID *ResolvedObject
)
{
    POPEN_PACKET OpenPacket = (POPEN_PACKET)Context;
    PDEVICE_OBJECT OriginalDevice = (PDEVICE_OBJECT)ParseObject;
    PDEVICE_OBJECT TargetDevice = OriginalDevice;
    PFILE_OBJECT FileObject = NULL;
    PVPB Vpb = NULL;
    POIP Oip = NULL;
    POIP_STACK StackLoc = NULL;
    PANSI_STRING RemainingName = &LookupContext->RemainingName;
    POBJECT_HEADER ObjHeader;
    OBJECT_ATTRIBUTES ObjAttributes;
    ULONG Attributes = LookupContext->Attributes;
    VEASTATUS Status;

    /* Theres so much TODO here. In here, it's just a main logic of the code
    
       VeaOS may not 100% adopt the ReactOS code. the NT inspired may take 
       some other path of logic and code so dont worry if the code is
       different than the ReactOS one :) */
    
    /* Assume failure here */
    *ResolvedObject = NULL;

    if (OpenPacket == NULL)
    {
        *ResolvedObject = OriginalDevice;
        return STATUS_SUCCESS;
    }

    if (OriginalDevice->Vpb)
    {
        Vpb = ObioCheckVpbMounted(OpenPacket, OriginalDevice, RemainingName, &Status);
        if (!Vpb || !VEA_SUCCESS(Status))
        {
            return Status;
        }

        TargetDevice = Vpb->DeviceObject;
    }

    ObjHeader = OBJECT_TO_OBJECT_HEADER(TargetDevice);
    
    if (ObjHeader->UpperAttachedObject)
    {
        TargetDevice = ObioGetTopDeviceObject(TargetDevice);
    }

    InitializeObjectAttributes(&ObjAttributes, NULL, Attributes, 0, 0);

    Status = ObCreateObject(
        KernelMode,
        ObpFileObjectType,
        &ObjAttributes,
        LookupContext->AccessMode,
        NULL,
        sizeof(FILE_OBJECT),
        0,
        0,
        (PVOID*)&FileObject
    );
    if (!VEA_SUCCESS(Status)) return Status;

    RtlZeroMemory(FileObject, sizeof(FILE_OBJECT));
    FileObject->Type = IO_TYPE_FILE;
    FileObject->Size = sizeof(FILE_OBJECT);
    FileObject->DeviceObject = OriginalDevice;
    FileObject->Vpb = Vpb;

    if (RemainingName->Length > 0)
    {
        FileObject->FileName.MaximumLength = RemainingName->Length + sizeof(CHAR);
        FileObject->FileName.Buffer = UlAllocatePoolWithTag(
            PagedPool, 
            FileObject->FileName.MaximumLength, 
            'maNI'
        );

        if (!FileObject->FileName.Buffer)
        {
            ObDereferenceObject(FileObject);
            return STATUS_INSUFFICIENT_MEMORY;
        }

        RtlCopyString(&FileObject->FileName, RemainingName);
    }

    KsInitializeEvent(&FileObject->Event, NotificationEvent, FALSE);

    Oip = ObioAllocateOip(ObjHeader->StackDepth);
    if (!Oip)
    {
        if (FileObject->FileName.Buffer) UlFreePoolWithTag(FileObject->FileName.Buffer, 'maNI');
        ObDereferenceObject(FileObject);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    StackLoc = ObioGetNextOipStackLocation(Oip);
    StackLoc->MajorFunction = OIP_MJ_CREATE;
    StackLoc->FileObject = FileObject;
    StackLoc->Parameters.Create.Options = OpenPacket->CreateOptions;
    StackLoc->Parameters.Create.ShareAccess = OpenPacket->ShareAccess;
    StackLoc->Parameters.Create.EaBuffer = OpenPacket->EaBuffer;
    StackLoc->Parameters.Create.EaLength = OpenPacket->EaLength;

    Oip->RequestorMode = LookupContext->AccessMode;
    Oip->OriginalFileObject = FileObject;

    Status = ObioGoInterruptObject(TargetDevice, Oip);
    if (Status == STATUS_PENDING)
    {
        KsWaitForSingleObject(&FileObject->Event);
        Status = Oip->StatusInformation.Status;
    }

    ObioFreeOip(Oip);

    if (VEA_SUCCESS(Status))
    {
        *ResolvedObject = FileObject;
        OpenPacket->FileObject = FileObject;
        OpenPacket->FinalStatus = Status;
    }
    else
    {
        if (FileObject->FileName.Buffer) UlFreePoolWithTag(FileObject->FileName.Buffer, 'maNI');
        ObDereferenceObject(FileObject);
    }

    return Status;
}