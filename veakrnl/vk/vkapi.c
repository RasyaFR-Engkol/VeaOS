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

VOID
VEAPI
VkpDumpKeyRecursive(
    IN PVK_HIVE Hive,
    IN ULONG DirOffset,
    IN ULONG DepthLevel
);

/*
 * Open an existing VeaKey object and return a process handle for it.
 */
VEASTATUS
VEAPI
VeaOpenVeaKey(
    OUT PHANDLE KeyHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes
)
{
    PEPROCESS Process;
    PHANDLE_TABLE HandleTable;
    PVOID RootObject = NULL;
    OBJECT_ATTRIBUTES OpenAttributes;
    VEASTATUS Status;

    /* Validate the output and object-name arguments first. */
    if (!KeyHandle || !ObjectAttributes || !ObjectAttributes->ObjectName ||
        !ObjectAttributes->ObjectName->Buffer ||
        ObjectAttributes->ObjectName->Length == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *KeyHandle = NULL;

    RtlCopyMemory(&OpenAttributes, ObjectAttributes, sizeof(OBJECT_ATTRIBUTES));

    /* OBJECT_ATTRIBUTES stores a parent object pointer internally, while
       this public VeaKey API receives a parent handle from the caller. */
    if (ObjectAttributes->RootDirectory != NULL)
    {
        OpenAttributes.RootDirectory = ObjectAttributes->RootDirectory;
    }

    /* Use the object manager lookup path so this operation never creates a
       missing key and returns STATUS_OBJECT_NAME_NOT_FOUND instead. */
    Status = ObOpenObjectByName(
        &OpenAttributes,
        VkKeyObjectType,
        KernelMode,
        DesiredAccess,
        NULL,
        KeyHandle
    );

    /* Drop the temporary parent reference after lookup has completed. */
    if (RootObject != NULL)
    {
        ObDereferenceObject(RootObject);
    }

    return Status;
}

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

    Status = ObReferenceObjectByHandle(
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

VEASTATUS
VEAPI
VeaQueryKeyPath(
    IN HANDLE KeyHandle,
    OUT PANSI_STRING KeyPath
)
{
    PVK_KEY_OBJECT KeyObject = NULL;
    PEPROCESS CurrentProcess;
    VEASTATUS Status;

    if (!KeyPath)
    {
        return STATUS_INVALID_PARAMETER;
    }

    CurrentProcess = (PEPROCESS)PtGetCurrentProcess();
    if (!CurrentProcess || !CurrentProcess->ObjectTable)
    {
        return STATUS_INVALID_HANDLE;
    }

    Status = ObReferenceObjectByHandle(
        KeyHandle,
        KEY_QUERY_VALUE,
        VkKeyObjectType,
        KernelMode,
        (PVOID)&KeyObject
    );

    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    Status = VkpInitializeKeyObjectPath(KeyObject);
    if (VEA_SUCCESS(Status))
    {
        ULONG RequiredLength = KeyObject->FullKeyPath.Length + 1;

        /* Check KeyPath maximum length is big enough */
        if (!KeyPath->Buffer || KeyPath->MaximumLength < RequiredLength)
        {
            /* Report text length; caller must add one byte for '\0'. */
            KeyPath->Length = KeyObject->FullKeyPath.Length;
            KeyPath->MaximumLength = (USHORT)RequiredLength;
            ObDereferenceObject(KeyObject);
            return STATUS_BUFFER_TOO_SMALL;
        }

        /* Copy it */
        RtlCopyMemory(
            KeyPath->Buffer,
            KeyObject->FullKeyPath.Buffer,
            KeyObject->FullKeyPath.Length
        );
        KeyPath->Buffer[KeyObject->FullKeyPath.Length] = '\0';
        KeyPath->Length = KeyObject->FullKeyPath.Length;
    }

    ObDereferenceObject(KeyObject);
    return Status;
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
    Status = ObReferenceObjectByHandle(
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

    UNREFERENCED_PARAMETER(TitleIndex);
    UNREFERENCED_PARAMETER(Class);

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
    HANDLE OpenedParentHandle = NULL; // Untuk melacak handle temporary agar bisa ditutup

    if (ParentHandle != NULL)
    {
        Status = ObReferenceObjectByHandle(
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
                &ParentObjAttr,
                VkKeyObjectType,
                KernelMode,
                KEY_CREATE_SUB_KEY,
                NULL,
                &OpenedParentHandle
            );

            if (!VEA_SUCCESS(Status))
            {
                DPRINT("VeaCreateKey: ObOpenObjectByName failed: 0x%x\n", Status);
                return Status;
            }

            Status = ObReferenceObjectByHandle(
                OpenedParentHandle,
                0x10000,
                VkKeyObjectType,
                KernelMode,
                (PVOID*)&ParentKeyObject
            );

            if (!VEA_SUCCESS(Status))
            {
                DPRINT("VeaCreateKey: ObReferenceObjectByHandle failed: 0x%x\n", Status);
                ObCloseHandle(OpenedParentHandle, KernelMode);
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
                if (OpenedParentHandle) ObCloseHandle(OpenedParentHandle, KernelMode);
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
                    Status = VkpAttachVolatileSubkey(
                        Hive, 
                        ParentOffset, 
                        &LeafNameStr, 
                        &TargetSubkeyOffset
                    );
                }
                else
                {
                    Status = VkpAttachStableSubkey(
                        Hive, 
                        ParentOffset, 
                        &LeafNameStr, 
                        &TargetSubkeyOffset
                    );
                }

                if (Status == STATUS_OBJECT_NAME_COLLISION)
                {
                    Status = STATUS_SUCCESS;
                    IsNewKeyCreated = FALSE;
                }
                else if (!VEA_SUCCESS(Status))
                {
                    ObDereferenceObject(ParentKeyObject);
                    if (OpenedParentHandle) ObCloseHandle(OpenedParentHandle, KernelMode);
                    return Status;
                }
                else
                {
                    IsNewKeyCreated = TRUE;
                }
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
        if (OpenedParentHandle) ObCloseHandle(OpenedParentHandle, KernelMode);
        return Status;
    }

    // Lepas parent dereference & tutup handle parent sementara jika ada
    ObDereferenceObject(ParentKeyObject);
    if (OpenedParentHandle)
    {
        ObCloseHandle(OpenedParentHandle, KernelMode);
        OpenedParentHandle = NULL;
    }

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
        (PVOID*)&TargetKeyObject
    );

    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    RtlZeroMemory(TargetKeyObject, sizeof(VK_KEY_OBJECT));
    TargetKeyObject->HiveBase  = Hive;
    TargetKeyObject->KeyNode   = TargetDirNode;
    TargetKeyObject->KeyOffset = TargetSubkeyOffset;

    Status = VkpInitializeKeyObjectPath(TargetKeyObject);
    if (!VEA_SUCCESS(Status))
    {
        ObDereferenceObject(TargetKeyObject);
        return Status;
    }

    // Panggil ObInsertObject versi baru (6 parameter)
    // Parameter DesiredAccess & KeyHandle langsung dikirim ke ObInsertObject
    Status = ObInsertObject(
        TargetKeyObject,
        NULL,           // AccessState
        DesiredAccess,  // DesiredAccess
        0,              // ObjectPointerBias
        NULL,           // NewObject
        KeyHandle       // Target handle dikembalikan langsung via ObInsertObject
    );

    if (!VEA_SUCCESS(Status))
    {
        return Status;
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
    if (!KeyHandle || !ValueName || !ValueName->Buffer)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // =========================================================================
    // STEP 1: Ambil VK_KEY_OBJECT dari Handle Table
    // =========================================================================
    PVK_KEY_OBJECT KeyObject = NULL;
    VEASTATUS Status = ObReferenceObjectByHandle(
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
    ULONG ExistingNodeOffset = 0;
    PKEY_FILE_NODE ExistingFileNode =
        VkpFindValueNode(Hive, TargetNode, ValueName, &ExistingNodeOffset);

    if (ExistingFileNode != NULL)
    {
        BOOLEAN WasInline = (ExistingFileNode->Flags & KEY_FILE_INLINE) ||
                            ExistingFileNode->DataLength <= sizeof(ULONG);

        // (A) Data baru <= 4 byte: SELALU inline di DataPOffset
        if (DataSize <= sizeof(ULONG))
        {
            ULONG InlineData = 0;
            if (Data != NULL && DataSize > 0)
            {
                RtlCopyMemory(&InlineData, Data, DataSize);
            }
            ExistingFileNode->DataPOffset = InlineData;
            ExistingFileNode->DataLength  = DataSize;
            ExistingFileNode->Type        = Type;
            ExistingFileNode->Flags      |= KEY_FILE_INLINE;

            VkpMarkCellDirty(Hive, ExistingNodeOffset);
            ObDereferenceObject(KeyObject);
            return STATUS_SUCCESS;
        }

        if (Data == NULL)
        {
            ObDereferenceObject(KeyObject);
            return STATUS_INVALID_PARAMETER;
        }

        // (B) Data > 4 byte, value lama out-of-line dan cukup: timpa di tempat
        if (!WasInline && DataSize <= ExistingFileNode->DataLength)
        {
            PVOID ExistingDataCell = VkpGetCellPointer(Hive, ExistingFileNode->DataPOffset);
            if (ExistingDataCell)
            {
                RtlCopyMemory((PUCHAR)ExistingDataCell + sizeof(LONG), Data, DataSize);
                ExistingFileNode->DataLength = DataSize;
                ExistingFileNode->Type       = Type;

                VkpMarkCellDirty(Hive, ExistingFileNode->DataPOffset);
                VkpMarkCellDirty(Hive, ExistingNodeOffset);
                ObDereferenceObject(KeyObject);
                return STATUS_SUCCESS;
            }
        }

        // (C) Butuh cell data baru
        {
            ULONG NewDataOffset = 0;
            ULONG CellAllocSize = DataSize + sizeof(LONG);
            PVOID NewDataCell = IsVolatile
                ? VkpAllocateVolatileCell(Hive, CellAllocSize, &NewDataOffset)
                : VkpAllocateStableCell(Hive, CellAllocSize, &NewDataOffset);
                

            if (!NewDataCell)
            {
                ObDereferenceObject(KeyObject);
                return STATUS_INSUFFICIENT_MEMORY;
            }

            if (IsVolatile)
            {
                NewDataOffset = VkMakeVolatileOffset(NewDataOffset);   // <-- yang tadi hilang
            }

            *(PLONG)NewDataCell = -(LONG)CellAllocSize;
            RtlCopyMemory((PUCHAR)NewDataCell + sizeof(LONG), Data, DataSize);

            ExistingFileNode->DataPOffset = NewDataOffset;
            ExistingFileNode->DataLength  = DataSize;
            ExistingFileNode->Type        = Type;
            ExistingFileNode->Flags      &= ~KEY_FILE_INLINE;

            VkpMarkCellDirty(Hive, ExistingNodeOffset);
            ObDereferenceObject(KeyObject);
            return STATUS_SUCCESS;
        }
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

    RtlZeroMemory(FileNode, NodeAllocationSize);      // <-- pindah ke sini

    // STEP 3
    ULONG DataOffset = 0;
    ULONG InlineFlag = 0;

    if (DataSize <= sizeof(ULONG))
    {
        if (Data != NULL && DataSize > 0)
        {
            RtlCopyMemory(&DataOffset, Data, DataSize);
        }
        InlineFlag = KEY_FILE_INLINE;
    }
    else
    {
        if (Data == NULL)
        {
            ObDereferenceObject(KeyObject);
            return STATUS_INVALID_PARAMETER;
        }

        PVOID DataCellPtr = NULL;
        ULONG CellAllocSize = DataSize + sizeof(LONG);

        if (IsVolatile)
        {
            DataCellPtr = VkpAllocateVolatileCell(Hive, CellAllocSize, &DataOffset);
            if (DataCellPtr != NULL)
            {
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

        *(PLONG)DataCellPtr = -(LONG)CellAllocSize;
        RtlCopyMemory((PUCHAR)DataCellPtr + sizeof(LONG), Data, DataSize);
    }

    // STEP 4
    FileNode->CellSize        = -(LONG)NodeAllocationSize;
    FileNode->Signature       = KEY_FILE_SIG;
    FileNode->NextValueOffset = 0;
    FileNode->NameLength      = ValueName->Length;
    FileNode->DataPOffset     = DataOffset;
    FileNode->DataLength      = DataSize;
    FileNode->Type            = Type;
    FileNode->Flags           = (IsVolatile ? KEY_FILE_VOLATILE : KEY_FILE_DEFAULT) | InlineFlag;

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

