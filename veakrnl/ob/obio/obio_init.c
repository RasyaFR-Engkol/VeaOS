#include <veakrnl.h>

POBJECT_TYPE ObpDriverObjectType = NULL;
POBJECT_TYPE ObpDeviceObjectType = NULL;
PVOID ObpFileSystemDirectoryObject = NULL; // Folder baru buat File System
PBLOCK_BOOT_2 ObioBlockBoot = NULL;
PANSI_STRING ObioGroupOrderTable = NULL;
ULONG ObioGroupOrderount = 0;

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

    // 1. Register "Driver" Object Type
    RtlZeroMemory(&TypeInit, sizeof(OBJECT_TYPE_INITIALIZER));
    TypeInit.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    TypeInit.PoolTag = 'Driv';
    TypeInit.ObjectSize = sizeof(DRIVER_OBJECT);
    TypeInit.InterruptHandler = ObioDummyInterruptHandler;

    ObCreateObjectType("Driver", &TypeInit, &ObpDriverObjectType);

    // 2. Register "Device" Object Type
    RtlZeroMemory(&TypeInit, sizeof(OBJECT_TYPE_INITIALIZER));
    TypeInit.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    TypeInit.PoolTag = 'Devc';
    TypeInit.ObjectSize = sizeof(DEVICE_OBJECT);
    TypeInit.InterruptHandler = ObioDummyInterruptHandler;

    ObCreateObjectType("Device", &TypeInit, &ObpDeviceObjectType);

    // 3. Bikin /FileSystem Directory Node
    RtlInitAnsiString(&NameAnsi, "FileSystem");
    InitializeObjectAttributes(&Attr, &NameAnsi, OBJ_CASE_INSENSITIVE, ObpRootDirectoryObject, NULL);

    // Updated ObAllocateObject (4 parameters: ObjectType, Attr, ObjectSize=0, ReturnedObject)
    ObAllocateObject(ObpDirectoryObjectType, &Attr, 0, &ObpFileSystemDirectoryObject);
    
    // Inisialisasi list kepala direktori
    InitializeListHead(&((POBJECT_DIRECTORY)ObpFileSystemDirectoryObject)->Head);
    
    // Masukkan ke object tree
    ObInsertObject(ObpFileSystemDirectoryObject, &Attr);

    kdp_print("OBIO: I/O Subsystem successfully initialized!\n\r");

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
    Status = VeaCreateKey(&KeyHandle, KEY_READ, &ObjAttr, 0, NULL, 0, NULL);
    if(!VEA_SUCCESS(Status))
    {
        /* Why the hell does this fail? */
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
            /* No buffer */
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
    Count = 1; 

    for (ULONG i = 0; i < ResultLength; i++)
    {
        /* VKEY: \n in MultiSZ is new line */
        /* One string done */
        if (Ptr[i] == '\n')
        {
            Count++;
        }
    }

    /* Lets allocate the memory */
    ObioGroupOrderTable = (PANSI_STRING)UlAllocatePoolZero(NonPagedPool, Count * sizeof(ANSI_STRING), 'Obio');
    if (!ObioGroupOrderTable)
    {
        Status = STATUS_INSUFFICIENT_MEMORY;
        goto Cleanup;
    }
    ObioGroupOrderount = Count;

    /* Now we gotta parse and insert them */
    ULONG CurrentIndex = 0;
    PUCHAR StartPtr = Ptr;
    ULONG CurrentLength = 0;

    for (ULONG i = 0; i < ResultLength; i++)
    {
        // Jika ketemu pemisah (\n) atau null terminator di akhir hexdump
        if (Ptr[i] == '\n' || Ptr[i] == '\0')
        {
            // Mutasi buffer: ubah \n menjadi \0 agar setiap string C-style null-terminated
            Ptr[i] = '\0';
            
            ObioGroupOrderTable[CurrentIndex].Buffer = (PCHAR)StartPtr;
            ObioGroupOrderTable[CurrentIndex].Length = (USHORT)CurrentLength;
            ObioGroupOrderTable[CurrentIndex].MaximumLength = (USHORT)(CurrentLength + 1);
            
            CurrentIndex++;
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

Cleanup:
    if (Status != STATUS_SUCCESS) DPRINT("[Obio] ERROR | GroupOrderLoad fail. Status: 0x%x\n", Status);
    if(Buffer) UlFreePoolWithTag(Buffer, 'Obio');
    return;
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
        KdPrintf("|-- %s [State=%u Flags=0x%x]\n\r", Node->InstancePath.Buffer, Node->State, Node->Flags);
    } else {
        KdPrintf("|-- (Unnamed) [State=%u Flags=0x%x]\n\r", Node->State, Node->Flags);
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

//
// Initialize oure System IO
//
BOOLEAN
VEAPI
ObioInitSystem1(VOID)
{
    VEASTATUS Status;
    /* Save our BlockBoot */
    ObioBlockBoot = KsLoaderBlock;

    /* Initialize Pnp */
    Status = ObioInitializePnPService();
    if(!VEA_SUCCESS(Status))
    {
        /* BugCheck */
        KsBugCheckEx(IO1_INITIALIZATION_FAILED, Status, 0, 0, 0);
    }

    /* It should initialize ACPI_HCT by itself (read ACPI, more) */
    HctInitializePnp();

    /* Dump current DevNode tree to help debugging */
    // ObioDumpDeviceTree();

    /* So we can initialize BootDrivers now (it must SUCCESS) */
    // if(!ObioInitializeBootDriver())
    // {
    //     /* Bugcheck then if it's not success */
    //     KsBugCheckEx(IO1_INITIALIZATION_FAILED, 0x1, 0, 0, 0);
    // }

    return TRUE;
}