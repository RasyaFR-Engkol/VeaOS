/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : veakey.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Interactive Log UI
 ----- Effective since 07-2026 til FOREVER ------------------------------- */

#include <osi386.h>

/* Revision History ------------------------------------------------------
 * DATE       : 30-07-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file logui.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
LdrLoadSystemHive(
    IN OUT BLOCK_BOOT_2 *BlockBoot
)
{
    VFS_FILE_INFO HiveFileInfo;

    // Find SYSTEM key on disk
    cmemset(&HiveFileInfo, 0, sizeof(VFS_FILE_INFO));
    if(FsLookupFileInformation("/VeaOS/SystemReserved/Config/SYSTEM.VKEY", &HiveFileInfo) != 0)
    {
        LdrError(STATUS_FILE_NOT_FOUND);
    }
    
    // Allocate VEAKEY buffer
    ULONG HiveLength = HiveFileInfo.FileSize;
    PCHAR Buffer = (PCHAR)LmAllocatePool(LdrUnreclaimablePool, HiveLength);
    if(!Buffer)
    {
        LdrError(STATUS_MEMORY_MAP_FAILED);
    }

    // Read it to VEAKEY buffer
    ULONG BytesReaded = 0;
    BytesReaded = FsReadFile(&HiveFileInfo, Buffer);
    if(BytesReaded == 0)
    {
        LdrError(STATUS_READ_DISK_ERROR);
    }

    // We officialy store VEAKEY to RAM. Now we set the pointer from blockboot
    BlockBoot->SystemHiveBase = Buffer;
    BlockBoot->SystemHiveLength = HiveLength;
}