/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : vkapi.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Syscall VeaKey Subsystem
 ----- Effective since 07-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 30-07-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file vkapi.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
VeaQueryValueKey(
    IN HANDLE KeyHandle,
    IN PANSI_STRING ValueName,
    OUT PULONG Type,
    OUT PVOID Data,
    IN ULONG DataSize,
    OUT PULONG ResultLength
)
{
    PVK_KEY_OBJECT KeyObject = NULL;
    VEASTATUS Status;
    PEPROCESS CurrentProcess = (PEPROCESS)PtGetCurrentProcess();
    PHANDLE_TABLE HandleTable = CurrentProcess->ObjectTable;

    Status = ObReferenceObjectByHandle(
        HandleTable,
        KeyHandle,
        KEY_QUERY_VALUE,
        VkKeyObjectType,
        KernelMode,
        (PVOID)&KeyObject
    );

    if (!VEA_SUCCESS(Status)) {
        return Status;
    }

    Status = VkpQueryValueKey(KeyObject, ValueName, Type, Data, DataSize, ResultLength);

    ObDereferenceObject(KeyObject);

    return Status;
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

VEASTATUS
VEAPI
VeaDumpKdPrint(
    IN HANDLE KeyHandle
)
{
    PVK_KEY_OBJECT KeyObject = NULL;
    VEASTATUS Status;

    // 1. Resolve Handle dari User Mode / Kernel Mode ke Object Manager
    // **FIXME
    PEPROCESS Process = (PEPROCESS)PtGetCurrentProcess();
    PHANDLE_TABLE Table = Process->ObjectTable;
    Status = ObReferenceObjectByHandle(
        Table,
        KeyHandle,               
        KEY_READ_ACCESS,        // Expected Object Type
        VkKeyObjectType,    // Validate UserMode / KernelMode Boundary
        KernelMode,
        (PVOID*)&KeyObject
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[Vk] ERROR | VeaDumpKdPrint received invalid handle!\n");
        return Status;
    }

    // 2. Extract Hive & Node dari VK_KEY_OBJECT
    PVK_HIVE Hive = KeyObject->HiveBase;
    ULONG TargetOffset = KeyObject->KeyOffset;

    if (!Hive || TargetOffset == 0)
    {
        ObDereferenceObject(KeyObject);
        return STATUS_INVALID_HANDLE;
    }

    // 3. Print Header Information
    KdPrintf("\n==================================================\n");
    KdPrintf("          VEAOS REGISTRY DUMP (SYSCALL)           \n");
    KdPrintf("==================================================\n");
    KdPrintf("Hive Path    : %s\n", Hive->HiveFilePath.Buffer ? Hive->HiveFilePath.Buffer : "Volatile-Only");
    KdPrintf("Dump Root Off: 0x%X\n", TargetOffset);
    KdPrintf("Volatile Bins: %u Bins\n", Hive->VolatileBinCount);
    KdPrintf("--------------------------------------------------\n");

    // 4. Panggil Helper Rekursif Internal (Vkp...)
    VkpDumpKeyRecursive(Hive, TargetOffset, 0);

    KdPrintf("==================================================\n\n");

    // 5. Release Object Reference
    ObDereferenceObject(KeyObject);

    return STATUS_SUCCESS;
}

static VEASTATUS
VkpEnsureKeyPathRecursive(
    IN PVK_HIVE Hive,
    IN ULONG RootOffset,
    IN PANSI_STRING RelativePath,
    IN ULONG CreateOptions,
    OUT PULONG FinalKeyOffset,
    OUT PBOOLEAN KeyCreated
)
{
    VEASTATUS Status;
    ULONG CurrentOffset = RootOffset;
    BOOLEAN CreatedAny = FALSE;

    if (!Hive || RootOffset == 0 || !RelativePath || !RelativePath->Buffer || RelativePath->Length == 0 ||
        !FinalKeyOffset || !KeyCreated)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *FinalKeyOffset = RootOffset;
    *KeyCreated = FALSE;

    PKEY_DIRECTORY_NODE CurrentNode = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, RootOffset);
    if (!CurrentNode || CurrentNode->Signature != KEY_DIR_SIG)
    {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    CHAR PathBuffer[256];
    ULONG PathLen = RelativePath->Length;
    if (PathLen >= sizeof(PathBuffer))
    {
        return STATUS_NAME_TOO_LONG;
    }

    RtlZeroMemory(PathBuffer, sizeof(PathBuffer));
    RtlCopyMemory(PathBuffer, RelativePath->Buffer, PathLen);
    PathBuffer[PathLen] = '\0';

    PCHAR Cursor = PathBuffer;
    while (*Cursor == '/')
    {
        Cursor++;
    }

    while (*Cursor != '\0')
    {
        while (*Cursor == '/')
        {
            Cursor++;
        }

        if (*Cursor == '\0')
        {
            break;
        }

        PCHAR TokenStart = Cursor;
        while (*Cursor != '\0' && *Cursor != '/')
        {
            Cursor++;
        }

        ULONG TokenLen = (ULONG)(Cursor - TokenStart);
        if (TokenLen == 0)
        {
            continue;
        }

        CHAR TokenBuffer[128];
        if (TokenLen >= sizeof(TokenBuffer))
        {
            return STATUS_NAME_TOO_LONG;
        }

        RtlZeroMemory(TokenBuffer, sizeof(TokenBuffer));
        RtlCopyMemory(TokenBuffer, TokenStart, TokenLen);

        ANSI_STRING TokenName;
        TokenName.Buffer = TokenBuffer;
        TokenName.Length = (USHORT)TokenLen;
        TokenName.MaximumLength = (USHORT)TokenLen;

        ULONG ChildOffset = 0;
        PKEY_DIRECTORY_NODE ExistingNode = VkpFindSubkey(
            Hive,
            CurrentNode,
            TokenBuffer,
            TokenLen,
            &ChildOffset
        );

        if (!ExistingNode)
        {
            if (CreateOptions & REG_OPTION_VOLATILE)
            {
                Status = VkpAttachVolatileSubkey(Hive, CurrentOffset, &TokenName, &ChildOffset);
            }
            else
            {
                Status = VkpAttachStableSubkey(Hive, CurrentOffset, &TokenName, &ChildOffset);
            }

            if (!VEA_SUCCESS(Status))
            {
                return Status;
            }

            CreatedAny = TRUE;
        }

        CurrentOffset = ChildOffset;
        *FinalKeyOffset = CurrentOffset;

        CurrentNode = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, CurrentOffset);
        if (!CurrentNode || CurrentNode->Signature != KEY_DIR_SIG)
        {
            return STATUS_OBJECT_TYPE_MISMATCH;
        }
    }

    *KeyCreated = CreatedAny;
    return STATUS_SUCCESS;
}

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
)
{
    VEASTATUS Status;
    PEPROCESS Process = (PEPROCESS)PtGetCurrentProcess();
    PHANDLE_TABLE Table = Process->ObjectTable;

    if (!KeyHandle || !ObjectAttributes || !ObjectAttributes->ObjectName)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PCHAR FullPath = ObjectAttributes->ObjectName->Buffer;
    USHORT PathLen = ObjectAttributes->ObjectName->Length;

    if (!FullPath || PathLen == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    ANSI_STRING RelativePathStr;
    CHAR RelativePathBuf[256];
    ULONG RelativePathLen = 0;

    if (FullPath[0] == '/')
    {
        RelativePathLen = PathLen - 1;
        if (RelativePathLen > sizeof(RelativePathBuf) - 1)
        {
            return STATUS_NAME_TOO_LONG;
        }

        RtlZeroMemory(RelativePathBuf, sizeof(RelativePathBuf));
        RtlCopyMemory(RelativePathBuf, FullPath + 1, RelativePathLen);
    }
    else
    {
        RelativePathLen = PathLen;
        if (RelativePathLen > sizeof(RelativePathBuf) - 1)
        {
            return STATUS_NAME_TOO_LONG;
        }

        RtlZeroMemory(RelativePathBuf, sizeof(RelativePathBuf));
        RtlCopyMemory(RelativePathBuf, FullPath, RelativePathLen);
    }

    RelativePathStr.Buffer = RelativePathBuf;
    RelativePathStr.Length = (USHORT)RelativePathLen;
    RelativePathStr.MaximumLength = (USHORT)RelativePathLen;

    PVK_KEY_OBJECT ParentKeyObject = NULL;
    HANDLE ParentHandle = ObjectAttributes->RootDirectory;
    BOOLEAN ParentIsBorrowed = FALSE;

    if (ParentHandle != NULL)
    {
        Status = ObReferenceObjectByHandle(
            Table,
            ParentHandle,
            0x10000,
            VkKeyObjectType,
            KernelMode,
            (PVOID*)&ParentKeyObject
        );

        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }

        ParentIsBorrowed = TRUE;
    }
    else
    {
        LONG LastSlashIndex = -1;
        for (LONG i = (LONG)PathLen - 1; i >= 0; i--)
        {
            if (FullPath[i] == '/')
            {
                LastSlashIndex = i;
                break;
            }
        }

        if (LastSlashIndex > 0)
        {
            CHAR ParentPathBuf[256];
            ULONG ParentPathLen = (ULONG)LastSlashIndex;
            if (ParentPathLen >= sizeof(ParentPathBuf))
            {
                return STATUS_NAME_TOO_LONG;
            }

            RtlZeroMemory(ParentPathBuf, sizeof(ParentPathBuf));
            RtlCopyMemory(ParentPathBuf, FullPath, ParentPathLen);

            ANSI_STRING ParentPathStr;
            ParentPathStr.Buffer = ParentPathBuf;
            ParentPathStr.Length = (USHORT)ParentPathLen;
            ParentPathStr.MaximumLength = (USHORT)ParentPathLen;

            OBJECT_ATTRIBUTES ParentObjAttr;
            RtlZeroMemory(&ParentObjAttr, sizeof(OBJECT_ATTRIBUTES));
            ParentObjAttr.ObjectName = &ParentPathStr;
            ParentObjAttr.RootDirectory = NULL;

            Status = ObOpenObjectByName(
                Table,
                &ParentObjAttr,
                VkKeyObjectType,
                KernelMode,
                KEY_CREATE_SUB_KEY,
                &ParentHandle
            );

            if (!VEA_SUCCESS(Status))
            {
                return Status;
            }

            Status = ObReferenceObjectByHandle(
                Table,
                ParentHandle,
                0x10000,
                VkKeyObjectType,
                KernelMode,
                (PVOID*)&ParentKeyObject
            );

            if (!VEA_SUCCESS(Status))
            {
                return Status;
            }
        }
        else
        {
            return STATUS_OBJECT_PATH_INVALID;
        }
    }

    PVK_HIVE Hive = ParentKeyObject->HiveBase;
    ULONG ParentOffset = ParentKeyObject->KeyOffset;
    PKEY_DIRECTORY_NODE ParentNode = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, ParentOffset);
    ParentKeyObject->KeyNode = ParentNode;

    ULONG TargetSubkeyOffset = 0;
    BOOLEAN IsNewKeyCreated = FALSE;

    if (ObjectAttributes->RootDirectory != NULL)
    {
        Status = VkpEnsureKeyPathRecursive(
            Hive,
            ParentOffset,
            &RelativePathStr,
            CreateOptions,
            &TargetSubkeyOffset,
            &IsNewKeyCreated
        );
    }
    else
    {
        CHAR LeafNameBuf[128];
        ULONG LeafNameLen = 0;
        LONG LastSlashIndex = -1;

        for (LONG i = (LONG)PathLen - 1; i >= 0; i--)
        {
            if (FullPath[i] == '/')
            {
                LastSlashIndex = i;
                break;
            }
        }

        if (LastSlashIndex >= 0)
        {
            LeafNameLen = (ULONG)(PathLen - (LastSlashIndex + 1));
            if (LeafNameLen >= sizeof(LeafNameBuf))
            {
                ObDereferenceObject(ParentKeyObject);
                return STATUS_NAME_TOO_LONG;
            }

            RtlZeroMemory(LeafNameBuf, sizeof(LeafNameBuf));
            RtlCopyMemory(LeafNameBuf, FullPath + LastSlashIndex + 1, LeafNameLen);

            ANSI_STRING LeafNameStr;
            LeafNameStr.Buffer = LeafNameBuf;
            LeafNameStr.Length = (USHORT)LeafNameLen;
            LeafNameStr.MaximumLength = (USHORT)LeafNameLen;

            if (LeafNameLen > 0)
            {
                if (CreateOptions & REG_OPTION_VOLATILE)
                {
                    Status = VkpAttachVolatileSubkey(Hive, ParentOffset, &LeafNameStr, &TargetSubkeyOffset);
                }
                else
                {
                    Status = VkpAttachStableSubkey(Hive, ParentOffset, &LeafNameStr, &TargetSubkeyOffset);
                }

                if (!VEA_SUCCESS(Status))
                {
                    ObDereferenceObject(ParentKeyObject);
                    return Status;
                }

                IsNewKeyCreated = TRUE;
            }
            else
            {
                Status = STATUS_OBJECT_PATH_INVALID;
            }
        }
        else
        {
            Status = STATUS_OBJECT_PATH_INVALID;
        }
    }

    if (!VEA_SUCCESS(Status))
    {
        ObDereferenceObject(ParentKeyObject);
        return Status;
    }

    ObDereferenceObject(ParentKeyObject);

    PKEY_DIRECTORY_NODE TargetDirNode = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, TargetSubkeyOffset);
    PVK_KEY_OBJECT TargetKeyObject = NULL;

    Status = ObCreateObject(
        KernelMode,
        VkKeyObjectType,
        ObjectAttributes,
        KernelMode,
        NULL,
        sizeof(VK_KEY_OBJECT),
        0,
        0,
        (PVOID)&TargetKeyObject
    );

    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    RtlZeroMemory(TargetKeyObject, sizeof(VK_KEY_OBJECT));
    TargetKeyObject->HiveBase  = Hive;
    TargetKeyObject->KeyNode   = TargetDirNode;
    TargetKeyObject->KeyOffset = TargetSubkeyOffset;

    Status = ObInsertObject((PVOID)TargetKeyObject, NULL);
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    if (KeyHandle != NULL)
    {
        *KeyHandle = UlCreateHandle(Table, TargetKeyObject, KEY_ALL_ACCESS);
    }

    if (Disposition != NULL)
    {
        *Disposition = IsNewKeyCreated ? REG_CREATED_NEW_KEY : REG_OPENED_EXISTING_KEY;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VeaSetValueKey(
    IN HANDLE KeyHandle,
    IN PANSI_STRING ValueName, // Nama File Node
    IN ULONG TitleIndex,
    IN ULONG Type,
    IN PVOID Data,
    IN ULONG DataSize
)
{
    PEPROCESS Process = (PEPROCESS)PtGetCurrentProcess();
    PHANDLE_TABLE Table = Process->ObjectTable;

    if (!KeyHandle || !ValueName || !ValueName->Buffer)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // =========================================================================
    // STEP 1: Ambil VK_KEY_OBJECT dari Handle Table
    // =========================================================================
    PVK_KEY_OBJECT KeyObject = NULL;
    VEASTATUS Status = ObReferenceObjectByHandle(
        Table,
        KeyHandle,
        KEY_SET_VALUE,
        VkKeyObjectType,
        KernelMode,
        (PVOID*)&KeyObject
    );

    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    PVK_HIVE Hive = KeyObject->HiveBase;
    PKEY_DIRECTORY_NODE TargetNode = KeyObject->KeyNode;

    // Cek apakah Key Parent ini Volatile (lewat Flag DIR_VOLATILE atau Offset Masking)
    BOOLEAN IsVolatile = (VkIsVolatileOffset(KeyObject->KeyOffset) || 
                         (TargetNode->Flags & KEY_DIR_VOLATILE)) ? TRUE : FALSE;

    // Check duplicate. after check, we should update data in place
    PKEY_FILE_NODE ExistingFileNode = VkpFindValueNode(Hive, TargetNode, ValueName, NULL);

    if (ExistingFileNode != NULL)
    {
        // Update In Place logic
        if (DataSize <= ExistingFileNode->DataLength && DataSize > 0 && Data != NULL)
        {
            // Cukup di data cell lama (asalkan gak lebih gede) -> reuse
            PVOID ExistingDataCell = VkpGetCellPointer(Hive, ExistingFileNode->DataPOffset);
            if (ExistingDataCell)
            {
                RtlCopyMemory((PUCHAR)ExistingDataCell + sizeof(LONG), Data, DataSize);
                ExistingFileNode->DataLength = DataSize;
                ExistingFileNode->Type = Type;
                ObDereferenceObject(KeyObject);
                return STATUS_SUCCESS;
            }
        }

        // Data lebih gede dari cell lama (atau kondisi lain) -> alokasi cell data baru,
        // tapi TETAP reuse FileNode yang sama (jangan bikin FileNode baru / jangan
        // relink linked list)
        ULONG NewDataOffset = 0;
        if (DataSize > 0 && Data != NULL)
        {
            ULONG CellAllocSize = DataSize + sizeof(LONG);
            PVOID NewDataCell = IsVolatile
                ? VkpAllocateVolatileCell(Hive, CellAllocSize, &NewDataOffset)
                : VkpAllocateStableCell(Hive, CellAllocSize, &NewDataOffset);

            if (!NewDataCell)
            {
                ObDereferenceObject(KeyObject);
                return STATUS_INSUFFICIENT_MEMORY;
            }

            *(PLONG)NewDataCell = -(LONG)CellAllocSize;
            RtlCopyMemory((PUCHAR)NewDataCell + sizeof(LONG), Data, DataSize);
        }

        ExistingFileNode->DataPOffset = NewDataOffset;
        ExistingFileNode->DataLength  = DataSize;
        ExistingFileNode->Type        = Type;

        VkpMarkCellDirty(Hive, KeyObject->KeyOffset);
        ObDereferenceObject(KeyObject);
        return STATUS_SUCCESS;
    }

    // =========================================================================
    // STEP 2: Alokasikan Cell untuk KEY_FILE_NODE
    // =========================================================================
    ULONG FileNodeOffset = 0;
    PKEY_FILE_NODE FileNode = NULL;
    ULONG NodeAllocationSize = sizeof(KEY_FILE_NODE) + ValueName->Length;

    if (IsVolatile)
    {
        FileNode = (PKEY_FILE_NODE)VkpAllocateVolatileCell(Hive, NodeAllocationSize, &FileNodeOffset);
        if (FileNode != NULL)
        {
            // PERBAIKAN: Ubah raw offset menjadi volatile offset (set MSB)
            FileNodeOffset = VkMakeVolatileOffset(FileNodeOffset);
        }
    }
    else
    {
        FileNode = (PKEY_FILE_NODE)VkpAllocateStableCell(Hive, NodeAllocationSize, &FileNodeOffset);
    }

    if (!FileNode || FileNodeOffset == 0 || FileNodeOffset == 0xFFFFFFFF)
    {
        ObDereferenceObject(KeyObject);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    // =========================================================================
    // STEP 3: Alokasikan Cell untuk Payload Data (Jika Ada)
    // =========================================================================
    ULONG DataOffset = 0;
    if (DataSize > 0 && Data != NULL)
    {
        PVOID DataCellPtr = NULL;
        ULONG CellAllocSize = DataSize + sizeof(LONG); // Reservasi 4 Byte Header

        if (IsVolatile)
        {
            DataCellPtr = VkpAllocateVolatileCell(Hive, CellAllocSize, &DataOffset);
            if (DataCellPtr != NULL)
            {
                // PERBAIKAN: Ubah raw offset menjadi volatile offset (set MSB)
                DataOffset = VkMakeVolatileOffset(DataOffset);
            }
        }
        else
        {
            DataCellPtr = VkpAllocateStableCell(Hive, CellAllocSize, &DataOffset);
        }

        if (!DataCellPtr)
        {
            ObDereferenceObject(KeyObject);
            return STATUS_INSUFFICIENT_MEMORY;
        }

        // Set Cell Header (Negatif = Terpakai)
        *(PLONG)DataCellPtr = -(LONG)CellAllocSize;
        RtlCopyMemory((PUCHAR)DataCellPtr + sizeof(LONG), Data, DataSize);
    }

    // =========================================================================
    // STEP 4: Isi Field Struktur KEY_FILE_NODE
    // =========================================================================
    FileNode->CellSize        = -(LONG)NodeAllocationSize;
    FileNode->Signature       = KEY_FILE_SIG;
    FileNode->NextValueOffset = 0;             // NULL offset (ujung list)
    FileNode->NameLength      = ValueName->Length;
    FileNode->DataPOffset     = DataOffset;
    FileNode->DataLength      = DataSize;
    FileNode->Type            = Type;
    FileNode->Flags           = IsVolatile ? KEY_FILE_VOLATILE : KEY_FILE_DEFAULT;

    // Copy string Nama File Node ke Flexible Array Member
    RtlCopyMemory(FileNode->Name, ValueName->Buffer, ValueName->Length);

    TargetNode = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, KeyObject->KeyOffset);
    KeyObject->KeyNode = TargetNode;

    // =========================================================================
    // STEP 5: Attach File Node ke Single Linked List milik Target Directory Node
    // =========================================================================
    if (TargetNode->FirstValueOffset == 0)
    {
        // Jika belum punya File Node sama sekali, pasang di head
        TargetNode->FirstValueOffset = FileNodeOffset;
    }
    else
    {
        // Traverse linked list lewat NextValueOffset sampai ketemu ekornya
        ULONG CurrentOffset = TargetNode->FirstValueOffset;
        PKEY_FILE_NODE CurrentFileNode = NULL;

        while (CurrentOffset != 0)
        {
            CurrentFileNode = (PKEY_FILE_NODE)VkpGetCellPointer(Hive, CurrentOffset);
            if (!CurrentFileNode)
            {
                break;
            }

            if (CurrentFileNode->NextValueOffset == 0)
            {
                // Sambungkan ke ekor
                CurrentFileNode->NextValueOffset = FileNodeOffset;
                break;
            }

            CurrentOffset = CurrentFileNode->NextValueOffset;
        }
    }

    ObDereferenceObject(KeyObject);
    return STATUS_SUCCESS;
}
