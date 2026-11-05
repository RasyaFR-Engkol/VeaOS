/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : module.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Module
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <osi386.h>

/* Revision History ------------------------------------------------------
 * DATE       : 31-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file module.c
 * --------------------------------------------------------------------- */

PLOADED_MODULE_LIST_ENTRY
VEAPI
LdrInsertToModuleListEntry(
    IN OUT PBLOCK_BOOT_2 BlockBoot,
    IN PCSTR FilePath,
    IN PCSTR RegistryPath OPTIONAL,
    IN PVOID ImageBase,
    IN ULONG ImageSize,
    IN PVOID EntryPoint,
    IN ULONG Flags
)
{
    PLOADED_MODULE_LIST_ENTRY Entry;

    if (!ImageBase) return NULL;

    Entry = (PLOADED_MODULE_LIST_ENTRY)LmAllocatePool(
        LdrUnreclaimablePool, 
        sizeof(LOADED_MODULE_LIST_ENTRY)
    );
    if (!Entry) return NULL;

    cmemset(Entry, 0, sizeof(LOADED_MODULE_LIST_ENTRY));

    if (FilePath)
    {
        ULONG PathLen = 0;
        while (FilePath[PathLen] != '\0') PathLen++;

        PCHAR PathBuffer = (PCHAR)LmAllocatePool(LdrUnreclaimablePool, PathLen + 1);
        if (PathBuffer)
        {
            cmemcpy(PathBuffer, FilePath, PathLen + 1);
            RtlInitAnsiString(&Entry->FilePath, PathBuffer);
        }
    }

    if (RegistryPath)
    {
        ULONG RegLen = 0;
        while (RegistryPath[RegLen] != '\0') RegLen++;

        PCHAR RegBuffer = (PCHAR)LmAllocatePool(LdrUnreclaimablePool, RegLen + 1);
        if (RegBuffer)
        {
            cmemcpy(RegBuffer, RegistryPath, RegLen + 1);
            RtlInitAnsiString(&Entry->RegistryPath, RegBuffer);
        }
    }

    Entry->ImageBase = ImageBase;
    Entry->ImageSize = ImageSize;
    Entry->EntryPoint = EntryPoint;
    Entry->Flags = Flags;

    if (BlockBoot->LoadedModuleListHead.Flink == NULL || 
        BlockBoot->LoadedModuleListHead.Blink == NULL)
    {
        InitializeListHead(&BlockBoot->LoadedModuleListHead);
    }

    InsertTailList(&BlockBoot->LoadedModuleListHead, &Entry->Link);

    return Entry;
}