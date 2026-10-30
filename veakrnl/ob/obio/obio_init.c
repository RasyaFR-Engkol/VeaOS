#include <veakrnl.h>
#include <ndk/ldrtypes.h>

POBJECT_TYPE ObpDriverObjectType = NULL;
POBJECT_TYPE ObpDeviceObjectType = NULL;
POBJECT_TYPE ObpFileObjectType = NULL;
PVOID ObpFileSystemDirectoryObject = NULL;
PBLOCK_BOOT_2 ObioBlockBoot = NULL;
PLIST_ENTRY ObioGroupTable = NULL;
PANSI_STRING ObioGroupOrderTable = NULL;
ULONG ObioGroupOrderCount = 0;
ULONG ObioGroupTableCount = 0;
BOOLEAN ObioPnpSystemInit = FALSE;

VEASTATUS
VEAPI
ObioDummyInterruptHandler(
    PVOID Object,
    POIP Oip
);

BOOLEAN
VEAPI
ObioInitSubsystem(
    VOID
)
{
    OBJECT_TYPE_INITIALIZER TypeInit;
    OBJECT_ATTRIBUTES Attr;
    ANSI_STRING NameAnsi;
    VEASTATUS Status;
    HANDLE FsHandle;

    /* Create Driver Object type */
    RtlZeroMemory(&TypeInit, sizeof(OBJECT_TYPE_INITIALIZER));
    TypeInit.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    TypeInit.PoolTag = 'Driv';
    TypeInit.ObjectSize = sizeof(DRIVER_OBJECT);
    TypeInit.InterruptHandler = ObioDummyInterruptHandler;
    TypeInit.AllowAttachByDefault = TRUE;

    ObCreateObjectType("Driver", &TypeInit, &ObpDriverObjectType);

    /* Create Device Object type */
    RtlZeroMemory(&TypeInit, sizeof(OBJECT_TYPE_INITIALIZER));
    TypeInit.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    TypeInit.PoolTag = 'Devc';
    TypeInit.ObjectSize = sizeof(DEVICE_OBJECT);
    TypeInit.InterruptHandler = ObioDummyInterruptHandler;
    TypeInit.AllowAttachByDefault = TRUE;
    TypeInit.ParseRoutine = ObioParseDevice;

    ObCreateObjectType("Device", &TypeInit, &ObpDeviceObjectType);

    /* Create File Object type */
    RtlZeroMemory(&TypeInit, sizeof(OBJECT_TYPE_INITIALIZER));
    TypeInit.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    TypeInit.ObjectSize = sizeof(FILE_OBJECT);
    TypeInit.PoolTag = 'File'; // Fixed: sebelumnya PoolType = 'File'
    TypeInit.InterruptHandler = ObioDummyInterruptHandler;
    TypeInit.AllowAttachByDefault = FALSE;
    TypeInit.ParseRoutine = ObioParseFile;
    // TypeInit.CreateRoutine = ObioCreateFile;
    // TypeInit.DeleteRoutine = ObioDeleteFile;
    // TypeInit.QueryNameMethod  = ObioQueryNameFile;

    ObCreateObjectType("File", &TypeInit, &ObpFileObjectType);

    /* Create FileSystem directory node (/FileSystem) */
    RtlInitAnsiString(&NameAnsi, "FileSystem");
    InitializeObjectAttributes(&Attr, &NameAnsi, OBJ_PERMANENT, NULL, NULL);

    // Pakai ObCreateObject agar CreateInfo (nama & root directory) terpasang di Object Header
#if 0
    Status = ObCreateObject(
        KernelMode,
        ObpDirectoryObjectType,
        &Attr,
        KernelMode,
        NULL,
        sizeof(OBJECT_DIRECTORY),
        0,
        0,
        &ObpFileSystemDirectoryObject
    );
#endif
    Status = VeaCreateDirectoryNamespace(&FsHandle, 0x10000, &Attr);

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("OBIO: Failed to create FileSystem directory! Status: 0x%x\n\r", Status);
        return FALSE;
    }

    Status = ObReferenceObjectByHandle(
        FsHandle,
        0x10000,
        ObpDirectoryObjectType,
        KernelMode,
        (PVOID*)&ObpFileSystemDirectoryObject
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("OBIO: Failed to reference FileSystem directory! Status: 0x%x\n\r", Status);
        return FALSE;
    }

    VeaClose(FsHandle);

    /* HACK */
    InitializeListHead(&((POBJECT_DIRECTORY)ObpFileSystemDirectoryObject)->Head);

    // Panggil ObInsertObject versi baru (6 parameter)
    Status = ObInsertObject(
        ObpFileSystemDirectoryObject,
        NULL,   // AccessState
        0,      // DesiredAccess
        0,      // ObjectPointerBias
        NULL,   // NewObject
        NULL    // Handle
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("OBIO: Failed to insert FileSystem directory! Status: 0x%x\n\r", Status);
        return FALSE;
    }

    DPRINT("OBIO: I/O Subsystem successfully initialized!\n\r");

    return TRUE;
}
//
// Initialize Group Order Load
//
VOID
VEAPI
ObioInitGroupOrderLoad(VOID)
{
    HANDLE KeyHandle;
    VEASTATUS Status;
    PANSI_STRING GroupOrderTable;
    ULONG Count;
    ANSI_STRING GroupOrderLocation = 
        RTL_CONSTANT_ANSI_STRING("/REGISTRY/MACHINE/SYSTEM/CurrentControlSet"
                                "/Control/ServiceGroupOrder");
    ANSI_STRING GroupListString = RTL_CONSTANT_ANSI_STRING("List");
    OBJECT_ATTRIBUTES ObjAttr;  
    ULONG ResultLength;       
    PVOID Buffer;
    
    /* Read ServiceGroupOrder key */
    InitializeObjectAttributes(&ObjAttr, &GroupOrderLocation, 0, NULL, NULL);
    Status = ObioOpenVeaKey(
        NULL,
        &KeyHandle,
        &GroupOrderLocation,
        KEY_READ
    );
    if(!VEA_SUCCESS(Status))
    {
        DPRINT("VeaCreateKey failed: 0x%x\n", Status);
        return;
    }

    /* Enumerate List */
    Status = VeaQueryValueKey(KeyHandle, &GroupListString, NULL, NULL, 0, &ResultLength);
    if(Status == STATUS_BUFFER_TOO_SMALL)
    {

        /* We must allocate buffer with size of ResultLength */
        Buffer = (PVOID)UlAllocatePoolZero(NonPagedPool, ResultLength, 'Obio');
        if(!Buffer)
        {
            DPRINT("UlAllocatePoolZero failed: 0x%x\n", Status);
            return;
        }

        /* Reread the key */
        Status = VeaQueryValueKey(KeyHandle, &GroupListString, NULL, Buffer, ResultLength, &ResultLength);
        if(!VEA_SUCCESS(Status))
        {
            /* Cleaup */
            goto Cleanup;
        }
    }
    else if(!VEA_SUCCESS(Status))
    {
        /* Key is NOT THERE */
        goto Cleanup;
    }

    /* Lets init them into TABLE */
    PUCHAR Ptr = (PUCHAR)Buffer;
    Count = 0;
    ULONG CurrentLength = 0;

    for (ULONG i = 0; i < ResultLength; i++)
    {
        if (Ptr[i] == '\n' || Ptr[i] == '\0')
        {
            ULONG Start = i - CurrentLength;
            while (CurrentLength > 0 &&
                   (Ptr[Start] == ' ' || Ptr[Start] == '\t' || Ptr[Start] == '\r'))
            {
                Start++;
                CurrentLength--;
            }
            while (CurrentLength > 0 &&
                   (Ptr[Start + CurrentLength - 1] == ' ' ||
                    Ptr[Start + CurrentLength - 1] == '\t' ||
                    Ptr[Start + CurrentLength - 1] == '\r'))
            {
                CurrentLength--;
            }
            if (CurrentLength > 0)
            {
                Count++;
            }
            CurrentLength = 0;
        }
        else
        {
            CurrentLength++;
        }
    }

    /* Lets allocate the memory */
    ObioGroupOrderTable = (PANSI_STRING)UlAllocatePoolZero(NonPagedPool, Count * sizeof(ANSI_STRING), 'Obio');
    if (!ObioGroupOrderTable)
    {
        Status = STATUS_INSUFFICIENT_MEMORY;
        goto Cleanup;
    }
    ObioGroupOrderCount = Count;

    /* Now we gotta parse and insert them */
    ULONG CurrentIndex = 0;
    PUCHAR StartPtr = Ptr;
    CurrentLength = 0;

    for (ULONG i = 0; i < ResultLength; i++)
    {
        if (Ptr[i] == '\n' || Ptr[i] == '\0')
        {
            Ptr[i] = '\0';

            PUCHAR GroupStart = StartPtr;
            while (CurrentLength > 0 &&
                   (*GroupStart == ' ' || *GroupStart == '\t' || *GroupStart == '\r'))
            {
                GroupStart++;
                CurrentLength--;
            }
            while (CurrentLength > 0 &&
                   (GroupStart[CurrentLength - 1] == ' ' ||
                    GroupStart[CurrentLength - 1] == '\t' ||
                    GroupStart[CurrentLength - 1] == '\r'))
            {
                CurrentLength--;
            }

            if (CurrentLength > 0)
            {
                GroupStart[CurrentLength] = '\0';
                ObioGroupOrderTable[CurrentIndex].Buffer = (PCHAR)GroupStart;
                ObioGroupOrderTable[CurrentIndex].Length = (USHORT)CurrentLength;
                ObioGroupOrderTable[CurrentIndex].MaximumLength = (USHORT)(CurrentLength + 1);
                CurrentIndex++;
            }

            StartPtr = &Ptr[i + 1];
            CurrentLength = 0;
        }
        else
        {
            CurrentLength++;
        }
    }

    Buffer = NULL;
    Status = STATUS_SUCCESS;
    ObioDumpGroupLoadOrder();

Cleanup:
    if (Status != STATUS_SUCCESS) DPRINT("[Obio] ERROR | GroupOrderLoad fail. Status: 0x%x\n", Status);
    if(Buffer) UlFreePoolWithTag(Buffer, 'Obio');
    return;
}

VOID
VEAPI
ObioDumpGroupLoadOrder(VOID)
{
    if (!ObioGroupOrderTable || ObioGroupOrderCount == 0)
    {
        kdp_print("[Obio] Group load order is not initialized\n\r");
        return;
    }

    kdp_print("\n\r=== Group Load Order Dump ===\n\r");
    for (ULONG Index = 0; Index < ObioGroupOrderCount; Index++)
    {
        KdPrintf("[Obio] [%u] %s\n\r", Index, ObioGroupOrderTable[Index].Buffer);
    }
    kdp_print("=============================\n\r");
}

/* DevNode tree dump helpers */
VOID
VEAPI
ObioDumpDeviceNodeRecursive(
    IN PDEVICE_NODE Node,
    IN ULONG Depth
)
{
    if (!Node) return;

    for (ULONG i = 0; i < Depth; i++) {
        kdp_print("    ");
    }

    if (Node->InstancePath.Buffer) {
        KdPrintf("|-- %s [State=0x%x Flags=0x%x]\n\r", Node->InstancePath.Buffer, Node->State, Node->Flags);
    } else {
        KdPrintf("|-- (Unnamed) [State=0x%x Flags=0x%x]\n\r", Node->State, Node->Flags);
    }

    PDEVICE_NODE Child = Node->Child;
    while (Child) {
        ObioDumpDeviceNodeRecursive(Child, Depth + 1);
        Child = Child->Sibling;
    }
}

VOID
VEAPI
ObioDumpDeviceTree(VOID)
{
    kdp_print("\n\r=== DevNode Tree Dump ===\n\r");
    if (ObioRootDeviceNode) {
        ObioDumpDeviceNodeRecursive(ObioRootDeviceNode, 0);
    } else {
        kdp_print("(no root devnode)\n\r");
    }
    kdp_print("=========================\n\r");
}

USHORT
VEAPI
ObioGetGroupOrderIndex(HANDLE ServiceHandle)
{
    ANSI_STRING GroupValueName =
        RTL_CONSTANT_ANSI_STRING("Group");

    ANSI_STRING DriverGroup;
    CHAR GroupBuffer[128];
    ULONG ValueType;
    ULONG ResultLength;
    VEASTATUS Status;

    /* Check if ObioGroupOrderTable exist */
    if (!ObioGroupOrderTable || ObioGroupOrderCount == 0)
    {
        DPRINT("[Obio] Group order table is not available\n");
        return 0xFFFF;
    }

    /* Return size of ObioGroupOrderCount if no service handle specified */
    if (!ServiceHandle)
    {
        return (USHORT)ObioGroupOrderCount;
    }

    RtlZeroMemory(GroupBuffer, sizeof(GroupBuffer));

    /* Query key for driver Group */
    Status = VeaQueryValueKey(
        ServiceHandle,
        &GroupValueName,
        &ValueType,
        GroupBuffer,
        sizeof(GroupBuffer),
        &ResultLength
    );

    /* Check status */
    if (!VEA_SUCCESS(Status))
    {
        DPRINT(
            "[Obio] Failed to query Group value: 0x%x\n",
            Status
        );
        return 0xFFFF;
    }

    /* Make sure is FILE_KEY_STRING not any other KEY_TYPE */
    if (ValueType != FILE_KEY_STRING)
    {
        DPRINT("[Obio] Group value has invalid type: %u\n", ValueType);
        return 0xFFFF;
    }

    /* ResultLength should valid */
    if (ResultLength == 0 || ResultLength > sizeof(GroupBuffer))
    {
        return 0xFFFF;
    }

    /* Exclude Null Terminator from string length */
    if (GroupBuffer[ResultLength - 1] == '\0')
    {
        ResultLength--;
    }

    DriverGroup.Buffer = GroupBuffer;
    DriverGroup.Length = (USHORT)ResultLength;
    DriverGroup.MaximumLength = (USHORT)sizeof(GroupBuffer);

    /* Compare every group that exist */
    for (USHORT i = 0; i < ObioGroupOrderCount; i++)
    {
        if (RtlEqualString(
                &DriverGroup,
                &ObioGroupOrderTable[i],
                TRUE))
        {
            return i;
        }
    }

    /* Why the hell does this driver doesn't have group? Weirdical behavior */
    DPRINT(
        "[Obio] Driver group not found: %s\n",
        DriverGroup.Buffer
    );

    return 0xFFFF;
}

BOOLEAN
VEAPI
ObioMarkBootPartition(IN PBLOCK_BOOT_2 LoaderBlock)
{
    PBOOT_DEVICE_IDENTIFIER Bdi = &LoaderBlock->BootPartitionSignature;
    CHAR BdiPath[50];
    ULONG NumberPartition, DiskSignature;
    GUID GptDiskGuid;
    HANDLE FileHandle;
    OBJECT_ATTRIBUTES ObjAttr;
    ANSI_STRING DeviceName;
    OIP_INFORMATION IoStatusBlock;
    PFILE_OBJECT BootDevice;
    VEASTATUS Status;
    
    if (Bdi->Type == BootDeviceTypeMbr)
    {
        NumberPartition = Bdi->u.Mbr.PartitionNumber;
        DiskSignature = Bdi->u.Mbr.DiskSignature;

        RtlStringCbPrintfA(BdiPath, 50, "/BDI/BDI_%u%%%08X%%%u", Bdi->Type, DiskSignature, NumberPartition);
        RtlInitAnsiString(&DeviceName, BdiPath);
    }
    else if (Bdi->Type == BootDeviceTypeGpt)
    {
        // TODO: GPT
    }

    InitializeObjectAttributes(&ObjAttr, &DeviceName, 0, NULL, NULL);
    Status = VeaOpenFile(
        &FileHandle,
        0x80, // FILE_READ_ATTRIBUTES 
        &ObjAttr,
        &IoStatusBlock,
        0,
        0x40 // FILE_NON_DIRECTORY_FILE 
    ); 

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("VeaOpenFile() failed: 0x%x\n", Status);
        KsBugCheckEx(
            BOOT_DEVICE_HANDLE_MISSING,
            (ULONG_PTR)&DeviceName,
            Status,
            0,
            0
        );
    }

    Status = ObReferenceObjectByHandle(
        FileHandle,
        0,
        ObpFileObjectType,
        KernelMode,
        (PVOID*)&BootDevice
    );
    if (!VEA_SUCCESS(Status))
    {
        return FALSE;
    }

    DPRINT("BDI Symlink got file: %Z\n", BootDevice->FileName);
    BootDevice->DeviceObject->Flags |= DO_SYSTEM_BOOT_PARTITION;
    DPRINT("BootDevice %p is now DO_SYSTEM_BOOT_PARTITION\n", BootDevice);

    ObDereferenceObject(BootDevice);
    VeaClose(FileHandle);

    return TRUE;
}

VEASTATUS
VEAPI
ObioReassignSystemRoot(
    IN PBLOCK_BOOT_2 LoaderBlock,
    OUT PANSI_STRING AnsiString
)
{
    OBJECT_ATTRIBUTES ObjAttr;
    OIP_INFORMATION IoStatusBlock;
    HANDLE FileHandle;
    VEASTATUS Status;
    ANSI_STRING SystemRootTarget;
    ANSI_STRING SystemRootSymlinkName;

    RtlInitAnsiString(&SystemRootTarget, "/Device/Harddisk0/Partition1/VeaOS");
    RtlInitAnsiString(&SystemRootSymlinkName, "SystemRoot");

    InitializeObjectAttributes(
        &ObjAttr,
        &SystemRootTarget,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL
    );

    Status = VeaOpenFile(
        &FileHandle,
        FILE_READ_DATA | FILE_READ_ATTRIBUTES,
        &ObjAttr,
        &IoStatusBlock,
        0,
        FILE_DIRECTORY_FILE
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("ObioSetupSystemRoot: Failed to open '%Z'! Status = 0x%x\n", 
               &SystemRootTarget, Status);
        return Status; // BDI ketemu tapi folder /VeaOS/ gak ada di partisi
    }

    VeaClose(FileHandle);

    Status = ObCreateSymbolicLink(
        ObpRootDirectoryObject,
        SystemRootSymlinkName.Buffer,
        SystemRootTarget.Buffer
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("ObioSetupSystemRoot: Failed to create Symlink '/SystemRoot'! Status = 0x%x\n", Status);
        return Status;
    }

    return Status;
}

VOID
VEAPI
ObioInitializeSystemDrivers(VOID)
{
    PANSI_STRING *DriverList, *SavedList;

    SavedList = DriverList = VkGetSystemDriverList();

    while (*DriverList)
    {
        PANSI_STRING CurrentDriverPath = *DriverList;

        DPRINT("[OBIO] Loading driver: %Z\n", CurrentDriverPath);

        // TODO: Load Driver and resolve symbol with Mm module and Rtl

                DriverList++;
    }

    UlFreePool(SavedList);

    ObioQueueDeviceAction(
        ObioRootDeviceNode->PhysicalDeviceObject,
        PiActionEnumDeviceTree,
        NULL,
        NULL
    );
}

//
// Initialize our System IO
//
BOOLEAN
VEAPI
ObioInitSystem1(VOID)
{
    VEASTATUS Status;
    ANSI_STRING VeaBootPath;
    LARGE_INTEGER TimerExpired;

    DPRINT("ObioInitSystem1\n");
    VkDumpTree();

    /* Iniialize List Head */
    InitializeListHead(&ObioFsNetworkListHead);
    InitializeListHead(&ObioFsDiskListHead);
    InitializeListHead(&ObioFsCdRomListHead);
    InitializeListHead(&ObioTimerQueueHead);
    
    /* Save our BlockBoot */
    ObioBlockBoot = KsLoaderBlock;

    /* Initialize Timer SpinLock, HeadList, and it's DPC and timer */
    TimerExpired.QuadPart = -10000000;
    KsInitializeDpc(&ObioTimerDpc, ObioTimerDispatch, NULL);
    KsInitializeTimer(&ObioTimer);
    KsSetTimer(&ObioTimer, TimerExpired, 1000, &ObioTimerDpc);

    /* Initialize Pnp */
    Status = ObioInitializePnPService();
    if(!VEA_SUCCESS(Status))
    {
        /* BugCheck */
        KsBugCheckEx(IO1_INITIALIZATION_FAILED, Status, 0, 0, 0);
    }

    /* It should initialize ACPI_HCT by itself (read ACPI, more) */
    HctInitializePnp();

    /* Inititalize what HCT has added to us */
    ObioQueueDeviceAction(ObioRootDeviceNode->PhysicalDeviceObject,
                          PiActionEnumRootDevices,
                          NULL,
                          NULL);

    /* So we can initialize BootDrivers now (it must SUCCESS) */
    if(!ObioInitializeBootDriver())
    {
        /* Bugcheck then if it's not success */
        KsBugCheckEx(IO1_INITIALIZATION_FAILED, 0x1, 0, 0, 0);
    }

    /* Io no needing this now */
    ObioBlockBoot = NULL;

    ObioCreateBdiSymlink(KsLoaderBlock);

    if (!ObioMarkBootPartition(KsLoaderBlock))
    {
        DPRINT("ObioMarkBootPartition() failed\n");
        return FALSE;
    }

    /* Everything loaded here. we can load System Start Drivers*/
    
    ObioInitializeSystemDrivers();

    ObioPnpSystemInit = TRUE;

    Status = ObioReassignSystemRoot(KsLoaderBlock, &VeaBootPath);
    if (!VEA_SUCCESS(Status))
    {
        return FALSE;
    }

    /*
    
    Status = PtLocateSystemSo();
    if (!VEA_SUCCESS)
    {
        DPRINT("PtLocateSystemSo failed: 0x%x\n", Status);
        return FALSE;
    }

    */

    return TRUE;
}