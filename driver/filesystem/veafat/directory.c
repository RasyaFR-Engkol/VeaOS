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
        return STATUS_INSUFFICIENT_MEMORY;
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