/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : vkdump.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VK Dump Debug Purpose
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 19-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file vkdump.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
VkDumpTree(VOID)
{
    HANDLE KeyHandle;
    VEASTATUS Status;
    OBJECT_ATTRIBUTES ObjAttr;
    ANSI_STRING KeyString;

    RtlInitAnsiString(&KeyString, "/REGISTRY/MACHINE/SYSTEM");
    InitializeObjectAttributes(&ObjAttr, &KeyString, 0, NULL, NULL);

    Status = ObOpenObjectByName(
        &ObjAttr,
        VkKeyObjectType,
        KernelMode,
        KEY_ALL_ACCESS,
        NULL,
        &KeyHandle
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("VkDumpTree failed\n");
        return;
    }

    VeaDumpKdPrint(KeyHandle);
}

//
// to print indent
//
VOID
VEAPI
VkpPrintIndent(
    IN ULONG Level
)
{
    for (ULONG i = 0; i < Level; i++)
    {
        KdPrintf("  "); // 2 spasi per level
    }
}

//
// Routine to dump
//
VOID
VEAPI
VkpPrintValueData(
    IN PVK_HIVE Hive,
    IN PKEY_FILE_NODE ValNode,
    IN ULONG DepthLevel
)
{
    if (ValNode->DataLength == 0)
    {
        KdPrintf(" = (empty)\n");
        return;
    }

    PVOID DataPtr = NULL;

    // -------------------------------------------------------------------------
    // 1. CEK INLINE VS OUT-OF-LINE DATA
    // -------------------------------------------------------------------------
    if ((ValNode->Flags & KEY_FILE_INLINE) || ValNode->DataLength <= sizeof(ULONG))
    {
        // Inline Data: Data tersimpan langsung di dalam field DataPOffset
        DataPtr = &ValNode->DataPOffset;
    }
    else
    {
        // Out-of-line Data: DataPOffset adalah Cell Offset
        PUCHAR CellPtr = (PUCHAR)VkpGetCellPointer(Hive, ValNode->DataPOffset);
        if (!CellPtr)
        {
            KdPrintf(" = <!DATA CELL INVALID! Off: 0x%X>\n", ValNode->DataPOffset);
            return;
        }

        // LEWATI 4 BYTE Header CellSize (sizeof(LONG))!
        DataPtr = (PVOID)(CellPtr + sizeof(LONG));
    }

    // -------------------------------------------------------------------------
    // 2. PRINT SESUAI TIPE DATA
    // -------------------------------------------------------------------------
    switch (ValNode->Type)
    {
        case FILE_KEY_STRING:
        case FILE_KEY_EXPAND_STRING:
        case FILE_KEY_LINK:
        {
            CHAR StrBuf[256];
            USHORT Len = (ValNode->DataLength < 255) ? (USHORT)ValNode->DataLength : 255;
            RtlCopyMemory(StrBuf, DataPtr, Len);
            StrBuf[Len] = '\0';
            KdPrintf(" = \"%s\"\n", StrBuf);
            break;
        }

        case FILE_KEY_DWORD:
        {
            ULONG Val = *(PULONG)DataPtr;
            KdPrintf(" = 0x%X (%u)\n", Val, Val);
            break;
        }

        case FILE_KEY_QWORD:
        {
            ULONGLONG Val = *(PULONGLONG)DataPtr;
            KdPrintf(" = 0x%X\n", Val);
            break;
        }

        case FILE_KEY_BOOLEAN:
        {
            UCHAR Val = *(PUCHAR)DataPtr;
            KdPrintf(" = %s\n", Val ? "TRUE" : "FALSE");
            break;
        }

        case FILE_KEY_MULTI_STRING:
        {
            KdPrintf("\n");
            PUCHAR Cursor = (PUCHAR)DataPtr;
            PUCHAR End = Cursor + ValNode->DataLength;

            while (Cursor < End && *Cursor != '\0')
            {
                CHAR SubStr[128];
                ULONG SubLen = 0;
                while (Cursor + SubLen < End && Cursor[SubLen] != '\0' && SubLen < 127)
                {
                    SubLen++;
                }
                RtlCopyMemory(SubStr, Cursor, SubLen);
                SubStr[SubLen] = '\0';

                VkpPrintIndent(DepthLevel + 2);
                KdPrintf("* \"%s\"\n", SubStr);

                Cursor += SubLen + 1;
            }
            break;
        }

        case FILE_KEY_BINARY:
        case FILE_KEY_GUID:
        default:
        {
            KdPrintf(" = ");
            PUCHAR Bytes = (PUCHAR)DataPtr;
            ULONG PrintLen = (ValNode->DataLength < 16) ? ValNode->DataLength : 16;

            for (ULONG i = 0; i < PrintLen; i++)
            {
                KdPrintf("%X ", Bytes[i]);
            }
            if (ValNode->DataLength > 16)
            {
                KdPrintf("... (+%u bytes)", ValNode->DataLength - 16);
            }
            KdPrintf("\n");
            break;
        }
    }
}

VOID
VEAPI
VkpDumpKeyRecursive(
    IN PVK_HIVE Hive,
    IN ULONG DirOffset,
    IN ULONG DepthLevel
)
{
    if (!Hive || DirOffset == 0) return;

    PKEY_DIRECTORY_NODE DirNode = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, DirOffset);

    if (!DirNode || DirNode->Signature != KEY_DIR_SIG)
    {
        VkpPrintIndent(DepthLevel);
        KdPrintf("[!] ERROR | Corrupted Directory Cell at Offset 0x%X\n", DirOffset);
        return;
    }

    // -------------------------------------------------------------------------
    // 1. COPY & NULL-TERMINATE DIRECTORY NAME
    // -------------------------------------------------------------------------
    CHAR DirName[256];
    USHORT DirNameLen = (DirNode->NameLength < 255) ? DirNode->NameLength : 255;
    RtlCopyMemory(DirName, DirNode->Name, DirNameLen);
    DirName[DirNameLen] = '\0'; // Paksa kasih Null-Terminator!

    // PRINT DIRECTORY KEY ('DN')
    // Urutan specifier (%s, %u, 0x%X, 0x%X) disesuaikan persis dengan argumennya!
    VkpPrintIndent(DepthLevel);
    KdPrintf("+ [%s]%s (Subkeys: %u, Flags: 0x%X, Off: 0x%X)\n", 
             DirName, 
            (DirNode->Flags & KEY_DIR_SYMLINK) ? " -> SYMLINK" : "",
             DirNode->SubKeyCount,
             DirNode->Flags,
             DirOffset);

    // -------------------------------------------------------------------------
    // 2. DUMP VALUES ('FN')
    // -------------------------------------------------------------------------
    ULONG ValOffset = DirNode->FirstValueOffset;
    while (ValOffset != 0)
    {
        PKEY_FILE_NODE ValNode = (PKEY_FILE_NODE)VkpGetCellPointer(Hive, ValOffset);

        if (!ValNode || ValNode->Signature != KEY_FILE_SIG)
        {
            VkpPrintIndent(DepthLevel + 1);
            KdPrintf("[!] ERROR | Corrupted Value Cell at Offset 0x%X\n", ValOffset);
            break;
        }

        // COPY & NULL-TERMINATE VALUE NAME
        CHAR ValName[256];
        USHORT ValNameLen = (ValNode->NameLength < 255) ? ValNode->NameLength : 255;
        RtlCopyMemory(ValName, ValNode->Name, ValNameLen);
        ValName[ValNameLen] = '\0'; // Paksa kasih Null-Terminator!

        VkpPrintIndent(DepthLevel + 1);
        KdPrintf("  - %s [Type: %u, Size: %u Bytes] ",
                 ValName,
                 ValNode->Type,
                 ValNode->DataLength);

        VkpPrintValueData(Hive, ValNode, DepthLevel);

        ValOffset = ValNode->NextValueOffset;
    }

    // -------------------------------------------------------------------------
    // 3. DUMP SUBKEYS RECURSIVE
    // -------------------------------------------------------------------------
    ULONG ChildOffset = DirNode->FirstSubKeyOffset;
    while (ChildOffset != 0)
    {
        VkpDumpKeyRecursive(Hive, ChildOffset, DepthLevel + 1);

        PKEY_DIRECTORY_NODE ChildNode = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, ChildOffset);
        if (!ChildNode || ChildNode->Signature != KEY_DIR_SIG)
        {
            break; 
        }

        ChildOffset = ChildNode->NextSiblingOffset;
    }
}
