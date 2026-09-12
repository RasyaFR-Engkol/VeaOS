/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : bios.c
   CREATOR     : RasyaFr-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Getting SMBIOS Information
 ----- Effective since 07-2026 til FOREVER ------------------------------- */

#include <osi386.h>

/* Revision History ------------------------------------------------------
 * DATE       : 26-07-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file bios.c
 * --------------------------------------------------------------------- */

static
BOOLEAN
LdrVerifySmbiosChecksum(
    IN PUCHAR Buffer, 
    IN UCHAR Length
)
{
    /* Just calculating cheksum */
    UCHAR Sum = 0;
    for (UCHAR i = 0; i < Length; i++)
    {
        Sum += Buffer[i];
    }
    return (Sum == 0);
}

BOOLEAN
VEAPI
LdrGetSmbiosInformation(IN OUT BLOCK_BOOT_2 *BlockBoot2)
{
    /* We need to map BIOS Area ROM */
    //
    // 1. Map area BIOS ROM (F-Segment: 0xF0000 - 0xFFFFF, 64KB) pakai LmMapIoSpace
    //    agar aman dibaca walau CR3 sudah aktif.
    //
    PUCHAR BiosVirt = (PUCHAR)LmMapIoSpace(0xF0000, 0x10000);
    if (!BiosVirt)
    {
        BlockBoot2->SmbiosTable = NULL;
        return FALSE;
    }

    PSMBIOS_ENTRY_POINT FoundEps = NULL;

    for(ULONG Offset = 0; Offset < 0x10000; Offset += 16)
    {
        // Check SMBIOS Header
        if (BiosVirt[Offset]     == '_' &&
            BiosVirt[Offset + 1] == 'S' &&
            BiosVirt[Offset + 2] == 'M' &&
            BiosVirt[Offset + 3] == '_')
        {
            // We found or candidate
            PSMBIOS_ENTRY_POINT Candidate = (PSMBIOS_ENTRY_POINT)&BiosVirt[Offset];

            // Validate size and checksum for SMBIOS
            if (Candidate->Length >= sizeof(SMBIOS_ENTRY_POINT) &&
                LdrVerifySmbiosChecksum((PUCHAR)Candidate, Candidate->Length))
            {
                FoundEps = Candidate;
                break;
            }
        }
    }

    // if no candidate, return
    if (!FoundEps)
    {
        BlockBoot2->SmbiosTable = NULL;
        return FALSE;
    }

    // We also need to check the table so we need to map this
    PVOID TableVirt = LmMapIoSpace(FoundEps->TableAddress, FoundEps->TableLength);
    if (!TableVirt)
    {
        BlockBoot2->SmbiosTable = NULL;
        return FALSE;
    }

    // Allocate permanent memory in Block Boot with BootPool
    PSMBIOS_ENTRY_POINT PoolEps = (PSMBIOS_ENTRY_POINT)LmAllocatePool(
        LdrUnreclaimablePool, 
        sizeof(SMBIOS_ENTRY_POINT)
    );

    PVOID PoolTable = LmAllocatePool(
        LdrUnreclaimablePool, 
        FoundEps->TableLength
    );

    if (!PoolEps || !PoolTable)
    {
        BlockBoot2->SmbiosTable = NULL;
        return FALSE;
    }

    // Copy our data then
    cmemcpy(PoolEps, FoundEps, sizeof(SMBIOS_ENTRY_POINT));
    cmemcpy(PoolTable, TableVirt, FoundEps->TableLength);
    PoolEps->TableAddress = (ULONG)PoolTable;
    BlockBoot2->SmbiosTable = PoolEps;

    return TRUE;
}