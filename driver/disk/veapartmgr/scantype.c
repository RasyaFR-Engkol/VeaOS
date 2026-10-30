/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : scantype.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Pm Scanning to determine if this disk MBR or GPT
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "veapm.h"

/* Revision History ------------------------------------------------------
 * DATE       : 12-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file scantype.c
 * --------------------------------------------------------------------- */

const GUID GUID_NULL = { 0 };

VEASTATUS
VEAPI
PmpCreatePartitionDevice(
    IN PPM_DEVICE_EXTENSION DiskDevExt,
    IN PPM_PARTITION_ENTRY PartitionEntry,
    IN ULONG PartitionNum,
    IN PVOID PartitionInfo
)
{
    PDEVICE_OBJECT PartDevice = NULL;
    PPM_DEVICE_EXTENSION PartDevExt = NULL;
    VEASTATUS Status;
    CHAR DevNameBuf[64];
    KIRQL OldIrql;

    RtlStringCbPrintfA(DevNameBuf, sizeof(DevNameBuf), "Harddisk%u/Partition%u", DiskDevExt->HarddiskNum, PartitionNum);

    Status = ObioCreateNewDevice(
        DiskDevExt->SelfDeviceObject->DriverObject,
        sizeof(PM_DEVICE_EXTENSION),
        DevNameBuf,
        FILE_DEVICE_DISK,
        &PartDevice
    );

    if (!VEA_SUCCESS(Status))
    {
        PM_ERROR("Failed to create partition device: 0x%x\n", Status);
        return Status;
    }
    PM_INFO("Created partition device object %p with object type %s at /Device/%s\n",
            PartDevice,
            OBJECT_TO_OBJECT_HEADER(PartDevice)->Type->TypeName,
            DevNameBuf);

    PartDevExt = (PPM_DEVICE_EXTENSION)PartDevice->DeviceExtension;
    RtlZeroMemory(PartDevExt, sizeof(PM_DEVICE_EXTENSION));

    PartDevExt->SelfDeviceObject = PartDevice;
    PartDevExt->LowerDeviceObject = DiskDevExt->LowerDeviceObject; 
    PartDevExt->DeviceType = PmDeviceTypePartition;
    PartDevExt->HarddiskNum = DiskDevExt->HarddiskNum;
    PartDevExt->PartitionNum = PartitionNum;
    PartDevExt->BytesPerSector = DiskDevExt->BytesPerSector;
    PartDevExt->StartingOffset = PartitionEntry->StartingOffset;
    PartDevExt->PartitionLength = PartitionEntry->PartitionLength;
    PartDevExt->PartitionStyle = PartitionEntry->PartitionStyle;
    PartDevExt->DiskGeometry = DiskDevExt->DiskGeometry;

    if (PartitionEntry->PartitionStyle == PmGuidPartitionTable)
    {
        PGUID_PARTITION_TABLE_ENTRY GptEntry = (PGUID_PARTITION_TABLE_ENTRY)PartitionInfo;
        PartDevExt->Info.Gpt.PartitionTypeGuid = GptEntry->PartitionTypeGuid;
        PartDevExt->Info.Gpt.PartitionIdGuid = GptEntry->UniquePartitionGuid;
        PartDevExt->Info.Gpt.Attributes = GptEntry->Attributes;
        RtlCopyMemory(PartDevExt->Info.Gpt.PartitionName, GptEntry->PartitionName, sizeof(GptEntry->PartitionName));
    }
    else
    {
        PMBR_PARTITION_ENTRY MbrEntry = (PMBR_PARTITION_ENTRY)PartitionInfo;
        PartDevExt->Info.Mbr.PartitionType = MbrEntry->PartitionType;
        PartDevExt->Info.Mbr.BootIndicator = MbrEntry->BootIndicator;
    }

    PartDevExt->ParentDeviceObject = DiskDevExt->SelfDeviceObject;
    KsAcquireSpinLock(&DiskDevExt->PartitionListLock, &OldIrql);
    InsertTailList(&DiskDevExt->PartitionListHead, &PartDevExt->PartitionListEntry);
    DiskDevExt->PartitionCount++;
    KsReleaseSpinLock(&DiskDevExt->PartitionListLock, OldIrql);

    PartDevice->Flags |= DiskDevExt->LowerDeviceObject->Flags & (DO_DIRECT_IO | DO_BUFFERED_IO);
    PartDevice->Flags &= ~DO_DEVICE_INITIALIZING;

    PM_INFO("Berhasil bikin partisi: %s (Offset: 0x%08X%08X, Length: 0x%08X%08X)\n",
            DevNameBuf,
            (ULONG)(PartitionEntry->StartingOffset >> 32),
            (ULONG)PartitionEntry->StartingOffset,
            (ULONG)(PartitionEntry->PartitionLength >> 32),
            (ULONG)PartitionEntry->PartitionLength);

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
PmpCreateGptPartitionPdo(IN PPM_DEVICE_EXTENSION DevExt)
{
    VEASTATUS Status;
    ULONG SectorSize = DevExt->BytesPerSector;
    PCHAR Buffer;
    PGPT_HEADER Header;
    PVOID EntriesBuffer = NULL;
    ULONG EntriesByteSize;
    ULONG EntriesSectorCount;
    ULONG PartitionNum = 1;

    Buffer = UlAllocatePoolZero(NonPagedPool, SectorSize, TAG_OBIO);
    if (!Buffer) return STATUS_INSUFFICIENT_MEMORY;

    Status = PmpReadSectors(DevExt, GPT_HEADER_LBA, 1, Buffer);
    if (!VEA_SUCCESS(Status))
    {
        PM_ERROR("Read GPT Header failed: 0x%x\n", Status);
        return Status;
    }

    Header = (PGPT_HEADER)Buffer;
    
    if (RtlCompareMemory(Header->Signature, "EFI PART", 8) != 8)
    {
        PM_ERROR("Invalid GPT Signature\n");
        return STATUS_DISK_CORRUPT_ERROR;
    }

    EntriesByteSize = Header->NumberOfPartitionEntries * Header->SizeOfPartitionEntry;
    if (EntriesByteSize == 0)
    {
        return STATUS_SUCCESS;
    }

    EntriesSectorCount = (EntriesByteSize + SectorSize - 1) / SectorSize;

    EntriesBuffer = UlAllocatePoolZero(NonPagedPool, EntriesSectorCount * SectorSize, TAG_OBIO);
    if (!EntriesBuffer)
    {
        PM_ERROR("Failed to allocate memory for GPT partition entries\n");
        return STATUS_INSUFFICIENT_MEMORY;
    }

    Status = PmpReadSectors(DevExt, Header->PartitionEntryLBA, EntriesSectorCount, EntriesBuffer);
    if (!VEA_SUCCESS(Status))
    {
        PM_ERROR("Failed to read GPT partition entry array: 0x%x\n", Status);
        UlFreePoolWithTag(EntriesBuffer, TAG_OBIO);
        return Status;
    }

    for (ULONG i = 0; i < Header->NumberOfPartitionEntries; i++)
    {
        PM_PARTITION_ENTRY PartEntry;
        PGUID_PARTITION_TABLE_ENTRY Entry = 
            (PGUID_PARTITION_TABLE_ENTRY)((PUCHAR)EntriesBuffer + (i * Header->SizeOfPartitionEntry));

        if (IsEqualGUID(&Entry->PartitionTypeGuid, &GUID_NULL))
        {
            continue;
        }

        RtlZeroMemory(&PartEntry, sizeof(PM_PARTITION_ENTRY));

        PartEntry.PartitionStyle = PmGuidPartitionTable;
        PartEntry.StartingOffset = Entry->StartingLBA * SectorSize;
        PartEntry.PartitionLength = (Entry->EndingLBA - Entry->StartingLBA + 1) * SectorSize;
        PartEntry.HiddenSectors = (ULONG)Entry->StartingLBA;

        Status = PmpCreatePartitionDevice(
            DevExt,
            &PartEntry,
            PartitionNum,
            Entry
        );

        if (VEA_SUCCESS(Status))
        {
            PartitionNum++;
        }
        else
        {
            PM_WARNING("Failed to create partition PDO for entry %u: 0x%x\n", i, Status);
        }
    }

    UlFreePoolWithTag(EntriesBuffer, TAG_OBIO);

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
PmpDetectAndCreatePartition(
    IN PPM_DEVICE_EXTENSION DevExt,
    IN PCHAR Buffer
)
{
    VEASTATUS Status;
    PMASTER_BOOT_RECORD_ENTRY MbrRecordEntry;
    ULONG i, PartitionNum = 1;

    MbrRecordEntry = (PMASTER_BOOT_RECORD_ENTRY)Buffer;

    if (MbrRecordEntry->Partition[0].PartitionType == MBR_TYPE_GPT_PROTECTIVE)
    {
        return PmpCreateGptPartitionPdo(DevExt);
    }

    /* MBR Way */
    for (i = 0; i < 4; i++)
    {
        PM_PARTITION_ENTRY PartitionEntry;

        if (MbrRecordEntry->Partition[i].PartitionType == MBR_TYPE_EMPTY || 
            MbrRecordEntry->Partition[i].SectorCount == 0)
        {
            continue;
        }

        PartitionEntry.PartitionStyle = PmMasterBootRecord;
        PartitionEntry.PartitionType  = MbrRecordEntry->Partition[i].PartitionType;
        PartitionEntry.BootIndicator  = (MbrRecordEntry->Partition[i].BootIndicator == MBR_BOOT_ACTIVE);
        PartitionEntry.StartingOffset  = (ULONGLONG)MbrRecordEntry->Partition[i].StartingLBA * DevExt->BytesPerSector;
        PartitionEntry.PartitionLength = (ULONGLONG)MbrRecordEntry->Partition[i].SectorCount * DevExt->BytesPerSector;
        PartitionEntry.HiddenSectors   = MbrRecordEntry->Partition[i].StartingLBA;

        Status = PmpCreatePartitionDevice(DevExt, &PartitionEntry, PartitionNum, &MbrRecordEntry->Partition[i]);
        if (!VEA_SUCCESS(Status))
        {
            PM_WARNING("Partition no %u is failed to created\n", i);
            continue;
        }
        else
        {
            PartitionNum++;
        }
    }

    return STATUS_SUCCESS;
}