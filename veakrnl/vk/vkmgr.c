/* VeaOS SPDX License ----------------------------------------------------
  SPDX-License-Identifier: GPL-2.0-only
  LICENSE     : GNU General Public License v2.0
  FILE        : vkmgr.c
  CREATOR     : RasyaFR-Engkol
  MAINTAINER  : VeaOS Team
  PURPOSE     : VeaKey Subsystem
----- Effective since 07-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
* DATE       : 30-07-2026
* NAME       : RasyaFR-Engkol
* REVISION   : Creating file vkinit.c
* --------------------------------------------------------------------- */

PVK_HIVE VkSystemHive = NULL;

VOID
VEAPI
VkpMarkCellDirty(
    IN PVK_HIVE Hive,
    IN ULONG CellOffset
)
{
    if (!Hive || VkIsVolatileOffset(CellOffset) || Hive->IsReadOnly)
    {
        return;
    }

    // Check RawOffset
    ULONG RawOffset = VkGetRawOffset(CellOffset);

    // Should not more than HiveBaseAddress length
    if (RawOffset >= Hive->Length)
    {
        KdPrintf("[Vk] WARNING | CellOffset 0x%X exceeds Hive Length!\n", CellOffset);
        return;
    }

    // Count Block Index
    ULONG BlockIndex  = RawOffset / VK_BLOCK_SIZE;
    ULONG VectorIndex = BlockIndex / 32; // 1 ULONG = 32 Bits
    ULONG BitShift    = BlockIndex % 32;

    if (VectorIndex < Hive->DirtyVectorSize)
    {
        // Set as dirty
        Hive->DirtyVector[VectorIndex] |= (1 << BitShift);
        Hive->IsDirty = TRUE;
    }
}

// Helper API
PKEY_DIRECTORY_NODE
VEAPI
VkpFindSubkey(
    IN PVK_HIVE Hive,
    IN PKEY_DIRECTORY_NODE ParentNode,
    IN PCHAR SubkeyName,
    IN ULONG NameLen,
    OUT PULONG OutSubkeyOffset
)
{
    if (!Hive || !ParentNode || !SubkeyName || NameLen == 0)
        return NULL;

    ULONG CurrentOff = ParentNode->FirstSubKeyOffset;

    while (CurrentOff != 0)
    {
        // 1. Pake VkpGetCellPointer (Aman buat Stable & Volatile!)
        PKEY_DIRECTORY_NODE Node = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, CurrentOff);

        if (!Node || Node->Signature != KEY_DIR_SIG)
        {
            // Corrupted sibling list
            break;
        }

        // 2. String Matching
        if (Node->NameLength == NameLen &&
            RtlRawStringCompareN(Node->Name, SubkeyName, NameLen) == 0)
        {
            if (OutSubkeyOffset)
                *OutSubkeyOffset = CurrentOff; // Return offset asli (Stable/Volatile)

            return Node; // Return Pointer Node-nya
        }

        // 3. Next Sibling
        CurrentOff = Node->NextSiblingOffset;
    }

    return NULL; // Not found
}

PKEY_FILE_NODE
VEAPI
VkpFindValueNode(
    IN PVK_HIVE Hive,
    IN PKEY_DIRECTORY_NODE DirectoryNode,
    IN PANSI_STRING ValueName,
    OUT PULONG OutValueOffset OPTIONAL
)
{
	if (!Hive || !DirectoryNode || !ValueName || !ValueName->Buffer || ValueName->Length == 0)
    {
        return NULL;
    }

    ULONG CurrentValOff = DirectoryNode->FirstValueOffset;

	while (CurrentValOff != 0)
    {
        // Resolve pointer via VkpGetCellPointer (Aman buat Stable/Volatile!)
        PKEY_FILE_NODE FileNode = (PKEY_FILE_NODE)VkpGetCellPointer(Hive, CurrentValOff);

        if (!FileNode || FileNode->Signature != KEY_FILE_SIG)
        {
            // Corrupted chain / invalid offset
            break;
        }

        // Compare String Length & Name Buffer
        if (ValueName->Length == FileNode->NameLength &&
            RtlRawStringCompareN(ValueName->Buffer, FileNode->Name, FileNode->NameLength) == 0)
        {
            if (OutValueOffset != NULL)
            {
                *OutValueOffset = CurrentValOff;
            }

            return FileNode; // Found! Return pointer to KEY_FILE_NODE
        }

        // Next Sibling Value
        CurrentValOff = FileNode->NextValueOffset;
    }

    return NULL; // Value not found
}