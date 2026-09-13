/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : vkinit.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Initializing VeaKey Subsystem
 ----- Effective since 07-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 30-07-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file vkinit.c
 * --------------------------------------------------------------------- */

POBJECT_TYPE VkKeyObjectType;

BOOLEAN 
VEAPI
VkpInitializeSystemHive(
    IN PVOID SystemHive,
    IN ULONG HiveLength
)
{
    PKEY_FILE_HEADER Header;
    PKEY_BASE_BLOCK  BaseBlock;
    PKEY_DIRECTORY_NODE RootNode;

    // Check if VKEY is valid
    Header = (PKEY_FILE_HEADER)SystemHive;

    // Header must KEY1
    if(Header->Signature != KEY_HEADER_SIG)
    {
        KdPrintf("ERROR: Registry corrupted. Header mismatch\n");
        return FALSE;
    }

    // Sequence must SAME
    if(Header->PrimarySequence != Header->SecondarySequence)
    {
        KdPrintf("ERROR: Registry corrupted. Primary != Secondary\n");
        return FALSE;
    }

    // Check our BaseBlock
    BaseBlock = (PKEY_BASE_BLOCK)((PUCHAR)SystemHive + 0x1000);
    if (BaseBlock->Signature != KEY_BIN_SIG)
    {
        KdPrintf("ERROR: Registry corrupted. First BIN block mismatch\n");
        return FALSE;
    }

    PVOID KernelPoolHive = UlAllocatePoolWithTag(NonPagedPool, HiveLength, 'VkHv');
    if (!KernelPoolHive) {
        KdPrintf("ERROR: Failed to allocate NonPagedPool for System Hive deep copy.\n");
        return FALSE;
    }

    RtlCopyMemory(KernelPoolHive, SystemHive, HiveLength);

    // Allocate VkSystemHive so it's on our RAM
    VkSystemHive = (PVK_HIVE)UlAllocatePoolWithTag(NonPagedPool, sizeof(VK_HIVE), 'VkHv');
    if(!VkSystemHive)
    {
        KdPrintf("ERROR: Registry failed to allocate memory for GlobalHive.\n");
        UlFreePoolWithTag(KernelPoolHive, 'VkHv'); // Cleanup jika gagal
        return FALSE;
    }

    RtlZeroMemory(VkSystemHive, sizeof(VK_HIVE));

    // FIll out the crucial information
    VkSystemHive->Signature = VKHIVE_SIG;
    VkSystemHive->Length = HiveLength;
    VkSystemHive->BaseAddress = KernelPoolHive; 
    VkSystemHive->Header = (PKEY_FILE_HEADER)KernelPoolHive;
    VkSystemHive->BaseBlock = (PKEY_BASE_BLOCK)((PUCHAR)KernelPoolHive + 0x1000);
    VkSystemHive->FreeOffset  = HiveLength;

    VkSystemHive->StableBins[0].BinBase       = KernelPoolHive;
    VkSystemHive->StableBins[0].BinSize       = HiveLength;
    VkSystemHive->StableBins[0].BinBaseOffset = 0;
    VkSystemHive->StableBinCount              = 1;

    // Allocate our DirtyHive cell
    ULONG TotalBlocks = HiveLength / 0x1000;
    VkSystemHive->DirtyVectorSize = (TotalBlocks / 32) + 1;
    VkSystemHive->DirtyVector = (PULONG)UlAllocatePoolWithTag(
        NonPagedPool, 
        VkSystemHive->DirtyVectorSize * sizeof(ULONG), 
        'VkDv'
    );

    if (!VkSystemHive->DirtyVector)
    {
        KdPrintf("ERROR: Failed to allocate DirtyVector for VkSystemHive\n");
        UlFreePoolWithTag(VkSystemHive, 'VkHv');
        VkSystemHive = NULL;
        return FALSE;
    }

    RtlZeroMemory(VkSystemHive->DirtyVector, VkSystemHive->DirtyVectorSize * sizeof(ULONG));
    
    // Check our RootNode
    RootNode = (PKEY_DIRECTORY_NODE)((PUCHAR)BaseBlock + sizeof(KEY_BASE_BLOCK));
    if (RootNode->Signature != KEY_DIR_SIG)
    {
        KdPrintf("ERROR: Root node signature mismatch! Expected 'DN'\n");
        UlFreePoolWithTag(VkSystemHive->DirtyVector, 'VkDv');
        UlFreePoolWithTag(VkSystemHive, 'VkHv');
        VkSystemHive = NULL;
        return FALSE;
    }

    KdPrintf("[Vk] System Hive successfully loaded and validated at 0x%p (%u bytes)\n", SystemHive, HiveLength);
    return TRUE;
}

BOOLEAN
VEAPI
VkpMountSystemHive(VOID)
{
    VEASTATUS Status;
    OBJECT_ATTRIBUTES ObjAttr;
    ANSI_STRING SystemPath;
    PVK_KEY_OBJECT SystemKeyObject = NULL;
    HANDLE KeyHandle = NULL;

    // Basic Validation
    if (!VkSystemHive || !VkSystemHive->BaseBlock)
    {
        KdPrintf("[Vk] ERROR: Cannot mount, VkSystemHive is NULL\n");
        return FALSE;
    }

    // Take Root Key (DN) from BaseBlock. Jump after BaseBlock
    PKEY_DIRECTORY_NODE RootNode = (PKEY_DIRECTORY_NODE)(
        (PUCHAR)VkSystemHive->BaseBlock + sizeof(KEY_BASE_BLOCK)
    );

    // Validate it
    if (RootNode->Signature != KEY_DIR_SIG)
    {
        KdPrintf("[Vk] ERROR: Root node signature mismatch! Expected 'DN'\n");
        return FALSE;
    }

    // Set Attribute
    RtlInitAnsiString(&SystemPath, "/REGISTRY/MACHINE/SYSTEM");

    RtlZeroMemory(&ObjAttr, sizeof(OBJECT_ATTRIBUTES));
    ObjAttr.ObjectName = &SystemPath;
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
        (PVOID)&SystemKeyObject
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[Vk] ERROR: Failed to create Object Manager instance for SYSTEM key! (Status: 0x%X)\n", Status);
        return FALSE;
    }

    // Fill our VK_KEY_OBJECT with abstraction
    RtlZeroMemory(SystemKeyObject, sizeof(VK_KEY_OBJECT));
    SystemKeyObject->HiveBase    = VkSystemHive;
    SystemKeyObject->KeyNode     = RootNode;
    SystemKeyObject->KeyOffset = (ULONG)((PUCHAR)RootNode - (PUCHAR)VkSystemHive->BaseAddress);
    RtlInitAnsiString(&SystemKeyObject->FullKeyPath, "/REGISTRY/MACHINE/SYSTEM");

    // Insert it
    Status = ObInsertObject(
        (PVOID)SystemKeyObject,
        &ObjAttr
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[Vk] ERROR: Failed to insert SYSTEM key into Object Namespace!\n");
        return FALSE;
    }

    KdPrintf("[Vk] System Hive successfully mounted to /REGISTRY/MACHINE/SYSTEM\n");
    return TRUE;
}

VOID 
VEAPI
VkCreateSymlinkCCS(VOID)
{
    HANDLE LinkHandle = NULL;
    ULONG Disposition = 0;
    ANSI_STRING LinkPath, SymlinkValueName, TargetPath;
    OBJECT_ATTRIBUTES ObjAttr;
    VEASTATUS Status;
    PVK_KEY_OBJECT LinkKeyObject = NULL;
    PEPROCESS CurrentProcess = (PEPROCESS)PtGetCurrentProcess();

    if(!VkSystemHive)
    {
        KdPrintf("[Vk] ERROR | No System Hive.\n");
        return;
    }

    // 1. Inisialisasi ObjAttr
    RtlInitAnsiString(&LinkPath, "/REGISTRY/MACHINE/SYSTEM/CurrentControlSet");
    RtlZeroMemory(&ObjAttr, sizeof(OBJECT_ATTRIBUTES));
    ObjAttr.ObjectName = &LinkPath;

    // 2. Buat Volatile Key
    Status = VeaCreateKey(&LinkHandle, KEY_ALL_ACCESS, &ObjAttr, 0, NULL,
                        REG_OPTION_VOLATILE, &Disposition);
    
    if(!VEA_SUCCESS(Status))
    {
        KdPrintf("[Vk] ERROR | Failed to create Key.\n");
        return;
    }

    // -------------------------------------------------------------------------
    // 3. TULIS VALUE "SymbolicLinkValue" TERLEBIH DAHULU!
    // -------------------------------------------------------------------------
    RtlInitAnsiString(&SymlinkValueName, "SymbolicLinkValue");
    RtlInitAnsiString(&TargetPath, "/REGISTRY/MACHINE/SYSTEM/ControlSet001");
    
    // Gunakan TargetPath.Length + 1 agar '\0' ikut tersimpan
    Status = VeaSetValueKey(LinkHandle, &SymlinkValueName, 0, FILE_KEY_LINK,
                            TargetPath.Buffer, TargetPath.Length + 1);

    if(!VEA_SUCCESS(Status))
    {
        // Jika gagal di sini, kita bisa langsung tahu
        KdPrintf("[Vk] ERROR | Failed to set key at %Z. Status: %x\n", &TargetPath, Status);
        if(Status == STATUS_INVALID_HANDLE)
        {
            KdPrintf("[Vk] INVALID HANDLE: 0x%x\n", LinkHandle);
        }

        //VeaClose(LinkHandle);
        return;
    }

    // -------------------------------------------------------------------------
    // 4. BARU PASANG FLAG KEY_DIR_SYMLINK SETELAH VALUE TERPASANG
    // -------------------------------------------------------------------------
    ObReferenceObjectByHandle(CurrentProcess->ObjectTable, LinkHandle, 0x10000, VkKeyObjectType, KernelMode, (PVOID*)&LinkKeyObject);
    if(!LinkKeyObject)
    {
        KdPrintf("[Vk] ERROR | Failed to flagging key at %Z.\n", &TargetPath);
        return;
    }

    // Set flag symlink
    LinkKeyObject->KeyNode->Flags |= KEY_DIR_SYMLINK;

    // Tandai dirty (bila perlu, walau volatile cell biasanya di-skip oleh storage manager)
    VkpMarkCellDirty(LinkKeyObject->HiveBase, LinkKeyObject->KeyOffset);

    ObDereferenceObject(LinkKeyObject);
    //VeaClose(LinkHandle);
}

BOOLEAN
VEAPI
VkInitSystem1(IN PBLOCK_BOOT_2 BlockBoot)
{
    OBJECT_TYPE_INITIALIZER VKKeyObjInit;
    ANSI_STRING Name;

    // Make VKKeyObject 
    RtlZeroMemory(&VKKeyObjInit, sizeof(OBJECT_TYPE_INITIALIZER));
    RtlInitAnsiString(&Name, "Key");

    // Configuring object for VkKey
    VKKeyObjInit.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    VKKeyObjInit.PoolType = NonPagedPool;
    VKKeyObjInit.ObjectSize = sizeof(VK_KEY_OBJECT);
    VKKeyObjInit.ValidAccessMask = 0; // KEY_ALL_ACCESS
    VKKeyObjInit.ParseRoutine = VkpParseKeyObject;
    VKKeyObjInit.ValidAccessMask = KEY_ALL_ACCESS;
    VKKeyObjInit.DeleteRoutine = NULL; //VkpDeleteKeyObject;

    VEASTATUS Status = ObCreateObjectType(Name.Buffer, &VKKeyObjInit, &VkKeyObjectType);
    if(!VEA_SUCCESS(Status))
    {
        return FALSE;
    }

    // Make our Object Directory
    OBJECT_ATTRIBUTES DirectoryAttributes;
    ANSI_STRING DirectoryName;

    RtlZeroMemory(&DirectoryAttributes, sizeof(OBJECT_ATTRIBUTES));
    RtlInitAnsiString(&DirectoryName, "/REGISTRY");
    DirectoryAttributes.ObjectName = &DirectoryName;
    Status = VeaCreateDirectoryNamespace(&DirectoryAttributes);
    if(!VEA_SUCCESS(Status))
    {
        return FALSE;
    }

    RtlZeroMemory(&DirectoryAttributes, sizeof(OBJECT_ATTRIBUTES));
    RtlInitAnsiString(&DirectoryName, "/REGISTRY/MACHINE");
    DirectoryAttributes.ObjectName = &DirectoryName;
    Status = VeaCreateDirectoryNamespace(&DirectoryAttributes);
    if(!VEA_SUCCESS(Status))
    {
        return FALSE;
    }

    // We need to initialize our System HIVE
    if(BlockBoot->SystemHiveBase && BlockBoot->SystemHiveLength > 0)
    {
        if(!VkpInitializeSystemHive(BlockBoot->SystemHiveBase, BlockBoot->SystemHiveLength))
        {
            KsBugCheck(VKEY_CONFIG_CORRUPTED);
            return FALSE;
        }

        if(!VkpMountSystemHive())
        {
            KsBugCheck(VKEY_MASTER_FAILED);
            return FALSE;
        }
    }
    
    return TRUE;
}

BOOLEAN 
VEAPI
VkInitSystemPhase1(VOID)
{
    /* Create Symlink */
    VkCreateSymlinkCCS();

    return TRUE;
}