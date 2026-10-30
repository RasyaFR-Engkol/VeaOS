/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : oip.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : OIP Way
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "ahciheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 25-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file oip.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
AhciAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT TargetDevice
)
{
    VEASTATUS Status;
    PDEVICE_OBJECT Fdo;
    PAHCI_DEVICE_EXTENSION DevExt;

    Status = ObioCreateNewDevice(
        DriverObject,
        sizeof(AHCI_DEVICE_EXTENSION),
        NULL,
        FILE_DEVICE_CONTROLLER,
        &Fdo
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("AHCI: Failed to create new device: 0x%x\n", Status);
        return Status;
    }

    DevExt = (PAHCI_DEVICE_EXTENSION)Fdo->DeviceExtension;
    DevExt->FunctionalDeviceObject = Fdo;
    DevExt->PhysicalDeviceObject = TargetDevice;
    DevExt->NextDeviceObject = ObioAttachObject(Fdo, TargetDevice);
    InitializeListHead(&DevExt->ChildPdoListHead);
    KsInitializeSpinlock(&DevExt->ChildPdoListLock);
    DevExt->ChildPdoCount = 0;
    if (!DevExt->NextDeviceObject)
    {
        DPRINT("AHCI: Failed to attach object\n");
        ObioDeleteDevice(Fdo);
        return STATUS_OBJECT_ATTACH_FAIL;
    }
    
    KsInitializeSpinlock(&DevExt->ControllerLock);

    Fdo->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
AhciStartDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PAHCI_DEVICE_EXTENSION DevExt = (PAHCI_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    POBIO_RESOURCE_LIST ReqResource = NULL;
    PHYSICAL_ADDRESS AbarAddress;
    ULONG AbarLength;
    ULONG Vector;
    OIP_INFORMATION Information;
    PKINTERRUPT IntrObject;
    AHCI_INTERRUPT_INFO InterruptInfo;
    BUS_INTERFACE_STANDARD BusInterface;
    VEASTATUS Status;

    /* Ask PCI about resources */
    DPRINT("Requesting to NextDeviceObject\n");
    Status = AhciCallLowerDeviceOip(
        DevExt->NextDeviceObject,
        OIP_MJ_PNP,
        OIP_MN_QUERY_RESOURCES,
        NULL,
        &Information
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("AHCI: Failed to get Required Resources: 0x%x\n", Status);
        Oip->StatusInformation.Status = Status;
        return Status;
    }

    /* Set Pointer */
    ReqResource = (POBIO_RESOURCE_LIST)Information.Information;

    /* Get ABAR addres from Resource List */
    AbarAddress = AhciGetAbarPhysicalAddress(ReqResource, &AbarLength);
    if (!AbarAddress.QuadPart)
    {
        DPRINT("AHCI: Failed to get ABAR\n");
        Oip->StatusInformation.Status = STATUS_IO_ALLOCATE_FAILED;
        return STATUS_IO_ALLOCATE_FAILED;
    }

    /* Set needed DATA */
    DevExt->HbaPhysicalAddress = AbarAddress;
    DevExt->HbaLength = AbarLength;
    DevExt->HbaRegisters = (PAHCI_HBA_REGISTERS)MmMapIoSpace(
        AbarAddress.QuadPart,
        AbarLength,
        MmNonCached
    );

    /* Is fatal to failed mapping HbaRegisters */
    if (!DevExt->HbaRegisters)
    {
        DPRINT("AHCI: Failed to map ABAR MMIO space\n");
        Oip->StatusInformation.Status = STATUS_INSUFFICIENT_MEMORY;
        return STATUS_INSUFFICIENT_MEMORY;
    }

    /* We need to get Bus Interface Standard here */
    Status = AhciGetBusInterface(
        DevExt,
        (PGUID)&VEA_BUS_INTERFACE_STANDARD,
        &BusInterface,
        1,
        sizeof(BusInterface)
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("AHCI: Failed to extract Interrupt Info\n");
        MmUnmapIoSpace((PVOID)DevExt->HbaRegisters, AbarLength);
        Oip->StatusInformation.Status = STATUS_RESOURCE_DATA_NOT_FOUND;
        return STATUS_RESOURCE_DATA_NOT_FOUND;
    }

    /* Set to Write/Read PCI */
    DevExt->BusInterface.GetBusData = BusInterface.GetBusData;
    DevExt->BusInterface.SetBusData = BusInterface.SetBusData;
    DevExt->BusInterface.DereferenceInterface = BusInterface.InterfaceDereference;
    DevExt->BusInterface.ReferenceInterface = BusInterface.InterfaceReference;
    DevExt->BusInterface.Context = BusInterface.Context;

    /* Also get the interrupt info here */
    if (!AhciGetInterruptInfo(ReqResource, &InterruptInfo))
    {
        DPRINT("AHCI: Failed to extract Interrupt Info\n");
        MmUnmapIoSpace((PVOID)DevExt->HbaRegisters, AbarLength);
        Oip->StatusInformation.Status = STATUS_RESOURCE_DATA_NOT_FOUND;
        return STATUS_RESOURCE_DATA_NOT_FOUND;
    }

    /* Check if we have InterruptModeMessage */
    if (InterruptInfo.Mode == AhciInterruptModeMessage)
    {
        KINTERRUPT_MESSAGE MsgInfo;

        /* Try to connect it */
        Status = ObioConnectMessageInterrupt(
            &DevExt->InterruptObject,
            &MsgInfo,
            AhciIsrRoutine,
            (PVOID)DevExt,
            &DevExt->ControllerLock,
            AHCI_DEVICE_DIRQL,
            AHCI_DEVICE_DIRQL,
            0
        );

        if (VEA_SUCCESS(Status))
        {
            /* Enable MSI Capability */
            Status = AhciEnableMsiCapability(&BusInterface, InterruptInfo.CapOffset, &MsgInfo);
            if (!VEA_SUCCESS(Status))
            {
                /* Hell nawh */
                DPRINT("AHCI: Failed to enable MSI capability: 0x%x\n", Status);
            }
        }
    }
    /* or else, fallback */
    else
    {
        /* Try to connect to IRQ Line */
        Status = ObioConnectInterrupt(
            &DevExt->InterruptObject,
            AhciIsrRoutine,
            (PVOID)DevExt,
            &DevExt->ControllerLock,
            InterruptInfo.Vector,
            DISPATCH_LEVEL,
            DISPATCH_LEVEL,
            Message,
            0
        );

        if (VEA_SUCCESS(Status))
        {
            Status = AhciEnableLegacyInterrupt(&BusInterface);
            if (!VEA_SUCCESS(Status))
            {
                DPRINT("AHCI: Failed to connect to INTx line: 0x%x\n", Status);
            }
        }
    }

    /* What if we failed to connect interrupt? */
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("AHCI: Failed to connect interrupt: 0x%x\n", Status);
        MmUnmapIoSpace((PVOID)DevExt->HbaRegisters, AbarLength);
        Oip->StatusInformation.Status = Status;
        return Status;
    }

    /* We should initialize the ports now */
    Status = AhciStartHbaEngine(
        DevExt
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("AHCI: Failed to start and initialize: 0x%x\n", Status);
        MmUnmapIoSpace((PVOID)DevExt->HbaRegisters, AbarLength);
        Oip->StatusInformation.Status = Status;
        return Status;
    }

    Oip->StatusInformation.Status = STATUS_SUCCESS;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
AhciQueryDeviceRelations(
    IN PAHCI_DEVICE_EXTENSION DevExt,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PDEVICE_RELATIONS DeviceRelations;
    ULONG Size;
    KIRQL OldIrql;
    PLIST_ENTRY Entry;
    ULONG ChildCount, Index;

    if (OipStack->Parameters.QueryDeviceRelation.Type != BusRelations)
    {
        ObioSkipCurrentOipStackLocation(Oip);
        return ObioCallObject(DevExt->NextDeviceObject, Oip);
    }

    KsAcquireSpinLock(&DevExt->ChildPdoListLock, &OldIrql);

    ChildCount = DevExt->ChildPdoCount;

    Size = sizeof(DEVICE_RELATIONS) + (ChildCount > 0 ? (ChildCount - 1) * sizeof(PDEVICE_OBJECT) : 0);
    DeviceRelations = (PDEVICE_RELATIONS)UlAllocatePoolZero(NonPagedPool, Size, TAG_OBIO);

    if (!DeviceRelations)
    {
        KsReleaseSpinLock(&DevExt->ChildPdoListLock, OldIrql);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    DeviceRelations->Count = ChildCount;

    Index = 0;
    Entry = DevExt->ChildPdoListHead.Flink;

    while (Entry != &DevExt->ChildPdoListHead && Index < ChildCount)
    {
        PAHCI_PORT_CONTEXT PortCtx = CONTAINING_RECORD(Entry, AHCI_PORT_CONTEXT, ChildListEntry);
        
        if (PortCtx->DiskPdo)
        {
            DeviceRelations->Objects[Index] = PortCtx->DiskPdo;
            
            ObReferenceObject(PortCtx->DiskPdo);
            Index++;
        }

        Entry = Entry->Flink;
    }

    KsReleaseSpinLock(&DevExt->ChildPdoListLock, OldIrql);

    DPRINT("AHCI: Returned %u Child PDOs for BusRelations Query\n", DeviceRelations->Count);

    Oip->StatusInformation.Information = (ULONG_PTR)DeviceRelations;
    Oip->StatusInformation.Status = STATUS_SUCCESS;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
AhciQueryPortId(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN POIP Oip,
    IN BUS_QUERY_ID_TYPE IdType
)
{
    CHAR IdBuffer[24];
    PCHAR ResultBuffer;
    ULONG BufferLength;
    VEASTATUS Status;

    if (!PortCtx || !PortCtx->DiskPdo || !Oip)
    {
        return STATUS_INVALID_PARAMETER;
    }

    switch (IdType)
    {
        case BusQueryDeviceID:
            Status = RtlStringCchCopyA(
                IdBuffer,
                sizeof(IdBuffer),
                "AHCI/DISK"
            );
            break;

        case BusQueryInstanceID:
            Status = RtlStringCbPrintfA(
                IdBuffer,
                sizeof(IdBuffer),
                "%u",
                PortCtx->PortNumber
            );
            break;

        default:
            Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
            Oip->StatusInformation.Information = 0;
            return STATUS_NOT_SUPPORTED;
    }

    if (!VEA_SUCCESS(Status))
    {
        Oip->StatusInformation.Status = Status;
        Oip->StatusInformation.Information = 0;
        return Status;
    }

    BufferLength = RtlRawStringLength(IdBuffer) + 1;
    ResultBuffer = (PCHAR)UlAllocatePoolZero(PagedPool, BufferLength, TAG_OBIO);
    if (!ResultBuffer)
    {
        Oip->StatusInformation.Status = STATUS_INSUFFICIENT_MEMORY;
        Oip->StatusInformation.Information = 0;
        return STATUS_INSUFFICIENT_MEMORY;
    }

    RtlCopyMemory(ResultBuffer, IdBuffer, BufferLength);
    Oip->StatusInformation.Status = STATUS_SUCCESS;
    Oip->StatusInformation.Information = (ULONG_PTR)ResultBuffer;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
AhciMjPnp(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    PAHCI_PORT_CONTEXT PortCtx = *(PAHCI_PORT_CONTEXT*)DeviceObject->DeviceExtension;

    /* Child Handling PDO */
    if (PortCtx && PortCtx->DiskPdo == DeviceObject)
    {
        switch (OipStack->MinorFunction)
        {
            case OIP_MN_QUERY_ID:
                return AhciQueryPortId(
                    PortCtx,
                    Oip,
                    OipStack->Parameters.QueryId.IdType
                );

            case OIP_MN_START_DEVICE:
                Oip->StatusInformation.Status = STATUS_SUCCESS;
                return STATUS_SUCCESS;

            default:
                break;
        }

        Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
        return STATUS_NOT_SUPPORTED;
    }

    /* FDO Handling */
    PAHCI_DEVICE_EXTENSION DevExt = (PAHCI_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    switch (OipStack->MinorFunction)
    {
        case OIP_MN_START_DEVICE:
            return AhciStartDevice(DeviceObject, Oip, OipStack);

        case OIP_MN_QUERY_DEVICE_RELATIONS:
            return AhciQueryDeviceRelations(
                DevExt,
                Oip,
                OipStack
            );

        default:
            break;
    }

    Oip->StatusInformation.Status = STATUS_INVALID_DEVICE_REQUEST;
    return STATUS_INVALID_DEVICE_REQUEST;
}

VEASTATUS
VEAPI
AhciMjDeviceControl(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    PAHCI_PORT_CONTEXT PortCtx;
    VEASTATUS Status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG Information = 0;
    ULONG InputLength  = OipStack->Parameters.DeviceControl.InputBufferLength;
    ULONG OutputLength = OipStack->Parameters.DeviceControl.OutputBufferLength;
    ULONG IoControlCode = OipStack->Parameters.DeviceControl.IoControlCode;

    if (!DeviceObject || !DeviceObject->DeviceExtension || !Oip)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        Oip->StatusInformation.Information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    PortCtx = *(PAHCI_PORT_CONTEXT*)DeviceObject->DeviceExtension;

    /* Work on IOCTL now */
    switch (IoControlCode)
    {
        /* 
        
            ATA PASS THROUGH IOCTL
        
        */
        case IOCTL_ATA_PASS_THROUGH:
        {
            PATA_PASS_THROUGH_EX AtaPt;
            PVOID DataBuffer = NULL;

            if (InputLength < sizeof(ATA_PASS_THROUGH_EX) || OutputLength < sizeof(ATA_PASS_THROUGH_EX))
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            AtaPt = (PATA_PASS_THROUGH_EX)Oip->AssociatedOip.SystemBuffer;
            if (!AtaPt)
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            if (AtaPt->DataTransferLength > 0)
            {
                if (AtaPt->DataBufferOffset < sizeof(ATA_PASS_THROUGH_EX) ||
                    (AtaPt->DataBufferOffset + AtaPt->DataTransferLength) > InputLength)
                {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                DataBuffer = (PVOID)((ULONG_PTR)AtaPt + AtaPt->DataBufferOffset);
            }

            Status = AhciExecutePassThroughCommand(
                PortCtx->DevExt,
                PortCtx,
                AtaPt,
                DataBuffer
            );

            if (VEA_SUCCESS(Status))
            {
                Information = OutputLength;
            }
            break;
        }

        /*
        
            GET LENGTH INFO IOCTL
        
        */
        case IOCTL_DISK_GET_LENGTH_INFO:
        {
            if (OutputLength < sizeof(GET_LENGTH_INFORMATION))
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            PGET_LENGTH_INFORMATION LengthInfo = (PGET_LENGTH_INFORMATION)Oip->AssociatedOip.SystemBuffer;
            LengthInfo->Length.QuadPart = PortCtx->TotalSectors * PortCtx->SectorSize;

            Information = sizeof(GET_LENGTH_INFORMATION);
            Status = STATUS_SUCCESS;
            break;
        }

        /* 
        
            FLUSH DISK IOCTL
        
        */
        case IOCTL_DISK_FLUSH_CACHE:
        {
            /* Kirim ATA Command 0xEA (FLUSH CACHE EXT) via Command Slot */
            Status = AhciSendFlushCache(PortCtx->DevExt, PortCtx);
            Information = 0;
            break;
        }

        /*
        
            STORAGE QUERY PROPERTY IOCTL
        
        */
        case IOCTL_STORAGE_QUERY_PROPERTY:
        {
            PSTORAGE_PROPERTY_QUERY Query = (PSTORAGE_PROPERTY_QUERY)Oip->AssociatedOip.SystemBuffer;

            if (InputLength < sizeof(STORAGE_PROPERTY_QUERY))
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            if (Query->PropertyId == StorageDeviceProperty && Query->QueryType == PropertyStandardQuery)
            {
                if (OutputLength < sizeof(STORAGE_DEVICE_DESCRIPTOR))
                {
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                PSTORAGE_DEVICE_DESCRIPTOR Desc = (PSTORAGE_DEVICE_DESCRIPTOR)Oip->AssociatedOip.SystemBuffer;
                RtlZeroMemory(Desc, sizeof(STORAGE_DEVICE_DESCRIPTOR));

                Desc->Version = sizeof(STORAGE_DEVICE_DESCRIPTOR);
                Desc->Size = sizeof(STORAGE_DEVICE_DESCRIPTOR);
                Desc->DeviceType = FILE_DEVICE_DISK;
                Desc->RemovableMedia = FALSE;
                Desc->CommandQueueing = TRUE; // HBA AHCI Support NCQ
                Desc->BusType = BusTypeSata;  // 0x0B (SATA)

                Information = sizeof(STORAGE_DEVICE_DESCRIPTOR);
                Status = STATUS_SUCCESS;
            }
            else
            {
                Status = STATUS_NOT_SUPPORTED;
            }
            break;
        }

        /*
        
            STORAGE CHECK VERIFY IOCTL
        
        */
        case IOCTL_STORAGE_CHECK_VERIFY:
        {
            ULONG Ssts = PortCtx->Registers->Ssts;
            ULONG Det = Ssts & AHCI_PxSSTS_DET_MASK;

            if (Det == AHCI_PxSSTS_DET_PRESENT_PHY)
            {
                Status = STATUS_SUCCESS;
            }
            else
            {
                Status = STATUS_NO_SUCH_DEVICE;
            }
            Information = 0;
            break;
        }

        /*
        
            DISK GET DRIVER GEOMETRY IOCTL
        
        */
        case IOCTL_DISK_GET_DRIVE_GEOMETRY:
        {
            PDISK_GEOMETRY Geometry;

            if (OutputLength < sizeof(DISK_GEOMETRY))
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            Geometry = (PDISK_GEOMETRY)Oip->AssociatedOip.SystemBuffer;
            RtlZeroMemory(Geometry, sizeof(DISK_GEOMETRY));

            Geometry->BytesPerSector = (PortCtx->SectorSize > 0) ? PortCtx->SectorSize : 512;
            Geometry->MediaType = FixedMedia;
            
            /* Mapping C/H/S sintetis dari total LBA48 sectors */
            Geometry->SectorsPerTrack = 63;
            Geometry->TracksPerCylinder = 255;
            if (PortCtx->TotalSectors > 0)
            {
                Geometry->Cylinders.QuadPart = PortCtx->TotalSectors / (63 * 255);
            }

            Information = sizeof(DISK_GEOMETRY);
            Status = STATUS_SUCCESS;
            break;
        }

        /* 
        
            STORAGE GET DEVICE NUMBER IOCTL
        
        */
        case IOCTL_STORAGE_GET_DEVICE_NUMBER:
        {
            PSTORAGE_DEVICE_NUMBER DevNum;

            if (OutputLength < sizeof(STORAGE_DEVICE_NUMBER))
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            DevNum = (PSTORAGE_DEVICE_NUMBER)Oip->AssociatedOip.SystemBuffer;
            DevNum->DeviceType = FILE_DEVICE_DISK;    // 0x00000007
            DevNum->DeviceNumber = PortCtx->PortNumber; // 0 buat Port 0, 1 buat Port 1
            DevNum->PartitionNumber = 0;              // 0 = Physical Raw PDO

            Information = sizeof(STORAGE_DEVICE_NUMBER);
            Status = STATUS_SUCCESS;
            break;
        }

        /* 
        
            UNKNOWN IOCTL
        
        */
        default:    
            Status = STATUS_NOT_SUPPORTED;
            break;
    }

    Oip->StatusInformation.Status = Status;
    Oip->StatusInformation.Information = Information;
    return Status;
}

VEASTATUS
VEAPI
AhciPdoDeviceControl(
    IN PAHCI_PORT_CONTEXT PortCtx,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    VEASTATUS Status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG Information = 0;
    ULONG InputLength   = OipStack->Parameters.DeviceControl.InputBufferLength;
    ULONG OutputLength  = OipStack->Parameters.DeviceControl.OutputBufferLength;
    ULONG IoControlCode = OipStack->Parameters.DeviceControl.IoControlCode;
    PVOID SystemBuffer  = Oip->AssociatedOip.SystemBuffer;

    if (!PortCtx || !PortCtx->IsActive || !Oip)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        Oip->StatusInformation.Information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    switch (IoControlCode)
    {
        /* 
         * 1. ATA PASS THROUGH IOCTL
         */
        case IOCTL_ATA_PASS_THROUGH:
        {
            PATA_PASS_THROUGH_EX AtaPt = (PATA_PASS_THROUGH_EX)SystemBuffer;
            PVOID DataBuffer = NULL;

            if (InputLength < sizeof(ATA_PASS_THROUGH_EX) || OutputLength < sizeof(ATA_PASS_THROUGH_EX) || !AtaPt)
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            if (AtaPt->DataTransferLength > 0)
            {
                if (AtaPt->DataBufferOffset < sizeof(ATA_PASS_THROUGH_EX) ||
                    (AtaPt->DataBufferOffset + AtaPt->DataTransferLength) > InputLength)
                {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                DataBuffer = (PVOID)((ULONG_PTR)AtaPt + AtaPt->DataBufferOffset);
            }

            Status = AhciExecutePassThroughCommand(
                PortCtx->DevExt,
                PortCtx,
                AtaPt,
                DataBuffer
            );

            if (VEA_SUCCESS(Status))
            {
                Information = OutputLength;
            }
            break;
        }

        /* 
         * 2. GET LENGTH INFO IOCTL (Dipakai diska.sys / veapartmgr)
         */
        case IOCTL_DISK_GET_LENGTH_INFO:
        {
            if (OutputLength < sizeof(GET_LENGTH_INFORMATION) || !SystemBuffer)
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            PGET_LENGTH_INFORMATION LengthInfo = (PGET_LENGTH_INFORMATION)SystemBuffer;
            LengthInfo->Length.QuadPart = PortCtx->TotalSectors * PortCtx->SectorSize;

            Information = sizeof(GET_LENGTH_INFORMATION);
            Status = STATUS_SUCCESS;
            break;
        }

        /* 
         * 3. FLUSH CACHE IOCTL
         */
        case IOCTL_DISK_FLUSH_CACHE:
        {
            Status = AhciSendFlushCache(PortCtx->DevExt, PortCtx);
            Information = 0;
            break;
        }

        /* 
         * 4. STORAGE QUERY PROPERTY IOCTL
         */
        case IOCTL_STORAGE_QUERY_PROPERTY:
        {
            PSTORAGE_PROPERTY_QUERY Query = (PSTORAGE_PROPERTY_QUERY)SystemBuffer;

            if (InputLength < sizeof(STORAGE_PROPERTY_QUERY) || !SystemBuffer)
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            if (Query->PropertyId == StorageDeviceProperty && Query->QueryType == PropertyStandardQuery)
            {
                if (OutputLength < sizeof(STORAGE_DEVICE_DESCRIPTOR))
                {
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                PSTORAGE_DEVICE_DESCRIPTOR Desc = (PSTORAGE_DEVICE_DESCRIPTOR)SystemBuffer;
                RtlZeroMemory(Desc, sizeof(STORAGE_DEVICE_DESCRIPTOR));

                Desc->Version = sizeof(STORAGE_DEVICE_DESCRIPTOR);
                Desc->Size = sizeof(STORAGE_DEVICE_DESCRIPTOR);
                Desc->DeviceType = FILE_DEVICE_DISK;
                Desc->RemovableMedia = FALSE;
                Desc->CommandQueueing = TRUE; // NCQ Support
                Desc->BusType = BusTypeSata;  // 0x0B (SATA)

                Information = sizeof(STORAGE_DEVICE_DESCRIPTOR);
                Status = STATUS_SUCCESS;
            }
            else
            {
                Status = STATUS_NOT_SUPPORTED;
            }
            break;
        }

        /* 
         * 5. STORAGE CHECK VERIFY IOCTL
         */
        case IOCTL_STORAGE_CHECK_VERIFY:
        {
            if (PortCtx->Registers && ((PortCtx->Registers->Ssts & AHCI_PxSSTS_DET_MASK) == AHCI_PxSSTS_DET_PRESENT_PHY))
            {
                Status = STATUS_SUCCESS;
            }
            else
            {
                Status = STATUS_NO_SUCH_DEVICE;
            }
            Information = 0;
            break;
        }

        /* 
         * 6. DISK GET DRIVE GEOMETRY IOCTL
         */
        case IOCTL_DISK_GET_DRIVE_GEOMETRY:
        {
            if (OutputLength < sizeof(DISK_GEOMETRY) || !SystemBuffer)
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            PDISK_GEOMETRY Geometry = (PDISK_GEOMETRY)SystemBuffer;
            RtlZeroMemory(Geometry, sizeof(DISK_GEOMETRY));

            Geometry->BytesPerSector = (PortCtx->SectorSize > 0) ? PortCtx->SectorSize : 512;
            Geometry->MediaType = FixedMedia;
            Geometry->SectorsPerTrack = 63;
            Geometry->TracksPerCylinder = 255;
            if (PortCtx->TotalSectors > 0)
            {
                Geometry->Cylinders.QuadPart = PortCtx->TotalSectors / (63 * 255);
            }

            Information = sizeof(DISK_GEOMETRY);
            Status = STATUS_SUCCESS;
            break;
        }

        /* 
         * 7. STORAGE GET DEVICE NUMBER IOCTL
         */
        case IOCTL_STORAGE_GET_DEVICE_NUMBER:
        {
            if (OutputLength < sizeof(STORAGE_DEVICE_NUMBER) || !SystemBuffer)
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            PSTORAGE_DEVICE_NUMBER DevNum = (PSTORAGE_DEVICE_NUMBER)SystemBuffer;
            DevNum->DeviceType = FILE_DEVICE_DISK;
            DevNum->DeviceNumber = PortCtx->PortNumber;
            DevNum->PartitionNumber = 0;

            Information = sizeof(STORAGE_DEVICE_NUMBER);
            Status = STATUS_SUCCESS;
            break;
        }

        /* 
         * 8. DATA SET MANAGEMENT / TRIM IOCTL
         */
        case IOCTL_STORAGE_MANAGE_DATA_SET_ATTRIBUTES:
        {
            PDEVICE_MANAGE_DATA_SET_ATTRIBUTES DsmAttr;
            PDEVICE_DATA_SET_RANGE Ranges;
            ULONG RangeCount;

            if (InputLength < sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES) || !SystemBuffer)
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            DsmAttr = (PDEVICE_MANAGE_DATA_SET_ATTRIBUTES)SystemBuffer;

            // Pastikan action-nya adalah TRIM (1)
            if (DsmAttr->Action == DeviceDsmAction_Trim)
            {
                if (DsmAttr->DataSetRangesOffset < sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES) ||
                    (DsmAttr->DataSetRangesOffset + DsmAttr->DataSetRangesLength) > InputLength)
                {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                Ranges = (PDEVICE_DATA_SET_RANGE)((ULONG_PTR)DsmAttr + DsmAttr->DataSetRangesOffset);
                RangeCount = DsmAttr->DataSetRangesLength / sizeof(DEVICE_DATA_SET_RANGE);

                if (RangeCount == 0)
                {
                    Status = STATUS_SUCCESS;
                    Information = 0;
                    break;
                }

                Status = AhciSendTrimCommand(PortCtx, Ranges, RangeCount);
                Information = 0;
            }
            else
            {
                Status = STATUS_NOT_SUPPORTED;
            }
            break;
        }

        default:    
            Status = STATUS_NOT_SUPPORTED;
            break;
    }

    Oip->StatusInformation.Status = Status;
    Oip->StatusInformation.Information = Information;
    return Status;
}

VEASTATUS
VEAPI
AhciMjFlushBuffers(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    PAHCI_PORT_CONTEXT PortCtx;
    VEASTATUS Status;

    if (!DeviceObject->DeviceExtension)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        Oip->StatusInformation.Information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    PortCtx = *(PAHCI_PORT_CONTEXT*)DeviceObject->DeviceExtension;

    Status = AhciSendFlushCache(PortCtx->DevExt, PortCtx);

    Oip->StatusInformation.Status = Status;
    Oip->StatusInformation.Information = 0;

    return Status;
};

VEASTATUS
VEAPI
AhciObIntHandler(
    IN PVOID Object,
    IN POIP Oip
)
{
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    POIP_STACK OipStack = ObioGetCurrentOipStackLocation(Oip);

    DPRINT("AhciObIntHandler()\n");

    if (!DeviceObject || !DeviceObject->DeviceExtension || !Oip || !OipStack)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PAHCI_PORT_CONTEXT PortCtx = *(PAHCI_PORT_CONTEXT*)DeviceObject->DeviceExtension;

    /* PDO Handling */
    if (PortCtx && PortCtx->DiskPdo == DeviceObject)
    {
        switch (OipStack->MajorFunction)
        {
            case OIP_MJ_READ:
            case OIP_MJ_WRITE:
                return AhciPdoReadWrite(PortCtx, Oip, OipStack);

            case OIP_MJ_DEVICE_CONTROL:
                return AhciPdoDeviceControl(PortCtx, Oip, OipStack);

            case OIP_MJ_PNP:
                return AhciMjPnp(Object, Oip, OipStack);

            case OIP_MJ_CREATE:
            case OIP_MJ_CLOSE:
                Oip->StatusInformation.Status = STATUS_SUCCESS;
                return STATUS_SUCCESS;

            default:
                break;
        }

        Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
        return STATUS_NOT_SUPPORTED;
    }

    /* FDO Handling */
    PAHCI_DEVICE_EXTENSION DevExt = (PAHCI_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    switch (OipStack->MajorFunction)
    {
        case OIP_MJ_PNP:
            return AhciMjPnp(Object, Oip, OipStack);

        default:
            break;
    }

    Oip->StatusInformation.Status = STATUS_INVALID_DEVICE_REQUEST;
    return STATUS_INVALID_DEVICE_REQUEST;
}
