/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : devcontrol.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : NVMeDeviceControl
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "nvmeheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 09-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file devcontrol.c
 * --------------------------------------------------------------------- */

static
VEASTATUS
NVMeReadBlockSynchronously(
    IN PDEVICE_OBJECT DeviceObject,
    IN PNVME_DEVICE_EXTENSION DeviceExtension,
    IN ULONGLONG Lba,
    OUT PVOID Buffer
)
{
    POIP ReadOip;
    POIP_STACK ReadStack;
    OIP_INFORMATION IoStatus;
    KEVENT Event;
    LARGE_INTEGER Offset;
    VEASTATUS Status;

    if (DeviceExtension->BlockSize == 0 ||
        DeviceExtension->BlockSizeShift >= 64 ||
        Lba >= DeviceExtension->NamespaceBlocks ||
        Lba > ((ULONGLONG)-1 >> DeviceExtension->BlockSizeShift))
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(&IoStatus, sizeof(IoStatus));
    KsInitializeEvent(&Event, NotificationEvent, FALSE);
    Offset.QuadPart = Lba * DeviceExtension->BlockSize;

    ReadOip = ObioAllocateOip(1);
    if (ReadOip == NULL)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    ReadOip->UserBuffer = Buffer;
    ReadOip->UserEvent = &Event;
    ReadOip->UserInfoBlock = &IoStatus;

    ReadStack = ObioGetNextOipStackLocation(ReadOip);
    ReadStack->MajorFunction = OIP_MJ_READ;
    ReadStack->TargetObject = DeviceObject;
    ReadStack->Parameters.ReadWrite.Length = DeviceExtension->BlockSize;
    ReadStack->Parameters.ReadWrite.Offset = Offset;

    Status = NVMeMjReadWrite(DeviceObject, ReadOip, ReadStack);
    if (Status == STATUS_PENDING)
    {
        KsWaitForSingleObject(&Event);
        Status = IoStatus.Status;
    }
    else
    {
        Status = ReadOip->StatusInformation.Status;
        IoStatus = ReadOip->StatusInformation;
    }

    ObioFreeOip(ReadOip);
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    if (IoStatus.Information != DeviceExtension->BlockSize)
    {
        return STATUS_IO_DEVICE_ERROR;
    }

    return STATUS_SUCCESS;
}

static
VEASTATUS
NVMeQueryDiskStyle(
    IN PDEVICE_OBJECT DeviceObject,
    IN PNVME_DEVICE_EXTENSION DeviceExtension,
    OUT PDISK_STYLE DiskStyle
)
{
    PUCHAR SectorBuffer;
    VEASTATUS Status;
    BOOLEAN HasProtectiveMbr = FALSE;

    if (DeviceExtension->BlockSize < 512 || DiskStyle == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    SectorBuffer = (PUCHAR)UlAllocatePoolZero(
        NonPagedPool,
        DeviceExtension->BlockSize,
        NVME_POOL_TAG
    );
    if (SectorBuffer == NULL)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    Status = NVMeReadBlockSynchronously(
        DeviceObject,
        DeviceExtension,
        0,
        SectorBuffer
    );
    if (!VEA_SUCCESS(Status))
    {
        goto Cleanup;
    }

    if (SectorBuffer[510] != 0x55 || SectorBuffer[511] != 0xAA)
    {
        Status = STATUS_DISK_CORRUPT_ERROR;
        goto Cleanup;
    }

    for (ULONG Index = 0; Index < 4; Index++)
    {
        if (SectorBuffer[446 + (Index * 16) + 4] == 0xEE)
        {
            HasProtectiveMbr = TRUE;
            break;
        }
    }

    if (HasProtectiveMbr)
    {
        Status = NVMeReadBlockSynchronously(
            DeviceObject,
            DeviceExtension,
            1,
            SectorBuffer
        );
        if (!VEA_SUCCESS(Status))
        {
            goto Cleanup;
        }

        if (RtlCompareMemory(SectorBuffer, "EFI PART", 8) != 8)
        {
            Status = STATUS_DISK_CORRUPT_ERROR;
            goto Cleanup;
        }

        *DiskStyle = DiskStyleGpt;
    }
    else
    {
        *DiskStyle = DiskStyleMbr;
    }

    Status = STATUS_SUCCESS;

Cleanup:
    UlFreePoolWithTag(SectorBuffer, NVME_POOL_TAG);
    return Status;
}

VEASTATUS
VEAPI
NVMeMjDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PNVME_DEVICE_EXTENSION DevExt;
    ULONG ControlCode;
    PVOID Buffer;
    ULONG OutputLen;

    DevExt = (PNVME_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    ControlCode = OipStack->Parameters.DeviceControl.IoControlCode;
    Buffer = Oip->UserBuffer;
    OutputLen = OipStack->Parameters.DeviceControl.OutputBufferLength;

    switch (ControlCode)
    {
        case IOCTL_DISK_GET_DRIVE_GEOMETRY:
        {
            if (OutputLen < sizeof(DISK_GEOMETRY))
            {
                Oip->StatusInformation.Status = STATUS_BUFFER_TOO_SMALL;
                return STATUS_BUFFER_TOO_SMALL;
            }
            
            PDISK_GEOMETRY Geometry = (PDISK_GEOMETRY)Buffer;
            Geometry->BytesPerSector = DevExt->BlockSize;
            Geometry->SectorsPerTrack = 32;
            Geometry->TracksPerCylinder = 64;
            Geometry->Cylinders.QuadPart = DevExt->NamespaceBlocks >> 11;
            Geometry->MediaType = FixedMedia;

            Oip->StatusInformation.Information = sizeof(DISK_GEOMETRY);
            Oip->StatusInformation.Status = STATUS_SUCCESS;
            return STATUS_SUCCESS;
        }

        case IOCTL_DISK_GET_LENGTH_INFO:
        {
            if (OutputLen < sizeof(GET_LENGTH_INFORMATION))
            {
                Oip->StatusInformation.Status = STATUS_BUFFER_TOO_SMALL;
                return STATUS_BUFFER_TOO_SMALL;
            }

            PGET_LENGTH_INFORMATION LengthInfo = (PGET_LENGTH_INFORMATION)Buffer;
            LengthInfo->Length.QuadPart = DevExt->NamespaceBlocks * DevExt->BlockSize;

            Oip->StatusInformation.Information = sizeof(GET_LENGTH_INFORMATION);
            Oip->StatusInformation.Status = STATUS_SUCCESS;
            return STATUS_SUCCESS;
        }

        case IOCTL_DISK_GET_DISK_STYLE:
        case IOCTL_DISK_GET_PARTITION_STYLE:
        {
            if (OutputLen < sizeof(DISK_STYLE))
            {
                Oip->StatusInformation.Status = STATUS_BUFFER_TOO_SMALL;
                Oip->StatusInformation.Information = 0;
                return STATUS_BUFFER_TOO_SMALL;
            }
            if (Buffer == NULL)
            {
                Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
                Oip->StatusInformation.Information = 0;
                return STATUS_INVALID_PARAMETER;
            }

            VEASTATUS Status = NVMeQueryDiskStyle(
                DeviceObject,
                DevExt,
                (PDISK_STYLE)Buffer
            );
            Oip->StatusInformation.Status = Status;
            Oip->StatusInformation.Information =
                VEA_SUCCESS(Status) ? sizeof(DISK_STYLE) : 0;
            return Status;
        }
    }

    Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
    return STATUS_NOT_SUPPORTED;
}

VEASTATUS
VEAPI
NVMeCreateNamespacePdo(
    IN PNVME_DEVICE_EXTENSION DevExt,
    IN ULONG NamespaceId
)
{
    VEASTATUS Status;
    PDRIVER_OBJECT DriverObject;
    PDEVICE_OBJECT Pdo;
    PNVME_DEVICE_EXTENSION PdoDevExt;
    KIRQL OldIrql;
    CHAR NvmeNamespaceChar[24];
    PLIST_ENTRY Entry;

    if (!DevExt || NamespaceId == 0 ||
        DevExt->NamespaceId != NamespaceId ||
        DevExt->NamespaceBlocks == 0 || DevExt->BlockSize == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }
    DriverObject = DevExt->DriverObject;
    if (!DriverObject)
    {
        return STATUS_INVALID_PARAMETER;
    }

    KsAcquireSpinLock(&DevExt->ChildLock, &OldIrql);
    for (Entry = DevExt->ChildList.Flink;
         Entry != &DevExt->ChildList;
         Entry = Entry->Flink)
    {
        PNVME_DEVICE_EXTENSION ChildExt;

        ChildExt = CONTAINING_RECORD(
            Entry,
            NVME_DEVICE_EXTENSION,
            ChildEntry
        );
        if (ChildExt->NamespaceId == NamespaceId)
        {
            KsReleaseSpinLock(&DevExt->ChildLock, OldIrql);
            return STATUS_SUCCESS;
        }
    }
    KsReleaseSpinLock(&DevExt->ChildLock, OldIrql);

    Status = RtlStringCbPrintfA(
        NvmeNamespaceChar,
        sizeof(NvmeNamespaceChar),
        "NvmeNamespace%u",
        NamespaceId
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* Make new NS PDO */
    Status = ObioCreateNewDevice(
        DriverObject,
        sizeof(NVME_DEVICE_EXTENSION),
        NvmeNamespaceChar,
        FILE_DEVICE_DISK,
        &Pdo
    );

    if (!VEA_SUCCESS(Status)) return Status;

    PdoDevExt = (PNVME_DEVICE_EXTENSION)Pdo->DeviceExtension;
    RtlZeroMemory(PdoDevExt, sizeof(NVME_DEVICE_EXTENSION));

    PdoDevExt->DeviceObject = Pdo;
    PdoDevExt->DriverObject = DriverObject;
    PdoDevExt->ControllerExtension = DevExt;
    PdoDevExt->PdoDisk = TRUE;
    PdoDevExt->NamespaceId = NamespaceId;
    PdoDevExt->NamespaceBlocks = DevExt->NamespaceBlocks;
    PdoDevExt->BlockSize = DevExt->BlockSize;
    PdoDevExt->BlockSizeShift = DevExt->BlockSizeShift;
    InitializeListHead(&PdoDevExt->ChildEntry);

    Pdo->Flags |= DO_DIRECT_IO;
    Pdo->Flags &= ~DO_DEVICE_INITIALIZING;

    KsAcquireSpinLock(&DevExt->ChildLock, &OldIrql);
    InsertTailList(&DevExt->ChildList, &PdoDevExt->ChildEntry);
    DevExt->ChildCount++;
    KsReleaseSpinLock(&DevExt->ChildLock, OldIrql);

    DPRINT("NVMe: Created namespace PDO %p for NSID %u (%llu blocks, %u bytes/block)\n",
           Pdo,
           NamespaceId,
           PdoDevExt->NamespaceBlocks,
           PdoDevExt->BlockSize);

    // Invalidate
    ObioInvalidateDeviceRelations(DevExt->PhysicalDeviceObject, BusRelations);

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
NVMeQueryDeviceRelations(
    IN PNVME_DEVICE_EXTENSION DevExt,
    IN DEVICE_RELATION_TYPE RelationType,
    OUT PDEVICE_RELATIONS *DeviceRelations
)
{
    PDEVICE_RELATIONS Relations;
    PLIST_ENTRY Entry;
    KIRQL OldIrql;
    ULONG Count;
    ULONG AllocationSize;

    if (!DevExt || !DeviceRelations)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *DeviceRelations = NULL;
    if (RelationType != BusRelations || DevExt->PdoDisk)
    {
        return STATUS_SUCCESS;
    }

    for (;;)
    {
        KsAcquireSpinLock(&DevExt->ChildLock, &OldIrql);
        Count = DevExt->ChildCount;
        KsReleaseSpinLock(&DevExt->ChildLock, OldIrql);

        if (Count == 0)
        {
            return STATUS_SUCCESS;
        }

        if (Count > (0xFFFFFFFFUL - sizeof(DEVICE_RELATIONS)) /
                    sizeof(PDEVICE_OBJECT))
        {
            return STATUS_INSUFFICIENT_MEMORY;
        }

        AllocationSize = sizeof(DEVICE_RELATIONS) +
                         (Count * sizeof(PDEVICE_OBJECT));
        Relations = (PDEVICE_RELATIONS)UlAllocatePoolZero(
            NonPagedPool,
            AllocationSize,
            TAG_OBIO
        );
        if (!Relations)
        {
            return STATUS_INSUFFICIENT_MEMORY;
        }

        KsAcquireSpinLock(&DevExt->ChildLock, &OldIrql);
        if (DevExt->ChildCount > Count)
        {
            KsReleaseSpinLock(&DevExt->ChildLock, OldIrql);
            UlFreePoolWithTag(Relations, TAG_OBIO);
            continue;
        }

        Relations->Count = 0;
        for (Entry = DevExt->ChildList.Flink;
             Entry != &DevExt->ChildList;
             Entry = Entry->Flink)
        {
            PNVME_DEVICE_EXTENSION ChildExt;
            PDEVICE_OBJECT ChildPdo;

            ChildExt = CONTAINING_RECORD(
                Entry,
                NVME_DEVICE_EXTENSION,
                ChildEntry
            );
            ChildPdo = ChildExt->DeviceObject;
            ObReferenceObject(ChildPdo);
            Relations->Objects[Relations->Count++] = ChildPdo;
        }
        KsReleaseSpinLock(&DevExt->ChildLock, OldIrql);

        *DeviceRelations = Relations;
        return STATUS_SUCCESS;
    }
}

VEASTATUS
VEAPI
NVMeQueryNamespaceId(
    IN PNVME_DEVICE_EXTENSION DevExt,
    IN POIP Oip,
    IN BUS_QUERY_ID_TYPE IdType
)
{
    CHAR IdBuffer[24];
    PCHAR ResultBuffer;
    ULONG BufferLength;
    VEASTATUS Status;

    if (!DevExt || !DevExt->PdoDisk || !Oip)
    {
        return STATUS_INVALID_PARAMETER;
    }

    switch (IdType)
    {
        case BusQueryDeviceID:
            Status = RtlStringCchCopyA(
                IdBuffer,
                sizeof(IdBuffer),
                "NVME/NAMESPACE"
            );
            break;

        case BusQueryInstanceID:
            Status = RtlStringCbPrintfA(
                IdBuffer,
                sizeof(IdBuffer),
                "%u",
                DevExt->NamespaceId
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