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
PVK_HIVE VkSamHive    = NULL;

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
    {
        return NULL;
    }

    ULONG CurrentOff = ParentNode->FirstSubKeyOffset;
    ULONG Index = 0;

    while (CurrentOff != 0)
    {
        // 1. Pake VkpGetCellPointer (Aman buat Stable & Volatile!)
        PKEY_DIRECTORY_NODE Node = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, CurrentOff);

        if (!Node || Node->Signature != KEY_DIR_SIG)
        {
            DPRINT("[VkFindSubkey] ERROR | Corrupted node at Off 0x%X (Node=%p, Sig=0x%X)\n",
                   CurrentOff, Node, Node ? Node->Signature : 0);
            break;
        }

        // 2. String Matching
        if (Node->NameLength == NameLen)
        {
            INT CmpRes = RtlRawStringCompareN(Node->Name, SubkeyName, NameLen);

            if (CmpRes == 0)
            {

                if (OutSubkeyOffset)
                    *OutSubkeyOffset = CurrentOff; // Return offset asli (Stable/Volatile)

                return Node; // Return Pointer Node-nya
            }
        }
        // 3. Next Sibling
        CurrentOff = Node->NextSiblingOffset;
        Index++;
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

BOOLEAN
VEAPI
VkpInitializeHiveFromBuffer(
    IN PVOID HiveBuffer,
    IN ULONG HiveLength,
    IN PCHAR HiveFilePath OPTIONAL,
    OUT PVK_HIVE *OutHive
)
{
    PKEY_FILE_HEADER Header;
    PKEY_BASE_BLOCK  BaseBlock;
    PKEY_DIRECTORY_NODE RootNode;

    if (!HiveBuffer || HiveLength < (0x1000 + sizeof(KEY_BASE_BLOCK) + sizeof(KEY_DIRECTORY_NODE)) || !OutHive)
    {
        return FALSE;
    }

    *OutHive = NULL;
    Header = (PKEY_FILE_HEADER)HiveBuffer;

    if (Header->Signature != KEY_HEADER_SIG)
    {
        KdPrintf("[Vk] ERROR: Hive corrupted. Header mismatch (Expected 'KEY1')\n");
        return FALSE;
    }

    if (Header->PrimarySequence != Header->SecondarySequence)
    {
        KdPrintf("[Vk] ERROR: Hive corrupted. PrimarySequence != SecondarySequence\n");
        return FALSE;
    }

    BaseBlock = (PKEY_BASE_BLOCK)((PUCHAR)HiveBuffer + 0x1000);
    if (BaseBlock->Signature != KEY_BIN_SIG)
    {
        KdPrintf("[Vk] ERROR: Hive corrupted. First BIN1 block mismatch\n");
        return FALSE;
    }

    PVOID KernelPoolHive = UlAllocatePoolWithTag(NonPagedPool, HiveLength, 'VkHv');
    if (!KernelPoolHive)
    {
        KdPrintf("[Vk] ERROR: Failed to allocate NonPagedPool for Hive (%u bytes)\n", HiveLength);
        return FALSE;
    }

    RtlCopyMemory(KernelPoolHive, HiveBuffer, HiveLength);

    PVK_HIVE NewHive = (PVK_HIVE)UlAllocatePoolWithTag(NonPagedPool, sizeof(VK_HIVE), 'VkHv');
    if (!NewHive)
    {
        UlFreePoolWithTag(KernelPoolHive, 'VkHv');
        return FALSE;
    }

    RtlZeroMemory(NewHive, sizeof(VK_HIVE));

    NewHive->Signature   = VKHIVE_SIG;
    NewHive->Length      = HiveLength;
    NewHive->BaseAddress = KernelPoolHive;
    NewHive->Header      = (PKEY_FILE_HEADER)KernelPoolHive;
    NewHive->BaseBlock   = (PKEY_BASE_BLOCK)((PUCHAR)KernelPoolHive + 0x1000);
    NewHive->FreeOffset  = HiveLength;

    NewHive->StableBins[0].BinBase       = KernelPoolHive;
    NewHive->StableBins[0].BinSize       = HiveLength;
    NewHive->StableBins[0].BinBaseOffset = 0;
    NewHive->StableBinCount              = 1;

    if (HiveFilePath != NULL)
    {
        ULONG PathLen = (ULONG)RtlRawStringLength(HiveFilePath);
        PCHAR PathCopy = (PCHAR)UlAllocatePoolWithTag(NonPagedPool, PathLen + 1, 'VkHv');
        if (PathCopy)
        {
            RtlCopyMemory(PathCopy, HiveFilePath, PathLen);
            PathCopy[PathLen] = '\0';
            NewHive->HiveFilePath.Buffer        = PathCopy;
            NewHive->HiveFilePath.Length        = (USHORT)PathLen;
            NewHive->HiveFilePath.MaximumLength = (USHORT)(PathLen + 1);
        }
    }

    ULONG TotalBlocks = HiveLength / 0x1000;
    NewHive->DirtyVectorSize = (TotalBlocks / 32) + 1;
    NewHive->DirtyVector = (PULONG)UlAllocatePoolWithTag(
        NonPagedPool,
        NewHive->DirtyVectorSize * sizeof(ULONG),
        'VkDv'
    );

    if (!NewHive->DirtyVector)
    {
        if (NewHive->HiveFilePath.Buffer) UlFreePoolWithTag(NewHive->HiveFilePath.Buffer, 'VkHv');
        UlFreePoolWithTag(NewHive, 'VkHv');
        UlFreePoolWithTag(KernelPoolHive, 'VkHv');
        return FALSE;
    }

    RtlZeroMemory(NewHive->DirtyVector, NewHive->DirtyVectorSize * sizeof(ULONG));

    RootNode = (PKEY_DIRECTORY_NODE)((PUCHAR)NewHive->BaseBlock + sizeof(KEY_BASE_BLOCK));
    if (RootNode->Signature != KEY_DIR_SIG)
    {
        KdPrintf("[Vk] ERROR: Root node signature mismatch! Expected 'DN'\n");
        UlFreePoolWithTag(NewHive->DirtyVector, 'VkDv');
        if (NewHive->HiveFilePath.Buffer) UlFreePoolWithTag(NewHive->HiveFilePath.Buffer, 'VkHv');
        UlFreePoolWithTag(NewHive, 'VkHv');
        UlFreePoolWithTag(KernelPoolHive, 'VkHv');
        return FALSE;
    }

    *OutHive = NewHive;
    return TRUE;
}

BOOLEAN
VEAPI
VkpMountHive(
    IN PVK_HIVE Hive,
    IN PCHAR MountPath
)
{
    VEASTATUS Status;
    OBJECT_ATTRIBUTES ObjAttr;
    ANSI_STRING KeyPath;
    PVK_KEY_OBJECT KeyObject = NULL;

    if (!Hive || !Hive->BaseBlock || !MountPath)
    {
        return FALSE;
    }

    PKEY_DIRECTORY_NODE RootNode = (PKEY_DIRECTORY_NODE)(
        (PUCHAR)Hive->BaseBlock + sizeof(KEY_BASE_BLOCK)
    );

    if (RootNode->Signature != KEY_DIR_SIG)
    {
        KdPrintf("[Vk] ERROR: Root node signature mismatch for %s\n", MountPath);
        return FALSE;
    }

    RtlInitAnsiString(&KeyPath, MountPath);

    RtlZeroMemory(&ObjAttr, sizeof(OBJECT_ATTRIBUTES));
    ObjAttr.ObjectName = &KeyPath;
    ObjAttr.Attributes = OBJ_PERMANENT;

    Status = ObCreateObject(
        KernelMode,
        VkKeyObjectType,
        &ObjAttr,
        KernelMode,
        NULL,
        sizeof(VK_KEY_OBJECT),
        0,
        0,
        (PVOID*)&KeyObject
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[Vk] ERROR: Failed to create KeyObject for %s (Status: 0x%X)\n", MountPath, Status);
        return FALSE;
    }

    RtlZeroMemory(KeyObject, sizeof(VK_KEY_OBJECT));
    KeyObject->HiveBase  = Hive;
    KeyObject->KeyNode   = RootNode;
    KeyObject->KeyOffset = (ULONG)((PUCHAR)RootNode - (PUCHAR)Hive->BaseAddress);
    RtlInitAnsiString(&KeyObject->FullKeyPath, MountPath);

    Status = ObInsertObject(
        KeyObject,
        NULL,
        0,
        0,
        NULL,
        NULL
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[Vk] ERROR: Failed to insert %s into Object Namespace (Status: 0x%X)\n", MountPath, Status);
        return FALSE;
    }

    KdPrintf("[Vk] Hive successfully mounted to %s\n", MountPath);
    return TRUE;
}

VEASTATUS
VEAPI
VkpLoadAndMountHiveFromFile(
    IN PCHAR FilePath,
    IN PCHAR MountPath,
    OUT PVK_HIVE *OutHive
)
{
    HANDLE FileHandle = NULL;
    OBJECT_ATTRIBUTES ObjAttr;
    OIP_INFORMATION IoStatus;
    ANSI_STRING FilePathStr;
    FILE_STANDARD_INFORMATION FileInfo;
    VEASTATUS Status;

    KdPrintf("[Vk] Loading hive '%s' -> '%s'\n", FilePath, MountPath);

    RtlInitAnsiString(&FilePathStr, FilePath);
    RtlZeroMemory(&ObjAttr, sizeof(OBJECT_ATTRIBUTES));
    RtlZeroMemory(&IoStatus, sizeof(IoStatus));
    ObjAttr.ObjectName = &FilePathStr;

    // 1. Buka file SAM.VKEY dari disk (sesuaikan nama syscall VFS kernel lu bila berbeda)
    Status = VeaOpenFile(
        &FileHandle,
        FILE_GENERIC_READ | VEA_SYNCHRONIZE,
        &ObjAttr,
        &IoStatus,
        FILE_SHARE_READ,
        FILE_SYNCHRONOUS_IO_NONALERT
    );
    KdPrintf("[Vk] VeaOpenFile: status=0x%X ioStatus=0x%X information=%u handle=%p\n",
             Status, IoStatus.Status, IoStatus.Information, FileHandle);
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[Vk] ERROR: Failed to open hive file %s (Status: 0x%X)\n", FilePath, Status);
        return Status;
    }

    // 2. Ambil ukuran file
    RtlZeroMemory(&FileInfo, sizeof(FileInfo));
    RtlZeroMemory(&IoStatus, sizeof(IoStatus));
    Status = ObioQueryInformationFileKernel(
        FileHandle,
        &IoStatus,
        &FileInfo,
        sizeof(FileInfo),
        FileStandardInformation
    );
    KdPrintf("[Vk] ObioQueryInformationFileKernel: status=0x%X ioStatus=0x%X information=%u eof=%u\n",
             Status, IoStatus.Status, IoStatus.Information, FileInfo.EndOfFile.LowPart);
    if (!VEA_SUCCESS(Status) || FileInfo.EndOfFile.LowPart == 0)
    {
        VeaClose(FileHandle);
        return Status;
    }

    ULONG FileSize = FileInfo.EndOfFile.LowPart;
    PVOID TempBuf = UlAllocatePoolWithTag(NonPagedPool, FileSize, 'VkTp');
    if (!TempBuf)
    {
        VeaClose(FileHandle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // 3. Baca seluruh isi SAM.VKEY ke buffer
    LARGE_INTEGER ByteOffset;
    ByteOffset.QuadPart = 0;
    RtlZeroMemory(&IoStatus, sizeof(IoStatus));

    Status = ObioReadFileKernel(
        FileHandle,
        &IoStatus,
        TempBuf,
        FileSize,
        &ByteOffset
    );
    KdPrintf("[Vk] ObioReadFileKernel: status=0x%X ioStatus=0x%X information=%u requested=%u\n",
             Status, IoStatus.Status, IoStatus.Information, FileSize);
    VeaClose(FileHandle);

    if (!VEA_SUCCESS(Status))
    {
        UlFreePoolWithTag(TempBuf, 'VkTp');
        return Status;
    }

    // 4. Validasi header & buat struktur PVK_HIVE
    if (!VkpInitializeHiveFromBuffer(TempBuf, FileSize, FilePath, OutHive))
    {
        KdPrintf("[Vk] VkpInitializeHiveFromBuffer failed for '%s' (%u bytes)\n",
                 FilePath, FileSize);
        UlFreePoolWithTag(TempBuf, 'VkTp');
        return STATUS_REGISTRY_CORRUPT;
    }

    UlFreePoolWithTag(TempBuf, 'VkTp');
    KdPrintf("[Vk] Hive validated: %p (%u bytes)\n", *OutHive, FileSize);

    // 5. Mount ke Object Manager Namespace (/REGISTRY/MACHINE/SAM)
    if (!VkpMountHive(*OutHive, MountPath))
    {
        KdPrintf("[Vk] VkpMountHive failed for '%s'\n", MountPath);
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

/* EOF */