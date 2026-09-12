/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : vkfunc.h
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Header file for storing function for Vk subsystem
 ----- Effective since 07-2026 til FOREVER ------------------------------- */

#pragma once
#include "obtype.h"
#include "procbind.h"
#include "ldrtypes.h"
#include "ultypes.h"
#include "vktypes.h"

/* Revision History ------------------------------------------------------
 * DATE       : 30-07-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file vkfunc.h
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
VkpParseKeyObject(
    PVOID ParseObject,
    POB_LOOKUP_CONTEXT LookupContext,
    PVOID *ResolvedObject
);


BOOLEAN
VEAPI
VkInitSystem1(IN PBLOCK_BOOT_2 BlockBoot);

VEASTATUS
VEAPI
VkpQueryValueKey(
    IN PVK_KEY_OBJECT KeyObject,
    IN PANSI_STRING ValueName,
    OUT PULONG Type,
    OUT PVOID Data,
    IN ULONG DataSize,
    OUT PULONG ResultLength
);

#define VK_IS_VALUE_INLINE(FileNode) \
    (((FileNode)->Flags & KEY_FILE_INLINE) != 0)

#define VK_GET_VALUE_DATA(HiveBuffer, FileNode) \
    ((PVOID)( VK_IS_VALUE_INLINE(FileNode) ? \
        (PUCHAR)&((FileNode)->DataPOffset) : \
        ((PUCHAR)(HiveBuffer) + (FileNode)->DataPOffset + sizeof(LONG)) \
    ))

#define VK_ALIGN_UP(Size, Align) (((Size) + ((Align) - 1)) & ~((Align) - 1))

VEASTATUS
VEAPI
VeaQueryValueKey(
    IN HANDLE KeyHandle,
    IN PANSI_STRING ValueName,
    OUT PULONG Type,
    OUT PVOID Data,
    IN ULONG DataSize,
    OUT PULONG ResultLength
);

VEASTATUS
VEAPI
HvAllocateVolatileCell(
    PVK_HIVE Hive
);

VOID
VEAPI
VkpMarkCellDirty(
    IN PVK_HIVE Hive,
    IN ULONG CellOffset
);

PVOID
VEAPI
VkpGetCellPointer(
    PVK_HIVE Hive,
    ULONG CellOffset
);

PKEY_DIRECTORY_NODE
VEAPI
VkpFindSubkey(
    IN PVK_HIVE Hive,
    IN PKEY_DIRECTORY_NODE ParentNode,
    IN PCHAR SubkeyName,
    IN ULONG NameLen,
    OUT PULONG OutSubkeyOffset
);

PKEY_FILE_NODE
VEAPI
VkpFindValueNode(
    IN PVK_HIVE Hive,
    IN PKEY_DIRECTORY_NODE DirectoryNode,
    IN PANSI_STRING ValueName,
    OUT PULONG OutValueOffset OPTIONAL
);

VEASTATUS
VEAPI
HvAllocateStableCell(IN PVK_HIVE Hive);

PVOID
VEAPI
VkpAllocateVolatileCell(
    IN PVK_HIVE Hive,
    IN ULONG Size,
    OUT PULONG RawVolatileOffset
);

VEASTATUS
VEAPI
VkpAttachVolatileSubkey(
    IN PVK_HIVE Hive,
    IN ULONG ParentOffset,
    IN PANSI_STRING SubkeyName,
    OUT PULONG CreatedSubkeyOffset
);

VEASTATUS
VEAPI
VkpAttachVolatileKey(
    IN PVK_HIVE Hive,
    IN ULONG ParentOffset,
    IN PANSI_STRING ValueName,
    IN ULONG Type,
    IN PVOID Data,
    IN ULONG DataSize,
    OUT PULONG CreatedValueOffset
);

PVOID
VEAPI
VkpAllocateStableCell(
    IN PVK_HIVE Hive,
    IN ULONG Size,
    OUT PULONG OutStableOffset
);

VEASTATUS
VEAPI
VkpAttachStableSubkey(
    IN PVK_HIVE Hive,
    IN ULONG ParentOffset,
    IN PANSI_STRING SubkeyName,
    OUT PULONG CreatedSubkeyOffset
);

VEASTATUS
VEAPI
VkpAttachStableValueKey(
    IN PVK_HIVE Hive,
    IN ULONG ParentOffset,
    IN PANSI_STRING ValueName,
    IN ULONG Type,
    IN PVOID Data,
    IN ULONG DataSize,
    OUT PULONG CreatedValueOffset
);

VEASTATUS
VEAPI
VeaDumpKdPrint(
    IN HANDLE KeyHandle
);

VEASTATUS
VEAPI
VeaCreateKey(
    OUT PHANDLE KeyHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    IN ULONG TitleIndex,
    IN PANSI_STRING Class OPTIONAL,
    IN ULONG CreateOptions,
    OUT PULONG Disposition OPTIONAL
);

VEASTATUS
VEAPI
VeaSetValueKey(
    IN HANDLE KeyHandle,
    IN PANSI_STRING ValueName, // Nama File Node
    IN ULONG TitleIndex,
    IN ULONG Type,
    IN PVOID Data,
    IN ULONG DataSize
);

#define GET_HIVE_HEADER(Hive) ((PKEY_FILE_HEADER)((Hive)->BaseAddress))
#define GET_HIVE_BASEBLOCK(Hive) ((PKEY_BASE_BLOCK)((PUCHAR)(Hive)->BaseAddress + sizeof(KEY_FILE_HEADER)))

BOOLEAN 
VEAPI
VkInitSystemPhase1(VOID);