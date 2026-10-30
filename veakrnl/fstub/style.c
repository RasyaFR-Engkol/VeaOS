/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : style.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Analying Partition Style
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 13-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file style.c
 * --------------------------------------------------------------------- */

#pragma pack (push, 1)

typedef struct _MBR_PARTITION_ENTRY {
    UCHAR  BootIndicator;
    UCHAR  StartCHS[3];
    UCHAR  PartitionType;
    UCHAR  EndCHS[3];
    ULONG  StartingLBA;
    ULONG  SectorCount;
} __attribute__((packed)) FSTUB_MBR_PARTITION_ENTRY, *PFSTUB_MBR_PARTITION_ENTRY;

typedef struct _MASTER_BOOT_RECORD_ENTRY {
    UCHAR               BootCode[440];
    ULONG               DiskSignature;
    USHORT              Reserved;
    FSTUB_MBR_PARTITION_ENTRY Partition[4];
    USHORT              Signature;
} FSTUB_MASTER_BOOT_RECORD_ENTRY, *PFSTUB_MASTER_BOOT_RECORD_ENTRY;

#pragma pack (pop)

#define MBR_BOOT_ACTIVE      0x80

BOOLEAN
VEAPI
ObioAnalyzeBdiMatchMbr(
    IN PCHAR Buffer,
    IN ULONG PartitionNumber,
    IN ULONG DiskSignature
)
{
    PFSTUB_MASTER_BOOT_RECORD_ENTRY MbrEntry;
    ULONG PartitionIndex;

    if (!Buffer || !DiskSignature)
    {
        return FALSE;
    }

    if (PartitionNumber == 0 || PartitionNumber > 4)
    {
        DPRINT("Invalid boot partition number: %u (valid range is 1-4)\n",
               PartitionNumber);
        return FALSE;
    }

    MbrEntry = (PFSTUB_MASTER_BOOT_RECORD_ENTRY)Buffer;

    DPRINT("Disk Signature: 0x%x\n", MbrEntry->DiskSignature);
    DPRINT("BDI Disk Signature: 0x%x\n", DiskSignature);

    if (MbrEntry->DiskSignature != DiskSignature)
    {
        DPRINT("Disk signature mismatch\n");
        return FALSE;
    }

    DPRINT("Disk signature match\n");

    PartitionIndex = PartitionNumber - 1;
    if (MbrEntry->Partition[PartitionIndex].BootIndicator != MBR_BOOT_ACTIVE)
    {
        DPRINT("Partition %u does not have MBR_BOOT_ACTIVE (BootIndicator=0x%02x)\n",
               PartitionNumber,
               MbrEntry->Partition[PartitionIndex].BootIndicator);
        return FALSE;
    }

    DPRINT("Partition %u has MBR_BOOT_ACTIVE\n", PartitionNumber);
    return TRUE;
}