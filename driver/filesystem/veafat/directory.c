/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : directory.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Directory Main helper
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "veafat.h"

/* Revision History ------------------------------------------------------
 * DATE       : 17-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file directory.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
FatpAllocateCluster(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    OUT PULONG OutCluster
);

VEASTATUS
VEAPI
FatpSetNextCluster(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN ULONG CurrentCluster,
    IN ULONG NextCluster
);

BOOLEAN
FatpIsDirectoryEmpty(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN ULONG DirStartCluster
)
{
    ULONG CurrentCluster = DirStartCluster;
    PUCHAR SectorBuffer = UlAllocatePoolZero(NonPagedPool, Vcb->BytesPerSector, VEAFAT_POOL_TAG);
    if (!SectorBuffer) return FALSE;

    ULONG EntriesPerSector = Vcb->BytesPerSector / sizeof(FAT_DIR_ENTRY);

    while (CurrentCluster >= 2 && !FAT32_IS_EOF(CurrentCluster))
    {
        ULONGLONG ClusterLba = FAT32_CLUSTER_TO_LBA(Vcb, CurrentCluster);

        for (ULONG s = 0; s < Vcb->SectorsPerCluster; s++)
        {
            if (!VEA_SUCCESS(FatReadDiskSectors(Vcb->TargetDeviceObject, ClusterLba + s, 1, Vcb->BytesPerSector, SectorBuffer)))
            {
                UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
                return FALSE;
            }

            PFAT_DIR_ENTRY Entries = (PFAT_DIR_ENTRY)SectorBuffer;
            for (ULONG i = 0; i < EntriesPerSector; i++)
            {
                if (Entries[i].FileName[0] == 0x00)
                {
                    UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
                    return TRUE;
                }
                if (Entries[i].FileName[0] == 0xE5 || Entries[i].Attributes == FAT_ATTR_LFN)
                {
                    continue;
                }
                if (Entries[i].FileName[0] == '.')
                {
                    continue; /* Abaikan '.' dan '..' */
                }

                UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
                return FALSE;
            }
        }

        if (!VEA_SUCCESS(FatpGetNextCluster(Vcb, CurrentCluster, &CurrentCluster)))
        {
            break;
        }
    }

    UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
    return TRUE;
}

VEASTATUS
VEAPI
FatpSearchDirectory(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN ULONG DirStartCluster,
    IN PANSI_STRING ComponentName,
    OUT PFAT_DIR_ENTRY OutDirEntry,
    OUT PULONG OutParentCluster,
    OUT PULONG OutEntryOffset
)
{
    VEASTATUS Status;
    ULONG CurrentCluster = DirStartCluster;
    UCHAR FormattedSfn[11];
    PUCHAR SectorBuffer;
    USHORT LfnBuffer[256];
    BOOLEAN HasLfn = FALSE;
    UCHAR LfnChecksum = 0;

    if (DirStartCluster == 0)
    {
        CurrentCluster = Vcb->RootDirStartCluster;
    }

    FatpFormatSfn(ComponentName, FormattedSfn);

    SectorBuffer = (PUCHAR)UlAllocatePoolZero(
        NonPagedPool,
        4096,
        'FAT '
    );
    if (!SectorBuffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(LfnBuffer, sizeof(LfnBuffer));

    while (!FAT32_IS_EOF(CurrentCluster) && CurrentCluster != 0)
    {
        ULONGLONG ClusterLba = FAT32_CLUSTER_TO_LBA(Vcb, CurrentCluster);

        for (ULONG s = 0; s < Vcb->SectorsPerCluster; s++)
        {
            Status = FatReadDiskSectors(
                Vcb->TargetDeviceObject,
                ClusterLba + s,
                1,
                Vcb->BytesPerSector,
                SectorBuffer
            );

            if (!VEA_SUCCESS(Status))
            {
                UlFreePoolWithTag(SectorBuffer, 'FAT ');
                return Status;
            }

            PFAT_DIR_ENTRY Entry = (PFAT_DIR_ENTRY)SectorBuffer;
            ULONG EntriesPerSector = Vcb->BytesPerSector / sizeof(FAT_DIR_ENTRY);

            for (ULONG i = 0; i < EntriesPerSector; i++)
            {
                /* Entry 0x00: Akhir dari daftar direktori */
                if (Entry[i].FileName[0] == 0x00)
                {
                    UlFreePoolWithTag(SectorBuffer, 'FAT ');
                    return STATUS_OBJECT_NAME_NOT_FOUND;
                }

                /* Entry 0xE5: File terhapus, skip */
                if (Entry[i].FileName[0] == 0xE5)
                {
                    HasLfn = FALSE;
                    continue;
                }

                /* Skip LFN (Long File Name) Pseudo Entries */
                if ((Entry[i].Attributes & 0x3F) == FAT_ATTR_LFN)
                {
                    PFAT_LFN_ENTRY LfnEntry = (PFAT_LFN_ENTRY)&Entry[i];
                    UCHAR Sequence = LfnEntry->SequenceNumber;

                    /* Jika entry LFN terakhir (penanda awal urutan pembacaan LFN) */
                    if (Sequence & FAT_LFN_LAST_ENTRY)
                    {
                        RtlZeroMemory(LfnBuffer, sizeof(LfnBuffer));
                        HasLfn = TRUE;
                        LfnChecksum = LfnEntry->Checksum;
                    }

                    ULONG SeqIndex = (Sequence & FAT_LFN_SEQ_MASK);
                    if (SeqIndex >= 1 && SeqIndex <= 20)
                    {
                        /* Taruh 13 karakter di offset yang sesuai */
                        ULONG CharOffset = (SeqIndex - 1) * 13;
                        if (CharOffset + 13 < 256)
                        {
                            FatpExtractLfnChars(LfnEntry, &LfnBuffer[CharOffset]);
                        }
                    }
                    continue;
                }

                if (Entry[i].Attributes & FAT_ATTR_VOLUME_ID)
                {
                    HasLfn = FALSE;
                    continue;
                }

                BOOLEAN Match = FALSE;

                if (HasLfn)
                {
                    /* Verifikasi checksum SFN agar LFN terbukti milik entri ini */
                    if (FatpCalculateSfnChecksum(Entry[i].FileName) == LfnChecksum)
                    {
                        Match = FatpCompareLfnWithAnsi(LfnBuffer, ComponentName);
                    }
                    HasLfn = FALSE; // Reset state setelah SFN diproses
                }

                if (!Match)
                {
                    if (RtlCompareMemory(Entry[i].FileName, FormattedSfn, 11) == 11)
                    {
                        Match = TRUE;
                    }
                }

                if (Match)
                {
                    *OutDirEntry = Entry[i];
                    *OutParentCluster = CurrentCluster;
                    *OutEntryOffset = (s * Vcb->BytesPerSector) + (i * sizeof(FAT_DIR_ENTRY));

                    UlFreePoolWithTag(SectorBuffer, 'FAT ');
                    return STATUS_SUCCESS;
                }
            }
        }

        Status = FatpGetNextCluster(Vcb, CurrentCluster, &CurrentCluster);
        if (!VEA_SUCCESS(Status))
        {
            break;
        }
    }

    UlFreePoolWithTag(SectorBuffer, 'FAT ');
    return STATUS_OBJECT_NAME_NOT_FOUND;
}

VEASTATUS
VEAPI
FatpAddDirectoryEntry(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN ULONG ParentDirCluster,
    IN PANSI_STRING FileName,
    IN UCHAR Attributes,
    IN ULONG FirstCluster,
    IN ULONG FileSize,
    OUT PULONG OutParentCluster OPTIONAL,
    OUT PULONG OutEntryOffset OPTIONAL
)
//
// Routine Description:
//     Adds a new file directory entry with full LFN (Long File Name) support.
//     Finds contiguous empty slots in directory clusters, writes LFN entries
//     followed by the primary SFN entry.
//
{
    VEASTATUS Status;
    ULONG CurrentCluster = ParentDirCluster;
    UCHAR Sfn[11];
    UCHAR Checksum;
    BOOLEAN NeedsLfn;
    ULONG LfnCount = 0;
    ULONG TotalSlotsNeeded = 1;
    FAT_LFN_ENTRY LfnBuffer[20];
    PUCHAR SectorBuffer;

    if (CurrentCluster == 0)
    {
        CurrentCluster = Vcb->RootDirStartCluster;
    }

    NeedsLfn = FatpNeedsLfn(FileName);
    if (NeedsLfn)
    {
        BOOLEAN FoundUnique = FALSE;
        for (ULONG Tail = 1; Tail <= 9999; Tail++)
        {
            FatpGenerateSfnWithTail(FileName, Tail, Sfn);
            if (!FatpSfnExistsInDirectory(Vcb, CurrentCluster, Sfn))
            {
                FoundUnique = TRUE;
                break;
            }
        }
        if (!FoundUnique)
        {
            return STATUS_OBJECT_NAME_COLLISION;
        }
    }
    else
    {
        FatpFormatSfn(FileName, Sfn);
    }

    Checksum = FatpCalculateSfnChecksum(Sfn);

    if (NeedsLfn)
    {
        LfnCount = FatpGenerateLfnEntries(FileName, Checksum, LfnBuffer);
        TotalSlotsNeeded = 1 + LfnCount;
    }

    SectorBuffer = (PUCHAR)UlAllocatePoolZero(NonPagedPool, Vcb->BytesPerSector, 'FAT ');
    if (!SectorBuffer) return STATUS_INSUFFICIENT_RESOURCES;

    while (!FAT32_IS_EOF(CurrentCluster) && CurrentCluster >= 2)
    {
        ULONGLONG ClusterLba = FAT32_CLUSTER_TO_LBA(Vcb, CurrentCluster);

        for (ULONG s = 0; s < Vcb->SectorsPerCluster; s++)
        {
            ULONGLONG SectorLba = ClusterLba + s;
            ULONG ConsecutiveFound = 0; /* Wajib reset setiap pindah sektor */
            ULONG TargetEntryIndex = 0;

            Status = FatReadDiskSectors(Vcb->TargetDeviceObject, SectorLba, 1, Vcb->BytesPerSector, SectorBuffer);
            if (!VEA_SUCCESS(Status))
            {
                UlFreePoolWithTag(SectorBuffer, 'FAT ');
                return Status;
            }

            PFAT_DIR_ENTRY Entries = (PFAT_DIR_ENTRY)SectorBuffer;
            ULONG EntriesPerSector = Vcb->BytesPerSector / sizeof(FAT_DIR_ENTRY);

            for (ULONG i = 0; i < EntriesPerSector; i++)
            {
                if (Entries[i].FileName[0] == 0x00 || Entries[i].FileName[0] == 0xE5)
                {
                    if (ConsecutiveFound > 0 && ConsecutiveFound < TotalSlotsNeeded)
                    {
                        BOOLEAN ModifiedZero = FALSE;
                        for (ULONG k = TargetEntryIndex; k < EntriesPerSector; k++)
                        {
                            if (Entries[k].FileName[0] == 0x00)
                            {
                                Entries[k].FileName[0] = 0xE5;
                                ModifiedZero = TRUE;
                            }
                        }
                        if (ModifiedZero)
                        {
                            FatWriteDiskSectors(Vcb->TargetDeviceObject, SectorLba, 1, Vcb->BytesPerSector, SectorBuffer);
                        }
                    }
                    
                    if (ConsecutiveFound == 0)
                    {
                        TargetEntryIndex = i;
                    }

                    ConsecutiveFound++;

                    if (ConsecutiveFound == TotalSlotsNeeded)
                    {
                        for (ULONG l = 0; l < LfnCount; l++)
                        {
                            PFAT_LFN_ENTRY LfnSlot = (PFAT_LFN_ENTRY)&Entries[TargetEntryIndex + l];
                            RtlCopyMemory(LfnSlot, &LfnBuffer[l], sizeof(FAT_LFN_ENTRY));
                        }

                        ULONG SfnSlotIndex = TargetEntryIndex + LfnCount;
                        PFAT_DIR_ENTRY SfnSlot = &Entries[SfnSlotIndex];
                        RtlZeroMemory(SfnSlot, sizeof(FAT_DIR_ENTRY));
                        RtlCopyMemory(SfnSlot->FileName, Sfn, 11);
                        SfnSlot->Attributes = Attributes;
                        SfnSlot->FirstClusterLow  = (USHORT)(FirstCluster & 0xFFFF);
                        SfnSlot->FirstClusterHigh = (USHORT)((FirstCluster >> 16) & 0xFFFF);
                        SfnSlot->FileSize = FileSize;

                        Status = FatWriteDiskSectors(Vcb->TargetDeviceObject, SectorLba, 1, Vcb->BytesPerSector, SectorBuffer);
                        if (VEA_SUCCESS(Status))
                        {
                            if (OutParentCluster) *OutParentCluster = CurrentCluster;
                            if (OutEntryOffset)   *OutEntryOffset   = (s * Vcb->BytesPerSector) + (SfnSlotIndex * sizeof(FAT_DIR_ENTRY));
                        }

                        UlFreePoolWithTag(SectorBuffer, 'FAT ');
                        return Status;
                    }
                }
                else
                {
                    ConsecutiveFound = 0;
                }
            }
        }

        ULONG NextCluster;
        Status = FatpGetNextCluster(Vcb, CurrentCluster, &NextCluster);
        if (!VEA_SUCCESS(Status) || FAT32_IS_EOF(NextCluster))
        {
            ULONG NewDirCluster;
            Status = FatpAllocateCluster(Vcb, &NewDirCluster);
            if (!VEA_SUCCESS(Status)) break;

            FatpSetNextCluster(Vcb, CurrentCluster, NewDirCluster);
            CurrentCluster = NewDirCluster;

            RtlZeroMemory(SectorBuffer, Vcb->BytesPerSector);
            for (ULONG s = 0; s < Vcb->SectorsPerCluster; s++)
            {
                FatWriteDiskSectors(Vcb->TargetDeviceObject, FAT32_CLUSTER_TO_LBA(Vcb, NewDirCluster) + s, 1, Vcb->BytesPerSector, SectorBuffer);
            }
        }
        else
        {
            CurrentCluster = NextCluster;
        }
    }

    UlFreePoolWithTag(SectorBuffer, 'FAT ');
    return STATUS_DISK_FULL;
}

VEASTATUS
VEAPI
FatpUpdateDirectoryEntry(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN PVEAFAT_FCB Fcb
)
{
    if (!Fcb || Fcb->IsVolumeFcb || Fcb->ParentDirCluster < 2)
    {
        return STATUS_INVALID_PARAMETER;
    }

    ULONG SectorIndex = Fcb->DirEntryByteOffset / Vcb->BytesPerSector;
    ULONG OffsetInSector = Fcb->DirEntryByteOffset % Vcb->BytesPerSector;
    ULONGLONG SectorLba = FAT32_CLUSTER_TO_LBA(Vcb, Fcb->ParentDirCluster) + SectorIndex;

    PUCHAR SectorBuffer = UlAllocatePoolZero(NonPagedPool, Vcb->BytesPerSector, VEAFAT_POOL_TAG);
    if (!SectorBuffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    VEASTATUS Status = FatReadDiskSectors(Vcb->TargetDeviceObject, SectorLba, 1, Vcb->BytesPerSector, SectorBuffer);
    if (VEA_SUCCESS(Status))
    {
        PFAT_DIR_ENTRY Entry = (PFAT_DIR_ENTRY)(SectorBuffer + OffsetInSector);
        Entry->Attributes       = Fcb->Attributes;
        Entry->FirstClusterLow  = (USHORT)(Fcb->FirstCluster & 0xFFFF);
        Entry->FirstClusterHigh = (USHORT)((Fcb->FirstCluster >> 16) & 0xFFFF);
        Entry->FileSize         = Fcb->FileSize;

        Status = FatWriteDiskSectors(Vcb->TargetDeviceObject, SectorLba, 1, Vcb->BytesPerSector, SectorBuffer);
    }

    UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
    return Status;
}

/* EOF */