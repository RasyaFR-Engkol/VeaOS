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
        (PVOID*)&SystemKeyObject
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[Vk] ERROR: Failed to create Object Manager instance for SYSTEM key! (Status: 0x%X)\n", Status);
        return FALSE;
    }

    // Fill our VK_KEY_OBJECT with abstraction
    RtlZeroMemory(SystemKeyObject, sizeof(VK_KEY_OBJECT));
    SystemKeyObject->HiveBase  = VkSystemHive;
    SystemKeyObject->KeyNode   = RootNode;
    SystemKeyObject->KeyOffset = (ULONG)((PUCHAR)RootNode - (PUCHAR)VkSystemHive->BaseAddress);
    RtlInitAnsiString(&SystemKeyObject->FullKeyPath, "/REGISTRY/MACHINE/SYSTEM");

    // Insert ke Object Tree dengan 6 parameter
    Status = ObInsertObject(
        SystemKeyObject,
        NULL,   // AccessState
        0,      // DesiredAccess
        0,      // ObjectPointerBias
        NULL,   // NewObject
        NULL    // Handle
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
    ObjAttr.Attributes = OBJ_PERMANENT;

    // 2. Buat Volatile Key
    Status = VeaCreateKey(&LinkHandle, KEY_ALL_ACCESS, &ObjAttr, 0, NULL,
                        REG_OPTION_VOLATILE, &Disposition);
    
    if(!VEA_SUCCESS(Status))
    {
        DPRINT("Failed to create CCS Volatile Key: 0x%x\n", Status);
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

        VeaClose(LinkHandle);
        return;
    }

    // -------------------------------------------------------------------------
    // 4. BARU PASANG FLAG KEY_DIR_SYMLINK SETELAH VALUE TERPASANG
    // -------------------------------------------------------------------------
    Status = ObReferenceObjectByHandle(
        LinkHandle,
        0x10000,
        VkKeyObjectType,
        KernelMode,
        (PVOID*)&LinkKeyObject
    );
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[Vk] ERROR | Failed to flagging key at %Z. Status: %x\n", &TargetPath, Status);
        return;
    }

    // Set flag symlink
    LinkKeyObject->KeyNode->Flags |= KEY_DIR_SYMLINK;

    // Tandai dirty (bila perlu, walau volatile cell biasanya di-skip oleh storage manager)
    VkpMarkCellDirty(LinkKeyObject->HiveBase, LinkKeyObject->KeyOffset);

    ObDereferenceObject(LinkKeyObject);
    VeaClose(LinkHandle);
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
    HANDLE DirHandle;

    RtlZeroMemory(&DirectoryAttributes, sizeof(OBJECT_ATTRIBUTES));
    RtlInitAnsiString(&DirectoryName, "/REGISTRY");
    DirectoryAttributes.ObjectName = &DirectoryName;
    DirectoryAttributes.Attributes |= OBJ_PERMANENT;
    Status = VeaCreateDirectoryNamespace(DirHandle, 0x10000, &DirectoryAttributes);
    if(!VEA_SUCCESS(Status))
    {
        return FALSE;
    }

    VeaClose(DirHandle);

    RtlZeroMemory(&DirectoryAttributes, sizeof(OBJECT_ATTRIBUTES));
    RtlInitAnsiString(&DirectoryName, "/REGISTRY/MACHINE");
    DirectoryAttributes.ObjectName = &DirectoryName;
    DirectoryAttributes.Attributes |= OBJ_PERMANENT;
    Status = VeaCreateDirectoryNamespace(DirHandle, 0x10000, &DirectoryAttributes);
    if(!VEA_SUCCESS(Status))
    {
        return FALSE;
    }

    VeaClose(DirHandle);

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

static
ULONG
VkpGetGroupOrderIndex(
    IN PCHAR GroupListBuffer,
    IN ULONG GroupListSize,
    IN PCHAR DriverGroup
)
{
    if (!GroupListBuffer || GroupListSize == 0 || !DriverGroup || DriverGroup[0] == '\0')
    {
        return 0xFFFFFFFF;
    }

    PCHAR Cursor = GroupListBuffer;
    PCHAR End = GroupListBuffer + GroupListSize;
    ULONG Index = 0;

    while (Cursor < End && *Cursor != '\0')
    {
        ULONG SubLen = (ULONG)RtlRawStringLength(Cursor);
        
        if (RtlRawStringCompareN(Cursor, DriverGroup, SubLen) == 0 &&
            DriverGroup[SubLen] == '\0')
        {
            return Index;
        }

        Cursor += SubLen + 1;
        Index++;
    }

    return 0xFFFFFFFF;
}

PANSI_STRING*
VEAPI
VkGetSystemDriverList(VOID)
{
    HANDLE ServicesHandle = NULL;
    HANDLE GroupOrderHandle = NULL;
    OBJECT_ATTRIBUTES ObjAttr;
    ANSI_STRING ServicesPath, GroupOrderPath;
    PVK_KEY_OBJECT ServicesKeyObject = NULL;
    PVK_HIVE Hive;
    PKEY_DIRECTORY_NODE ServicesNode;
    ANSI_STRING StartValueName, GroupValueName;
    PANSI_STRING *DriverList = NULL;
    PVK_DRIVER_ORDER_ENTRY DriverEntries = NULL;
    ULONG ServiceOffset;
    ULONG DriverCount = 0;
    ULONG Index = 0;
    VEASTATUS Status;

    CHAR GroupListBuffer[1024];
    ULONG GroupListSize = 0;

    RtlZeroMemory(GroupListBuffer, sizeof(GroupListBuffer));
    RtlInitAnsiString(&GroupOrderPath, "/REGISTRY/MACHINE/SYSTEM/CurrentControlSet/Control/ServiceGroupOrder");
    RtlZeroMemory(&ObjAttr, sizeof(OBJECT_ATTRIBUTES));
    ObjAttr.ObjectName = &GroupOrderPath;

    Status = ObOpenObjectByName(&ObjAttr, VkKeyObjectType, KernelMode, KEY_READ_ACCESS, NULL, &GroupOrderHandle);
    if (VEA_SUCCESS(Status))
    {
        ANSI_STRING ListValName;
        ULONG Type = 0;
        RtlInitAnsiString(&ListValName, "List");

        VeaQueryValueKey(
            GroupOrderHandle,
            &ListValName,
            &Type,
            GroupListBuffer,
            sizeof(GroupListBuffer),
            &GroupListSize
        );

        ObCloseHandle(GroupOrderHandle, KernelMode);
    }

    RtlInitAnsiString(&ServicesPath, "/REGISTRY/MACHINE/SYSTEM/CurrentControlSet/Services");
    RtlZeroMemory(&ObjAttr, sizeof(OBJECT_ATTRIBUTES));
    ObjAttr.ObjectName = &ServicesPath;

    Status = ObOpenObjectByName(&ObjAttr, VkKeyObjectType, KernelMode, KEY_READ_ACCESS, NULL, &ServicesHandle);
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[Vk] ERROR | Failed to open Services key! Status: 0x%X\n", Status);
        return NULL;
    }

    Status = ObReferenceObjectByHandle(
        ServicesHandle,
        KEY_READ_ACCESS,
        VkKeyObjectType,
        KernelMode,
        (PVOID*)&ServicesKeyObject
    );

    if (!VEA_SUCCESS(Status))
    {
        ObCloseHandle(ServicesHandle, KernelMode);
        return NULL;
    }

    Hive = ServicesKeyObject->HiveBase;
    ServicesNode = ServicesKeyObject->KeyNode;

    if (!ServicesNode || ServicesNode->Signature != KEY_DIR_SIG)
    {
        ObDereferenceObject(ServicesKeyObject);
        ObCloseHandle(ServicesHandle, KernelMode);
        return NULL;
    }

    RtlInitAnsiString(&StartValueName, "Start");
    RtlInitAnsiString(&GroupValueName, "Group");

    ServiceOffset = ServicesNode->FirstSubKeyOffset;

    while (ServiceOffset != 0)
    {
        PKEY_FILE_NODE StartFileNode;
        PKEY_DIRECTORY_NODE ServiceNode = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, ServiceOffset);
        if (!ServiceNode || ServiceNode->Signature != KEY_DIR_SIG) break;

        StartFileNode = VkpFindValueNode(Hive, ServiceNode, &StartValueName, NULL);

        if (StartFileNode && StartFileNode->Type == FILE_KEY_DWORD)
        {
            ULONG StartType = 0;
            if ((StartFileNode->Flags & KEY_FILE_INLINE) || StartFileNode->DataLength <= sizeof(ULONG))
            {
                StartType = StartFileNode->DataPOffset;
            }
            else
            {
                PUCHAR CellPtr = (PUCHAR)VkpGetCellPointer(Hive, StartFileNode->DataPOffset);
                if (CellPtr) StartType = *(PULONG)(CellPtr + sizeof(LONG));
            }

            if (StartType == SERVICE_SYSTEM_START)
            {
                DriverCount++;
            }
        }

        ServiceOffset = ServiceNode->NextSiblingOffset;
    }

    if (DriverCount == 0)
    {
        ObDereferenceObject(ServicesKeyObject);
        ObCloseHandle(ServicesHandle, KernelMode);
        return NULL;
    }

    DriverEntries = (PVK_DRIVER_ORDER_ENTRY)UlAllocatePoolWithTag(
        NonPagedPool,
        DriverCount * sizeof(VK_DRIVER_ORDER_ENTRY),
        'VkDr'
    );

    if (!DriverEntries)
    {
        ObDereferenceObject(ServicesKeyObject);
        ObCloseHandle(ServicesHandle, KernelMode);
        return NULL;
    }

    RtlZeroMemory(DriverEntries, DriverCount * sizeof(VK_DRIVER_ORDER_ENTRY));

    ServiceOffset = ServicesNode->FirstSubKeyOffset;
    Index = 0;

    while (ServiceOffset != 0 && Index < DriverCount)
    {
        PKEY_DIRECTORY_NODE ServiceNode = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, ServiceOffset);
        PKEY_FILE_NODE StartFileNode;

        if (!ServiceNode || ServiceNode->Signature != KEY_DIR_SIG) break;

        StartFileNode = VkpFindValueNode(Hive, ServiceNode, &StartValueName, NULL);

        if (StartFileNode && StartFileNode->Type == FILE_KEY_DWORD)
        {
            ULONG StartType = 0;
            if ((StartFileNode->Flags & KEY_FILE_INLINE) || StartFileNode->DataLength <= sizeof(ULONG))
            {
                StartType = StartFileNode->DataPOffset;
            }
            else
            {
                PUCHAR CellPtr = (PUCHAR)VkpGetCellPointer(Hive, StartFileNode->DataPOffset);
                if (CellPtr) StartType = *(PULONG)(CellPtr + sizeof(LONG));
            }

            if (StartType == SERVICE_SYSTEM_START)
            {
                CHAR ServiceName[128];
                CHAR DriverGroup[64];
                USHORT NameLen = (ServiceNode->NameLength < 127) ? ServiceNode->NameLength : 127;
                PKEY_FILE_NODE GroupNode;

                RtlCopyMemory(ServiceName, ServiceNode->Name, NameLen);
                ServiceName[NameLen] = '\0';

                // Buat Full Registry Path
                RtlStringCbPrintfA(
                    DriverEntries[Index].RegistryPath,
                    sizeof(DriverEntries[Index].RegistryPath),
                    "/REGISTRY/MACHINE/SYSTEM/CurrentControlSet/Services/%s",
                    ServiceName
                );

                // Baca value Group jika ada
                RtlZeroMemory(DriverGroup, sizeof(DriverGroup));
                GroupNode = VkpFindValueNode(Hive, ServiceNode, &GroupValueName, NULL);

                if (GroupNode && GroupNode->DataLength > 0)
                {
                    PVOID DataPtr = NULL;
                    if ((GroupNode->Flags & KEY_FILE_INLINE) || GroupNode->DataLength <= sizeof(ULONG))
                    {
                        DataPtr = &GroupNode->DataPOffset;
                    }
                    else
                    {
                        PUCHAR CellPtr = (PUCHAR)VkpGetCellPointer(Hive, GroupNode->DataPOffset);
                        if (CellPtr) DataPtr = (PVOID)(CellPtr + sizeof(LONG));
                    }

                    if (DataPtr)
                    {
                        USHORT GroupLen = (GroupNode->DataLength < 63) ? (USHORT)GroupNode->DataLength : 63;
                        RtlCopyMemory(DriverGroup, DataPtr, GroupLen);
                        DriverGroup[GroupLen] = '\0';
                    }
                }

                // Cari GroupIndex berdasarkan ServiceGroupOrder\List
                DriverEntries[Index].GroupIndex = VkpGetGroupOrderIndex(
                    GroupListBuffer,
                    GroupListSize,
                    DriverGroup
                );

                Index++;
            }
        }

        ServiceOffset = ServiceNode->NextSiblingOffset;
    }

    ObDereferenceObject(ServicesKeyObject);
    ObCloseHandle(ServicesHandle, KernelMode);

    for (ULONG i = 0; i < DriverCount - 1; i++)
    {
        for (ULONG j = i + 1; j < DriverCount; j++)
        {
            if (DriverEntries[i].GroupIndex > DriverEntries[j].GroupIndex)
            {
                VK_DRIVER_ORDER_ENTRY Temp = DriverEntries[i];
                DriverEntries[i] = DriverEntries[j];
                DriverEntries[j] = Temp;
            }
        }
    }

    DriverList = (PANSI_STRING*)UlAllocatePoolWithTag(
        NonPagedPool,
        (DriverCount + 1) * sizeof(PANSI_STRING),
        'VkDr'
    );

    if (!DriverList)
    {
        UlFreePoolWithTag(DriverEntries, 'VkDr');
        return NULL;
    }

    RtlZeroMemory(DriverList, (DriverCount + 1) * sizeof(PANSI_STRING));

    for (ULONG i = 0; i < DriverCount; i++)
    {
        ULONG PathLen = (ULONG)RtlRawStringLength(DriverEntries[i].RegistryPath);

        PANSI_STRING AnsiStr = (PANSI_STRING)UlAllocatePoolWithTag(
            NonPagedPool,
            sizeof(ANSI_STRING) + PathLen + 1,
            'VkDr'
        );

        if (AnsiStr)
        {
            AnsiStr->Buffer = (PCHAR)(AnsiStr + 1);
            AnsiStr->Length = (USHORT)PathLen;
            AnsiStr->MaximumLength = (USHORT)(PathLen + 1);
            RtlCopyMemory(AnsiStr->Buffer, DriverEntries[i].RegistryPath, PathLen);
            AnsiStr->Buffer[PathLen] = '\0';

            DriverList[i] = AnsiStr;
        }
    }

    // Bebaskan buffer temporary sorting
    UlFreePoolWithTag(DriverEntries, 'VkDr');

    return DriverList;
}

/* EOF */