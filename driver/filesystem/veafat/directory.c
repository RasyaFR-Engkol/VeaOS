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
    WCHAR LfnBuffer[256];
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
                return Status;
            }

            PFAT_DIR_ENTRY Entry = (PFAT_DIR_ENTRY)SectorBuffer;
            ULONG EntriesPerSector = Vcb->BytesPerSector / sizeof(FAT_DIR_ENTRY);

            for (ULONG i = 0; i < EntriesPerSector; i++)
            {
                /* Entry 0x00: Akhir dari daftar direktori */
                if (Entry[i].FileName[0] == 0x00)
                {
                    return STATUS_OBJECT_NAME_NOT_FOUND;
                }

                /* Entry 0xE5: File terhapus, skip */
                if (Entry[i].FileName[0] == 0xE5)
                {
                    continue;
                }

                if ((Entry[i].Attributes & FAT_ATTR_VOLUME_ID) && 
                    (Entry[i].Attributes != FAT_ATTR_LFN))
                {
                    continue;
                }

                /* Skip LFN (Long File Name) Pseudo Entries */
                if (Entry[i].Attributes == FAT_ATTR_LFN)
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
    IN ULONG FileSize
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

    // 1. Tentukan apakah butuh LFN & buat SFN-nya
    NeedsLfn = FatpNeedsLfn(FileName);
    FatpGenerateSfn(FileName, Sfn);
    Checksum = FatpCalculateSfnChecksum(Sfn);

    if (NeedsLfn)
    {
        LfnCount = FatpGenerateLfnEntries(FileName, Checksum, LfnBuffer);
        TotalSlotsNeeded = LfnCount + 1; // LFN entries + 1 SFN entry
    }

    SectorBuffer = (PUCHAR)UlAllocatePoolZero(NonPagedPool, Vcb->BytesPerSector, 'FAT ');
    if (!SectorBuffer) return STATUS_INSUFFICIENT_RESOURCES;

    // 2. Cari slot berurutan sejumlah TotalSlotsNeeded di cluster direktori
    ULONG ConsecutiveFound = 0;
    ULONGLONG TargetSectorLba = 0;
    ULONG TargetEntryIndex = 0;

    while (!FAT32_IS_EOF(CurrentCluster) && CurrentCluster >= 2)
    {
        ULONGLONG ClusterLba = FAT32_CLUSTER_TO_LBA(Vcb, CurrentCluster);

        for (ULONG s = 0; s < Vcb->SectorsPerCluster; s++)
        {
            ULONGLONG SectorLba = ClusterLba + s;
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
                // Entri kosong ditemukan (0x00 = unused end, 0xE5 = deleted)
                if (Entries[i].FileName[0] == 0x00 || Entries[i].FileName[0] == 0xE5)
                {
                    if (ConsecutiveFound == 0)
                    {
                        TargetSectorLba = SectorLba;
                        TargetEntryIndex = i;
                    }

                    ConsecutiveFound++;

                    // Berhasil menemukan slot berurutan yang cukup dalam 1 sektor!
                    if (ConsecutiveFound == TotalSlotsNeeded)
                    {
                        // 3. Write LFN Entries terlebih dahulu
                        for (ULONG l = 0; l < LfnCount; l++)
                        {
                            PFAT_LFN_ENTRY LfnSlot = (PFAT_LFN_ENTRY)&Entries[TargetEntryIndex + l];
                            RtlCopyMemory(LfnSlot, &LfnBuffer[l], sizeof(FAT_LFN_ENTRY));
                        }

                        // 4. Write SFN Primary Entry di slot paling akhir
                        PFAT_DIR_ENTRY SfnSlot = &Entries[TargetEntryIndex + LfnCount];
                        RtlZeroMemory(SfnSlot, sizeof(FAT_DIR_ENTRY));
                        RtlCopyMemory(SfnSlot->FileName, Sfn, 11);
                        SfnSlot->Attributes = Attributes;
                        SfnSlot->FirstClusterLow  = (USHORT)(FirstCluster & 0xFFFF);
                        SfnSlot->FirstClusterHigh = (USHORT)((FirstCluster >> 16) & 0xFFFF);
                        SfnSlot->FileSize = FileSize;

                        // Tulis balik sektor direktori ke disk
                        Status = FatWriteDiskSectors(Vcb->TargetDeviceObject, SectorLba, 1, Vcb->BytesPerSector, SectorBuffer);

                        UlFreePoolWithTag(SectorBuffer, 'FAT ');
                        return Status;
                    }
                }
                else
                {
                    // Reset pencarian slot berurutan jika terputus entri terpakai
                    ConsecutiveFound = 0;
                }
            }
        }

        // Jika dalam cluster ini tidak muat slot berurutan, alokasikan cluster direktori baru
        ULONG NextCluster;
        Status = FatpGetNextCluster(Vcb, CurrentCluster, &NextCluster);
        if (!VEA_SUCCESS(Status) || FAT32_IS_EOF(NextCluster))
        {
            ULONG NewDirCluster;
            Status = FatpAllocateCluster(Vcb, &NewDirCluster);
            if (!VEA_SUCCESS(Status)) break;

            FatpSetNextCluster(Vcb, CurrentCluster, NewDirCluster);
            CurrentCluster = NewDirCluster;

            // Zero-out sektor cluster baru agar siap diisi slot 0x00
            RtlZeroMemory(SectorBuffer, Vcb->BytesPerSector);
            for (ULONG s = 0; s < Vcb->SectorsPerCluster; s++)
            {
                FatWriteDiskSectors(Vcb->TargetDeviceObject, FAT32_CLUSTER_TO_LBA(Vcb, NewDirCluster) + s, 1, Vcb->BytesPerSector, SectorBuffer);
            }
            ConsecutiveFound = 0;
        }
        else
        {
            CurrentCluster = NextCluster;
        }
    }

    UlFreePoolWithTag(SectorBuffer, 'FAT ');
    return STATUS_DISK_FULL;
}

/* EOF */