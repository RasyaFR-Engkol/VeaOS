#include <veakrnl.h>
#include <veaguid.h>

KSPIN_LOCK ObioMgrSpinLock = 0;

VEASTATUS
VEAPI
ObioCreateNewDevice(
    PDRIVER_OBJECT DriverObject,
    ULONG DeviceExtensionSize,
    PCHAR DeviceName,
    ULONG DeviceType,
    PDEVICE_OBJECT *NewDevice
)
{
    if(!DriverObject || !NewDevice)
    {
        DPRINT("ERROR: DriverObject: %p, NewDevice: %p.\n", DriverObject, NewDevice);
        return STATUS_INVALID_PARAMETER;
    }

    PDEVICE_OBJECT CreatedDevice = NULL;
    VEASTATUS Status;
    ANSI_STRING NameAnsi;
    OBJECT_ATTRIBUTES Attr;
    ULONG TotalBodySize = sizeof(DEVICE_OBJECT) + DeviceExtensionSize;
    HANDLE DeviceDirHandle;

    *NewDevice = NULL;

    if (DeviceName != NULL) {
        USHORT Len = 0;
        while (DeviceName[Len] != '\0') Len++;
        
        NameAnsi.Buffer = DeviceName;
        NameAnsi.Length = Len;
        NameAnsi.MaximumLength = Len + 1;

        if (DeviceName[0] == '/')
        {
            // Path Absolut (misal: "/FileSystem/VeaFat" atau "/Device/Harddisk0")
            // RootDirectory diset NULL agar ditelusuri dari Root (/)
            InitializeObjectAttributes(&Attr, &NameAnsi, OBJ_CASE_INSENSITIVE, NULL, NULL);
        }
        else
        {
            // Path Relatif (misal: "Harddisk0" atau "VeaFat")
            // Buka handle /Device sebagai RootDirectory default
            OBJECT_ATTRIBUTES ObjAttr;
            ANSI_STRING DevDirString;

            RtlInitAnsiString(&DevDirString, "/Device");
            InitializeObjectAttributes(&ObjAttr, &DevDirString, OBJ_CASE_INSENSITIVE, NULL, NULL);

            Status = ObOpenObjectByName(
                &ObjAttr,
                ObpDirectoryObjectType,
                KernelMode,
                0x10000,
                NULL,
                &DeviceDirHandle
            );

            if (!VEA_SUCCESS(Status))
            {
                DPRINT("ERROR: Failed to open /Device directory: 0x%x\n", Status);
                return Status;
            }

            InitializeObjectAttributes(&Attr, &NameAnsi, OBJ_CASE_INSENSITIVE, DeviceDirHandle, NULL);
        }
    }

    Status = ObCreateObject(
        KernelMode,
        ObpDeviceObjectType, 
        (DeviceName != NULL) ? &Attr : NULL,
        KernelMode,
        NULL,
        TotalBodySize,
        0,
        0,
        (PVOID*)&CreatedDevice
    );

    if (!VEA_SUCCESS(Status)) {
        DPRINT("ERROR: Status: 0x%x.\n", Status);
        return Status;
    }

    if (DeviceExtensionSize > 0) {
        CreatedDevice->DeviceExtension = (PVOID)(CreatedDevice + 1);
    } else {
        CreatedDevice->DeviceExtension = NULL;
    }

    CreatedDevice->DriverObject = DriverObject;
    CreatedDevice->DeviceType = DeviceType;
    CreatedDevice->NextDevice = NULL;
    CreatedDevice->Flags |= DO_DEVICE_INITIALIZING;
    InitializeListHead(&CreatedDevice->Queue.ListEntry);
    
    if (ObioDeviceTypeIsStorage(CreatedDevice))
    {
        PVPB LocalVpb = ObioAllocateVpb(CreatedDevice);
        if (LocalVpb)
        {
            CreatedDevice->Vpb = LocalVpb;
        }
        else
        {
            Status = STATUS_IO_ALLOCATE_FAILED;
            ObDereferenceObject(CreatedDevice);
            return Status;
        }
        
        KsInitializeEvent(&CreatedDevice->DeviceLock, SynchronizationEvent, TRUE);
    }

    if (ObioDeviceTypeIsFileSystem(CreatedDevice))
    {
        InitializeListHead(&CreatedDevice->Queue.ListEntry);
    }

    if (DriverObject->InterruptHandler)
    {
        Status = ObioAttachInterruptToThisObject(CreatedDevice, DriverObject->InterruptHandler);

        if (!VEA_SUCCESS(Status))
        {
            DPRINT("[OB] ObioAttachInterruptToThisObject() failed: 0x%x\n", Status);
            ObDereferenceObject(CreatedDevice);
            return Status;
        }
    }

    // Masukkan ke Object Tree jika memiliki nama
    if (DeviceName != NULL) {
        Status = ObInsertObject(
            CreatedDevice,
            NULL,   // AccessState
            0,      // DesiredAccess
            0,      // ObjectPointerBias
            NULL,   // NewObject
            NULL    // Handle
        );

        if (!VEA_SUCCESS(Status)) {
            DPRINT("ERROR: ObInsertObject failed: 0x%x.\n", Status);
            return Status;
        }

        VeaClose(DeviceDirHandle);
    }

    if (DriverObject->DeviceObject == NULL) {
        DriverObject->DeviceObject = CreatedDevice;
    } else {
        PDEVICE_OBJECT Current = DriverObject->DeviceObject;
        while (Current->NextDevice != NULL) {
            Current = Current->NextDevice;
        }
        Current->NextDevice = CreatedDevice;
    }

    *NewDevice = CreatedDevice;
    
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObioCreateDriver(
    PCHAR DriverName,
    PDRIVER_INITIALIZE InitializationFunction,
    OUT PDRIVER_OBJECT *DriverObject
)
{
    HANDLE DriverHandle;
    if (!DriverName || !InitializationFunction) {
        DPRINT("ERROR: No DriverName or InitializationFunction\n");
        return STATUS_INVALID_PARAMETER;
    }

    PDRIVER_OBJECT NewDriver = NULL;
    VEASTATUS Status;

    // 1. Siapkan Atribut untuk menaruh driver di /Driver
    ANSI_STRING NameAnsi;
    USHORT Len = 0;
    while (DriverName[Len] != '\0') Len++;
    
    NameAnsi.Buffer = DriverName;
    NameAnsi.Length = Len;
    NameAnsi.MaximumLength = Len + 1;

    OBJECT_ATTRIBUTES Attr, DriverAttr;
    ANSI_STRING DriverString;

    RtlInitAnsiString(&DriverString, "/Driver");
    InitializeObjectAttributes(&DriverAttr, &DriverString, 0, NULL, NULL);
    
    Status = ObOpenObjectByName(
        &DriverAttr,
        ObpDirectoryObjectType,
        KernelMode,
        0x10000,
        NULL,
        &DriverHandle
    );

    if (!VEA_SUCCESS(Status)) return Status;

    Attr.RootDirectory = DriverHandle;
    Attr.ObjectName = &NameAnsi;
    Attr.Attributes = OBJ_CASE_INSENSITIVE;
    Attr.SecurityDescriptor = NULL;
    Attr.SecurityQualityOfServiceLife = NULL;

    // 2. Buat Objek (Fase Alokasi)
    Status = ObCreateObject(
        KernelMode,
        ObpDriverObjectType,
        &Attr,
        KernelMode,
        NULL,
        sizeof(DRIVER_OBJECT),
        0,
        0,
        (PVOID*)&NewDriver
    );

    if (!VEA_SUCCESS(Status)) {
        return Status;
    }

    RtlZeroMemory(NewDriver, sizeof(DRIVER_OBJECT));

    // Inisialisasi dasar DRIVER_OBJECT
    NewDriver->DeviceObject = NULL;
    NewDriver->DriverStart = (PVOID)InitializationFunction;

    // 3. Masukkan ke dalam Object Tree (/Driver/<DriverName>)
    Status = ObInsertObject(
        NewDriver,
        NULL,   // AccessState
        0,      // DesiredAccess
        0,      // ObjectPointerBias
        NULL,   // NewObject
        NULL    // Handle
    );

    if (!VEA_SUCCESS(Status)) {
        return Status;
    }

    VeaClose(DriverHandle);

    // 4. EKSEKUSI DRIVER ENTRY!
    Status = InitializationFunction(NewDriver);

    if (!VEA_SUCCESS(Status)) {
        ObDereferenceObject(NewDriver);
        return Status;
    }
    
    if (DriverObject != NULL)
    {
        *DriverObject = NewDriver;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObioAttachInterruptToThisObject(
    PVOID Object,
    POBIO_INTERRUPT_HANDLER CustomHandler
)
{
    if (!Object || !CustomHandler) 
    {
        return STATUS_INVALID_PARAMETER;
    }

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Object);

    /* FIXME: Now we only support stacking Device and Driver object type
       to another object. But our architectural of VeaOS say any object
       can be attached and have it's own Interrupt Handler */
    if (Header->Type == ObpDeviceObjectType) 
    {
        PDEVICE_OBJECT Device = (PDEVICE_OBJECT)Object;
        Device->InterruptHandler = CustomHandler;
        return STATUS_SUCCESS;
    } 
    else if (Header->Type == ObpDriverObjectType) 
    {
        PDRIVER_OBJECT Driver = (PDRIVER_OBJECT)Object;
        Driver->InterruptHandler = CustomHandler;
        return STATUS_SUCCESS;
    }

    return STATUS_OBJECT_TYPE_MISMATCH;
}

PVOID
VEAPI
ObioAttachObject(
    IN PVOID SourceObject,
    IN PVOID TargetObject
)
{
    if (!SourceObject || !TargetObject || (SourceObject == TargetObject)) 
        return NULL;

    /* Acquire Spinlock */
    KIRQL OldIrql;
    KsAcquireSpinLock(&ObioMgrSpinLock, &OldIrql);

    POBJECT_HEADER SourceHeader = OBJECT_TO_OBJECT_HEADER(SourceObject);
    POBJECT_HEADER TargetHeader = OBJECT_TO_OBJECT_HEADER(TargetObject);

    if (!SourceHeader->Type || !SourceHeader->Type->AllowAttachByDefault ||
        !TargetHeader->Type || !TargetHeader->Type->AllowAttachByDefault)
    {
        DPRINT("OBIO: Cannot attach object! ObjectType does not allow attachment.\n\r");
        KsReleaseSpinLock(&ObioMgrSpinLock, OldIrql);
        return NULL;
    }

    if (SourceHeader->LowerAttachedObject != NULL)
    {
        // Source udah nempel di object lain -- gak boleh attach dobel
        KsReleaseSpinLock(&ObioMgrSpinLock, OldIrql);
        return NULL;
    }

    // Cari puncak stack TargetObject sekarang (mungkin TargetObject
    // sendiri udah ada yang numpuk di atasnya)
    PVOID CurrentTop = TargetObject;
    POBJECT_HEADER CurrentTopHeader = TargetHeader;

    while (CurrentTopHeader->UpperAttachedObject != NULL)
    {
        CurrentTop = CurrentTopHeader->UpperAttachedObject;
        CurrentTopHeader = OBJECT_TO_OBJECT_HEADER(CurrentTop);
    }

    SourceHeader->LowerAttachedObject = CurrentTop;
    CurrentTopHeader->UpperAttachedObject = SourceObject;
    SourceHeader->StackDepth = CurrentTopHeader->StackDepth + 1;

    ObReferenceObject(CurrentTop);
    KsReleaseSpinLock(&ObioMgrSpinLock, OldIrql);

    return CurrentTop;
}

VOID
VEAPI
ObioDetachObject(
    IN PVOID Object
)
{
    if (!Object) return;

    KIRQL OldIrql;
    PVOID LowerObjectToDereference = NULL;

    KsAcquireSpinLock(&ObioMgrSpinLock, &OldIrql);

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Object);

    // SAFETY CHECK: Pastikan tidak ada objek di atasnya
    if (Header->UpperAttachedObject != NULL)
    {
        kdp_print("OBIO FATAL: Attempting to detach an object that still has attached upper devices!\n\r");
        KsReleaseSpinLock(&ObioMgrSpinLock, OldIrql);
        KsBugCheckEx(INVALID_OBJECT_DETACH, (ULONG_PTR)Object, 0, 0, 0); 
        return;
    }

    if (Header->LowerAttachedObject != NULL)
    {
        POBJECT_HEADER LowerHeader = OBJECT_TO_OBJECT_HEADER(Header->LowerAttachedObject);
        LowerHeader->UpperAttachedObject = NULL;

        // Simpan pointer untuk di-dereference NANTI di luar lock
        LowerObjectToDereference = Header->LowerAttachedObject;
        Header->LowerAttachedObject = NULL;
    }

    Header->StackDepth = 1; // balik jadi standalone

    // Lepas Spinlock DULUAN!
    KsReleaseSpinLock(&ObioMgrSpinLock, OldIrql);

    // Sekarang aman untuk dereference di luar Spinlock
    if (LowerObjectToDereference != NULL)
    {
        ObDereferenceObject(LowerObjectToDereference);
    }
}

VOID
VEAPI
ObioDeleteDevice(IN PDEVICE_OBJECT DeviceObject)
{
    UNIMPLEMENTED_DBGBREAK;
}


VEASTATUS
VEAPI
ObioRegisterDeviceInterface(
    IN PDEVICE_OBJECT PhysicalDeviceObject,
    IN const GUID *InterfaceClassGuid,
    IN PANSI_STRING ReferenceString OPTIONAL,
    OUT PANSI_STRING SymbolicLinkName
)
{
    VEASTATUS Status;
    VEASTATUS LinkStatus;
    ANSI_STRING GuidString;
    ANSI_STRING KeyName;
    ANSI_STRING ValueName;
    ANSI_STRING ClassPath;
    OBJECT_ATTRIBUTES Attributes;
    HANDLE CurrentControlSetHandle = NULL;
    HANDLE ControlHandle = NULL;
    HANDLE DeviceClassesHandle = NULL;
    HANDLE ClassHandle = NULL;
    HANDLE InterfaceHandle = NULL;
    HANDLE ReferenceHandle = NULL;
    ULONG Disposition;
    ULONG InterfaceNameLength;
    ULONG LinkNameLength;
    ULONG OutputLength;
    ULONG i;
    PANSI_STRING InstancePath;
    PCHAR AllocatedSymbolicLinkName = NULL;
    CHAR InterfaceNameBuffer[256];
    CHAR ReferenceNameBuffer[256];
    ULONG ReferenceNameLength;
    CHAR LinkNameBuffer[256];
    CHAR OutputNameBuffer[258];
    BOOLEAN GuidStringAllocated = FALSE;
    BOOLEAN LinkCreated = FALSE;

    DPRINT("IoRegisterDeviceInterface(): PDO %p, RefString: %s\n",
        PhysicalDeviceObject,
        ReferenceString && ReferenceString->Buffer
            ? ReferenceString->Buffer
            : "");

    if (!PhysicalDeviceObject || !InterfaceClassGuid ||
        !SymbolicLinkName ||
        (ReferenceString && ReferenceString->Length && !ReferenceString->Buffer))
    {
        DPRINT("[Obio] DEVINTERFACE: Invalid Parameter\n");
        return STATUS_INVALID_PARAMETER;
    }

    if (SymbolicLinkName->Buffer != NULL)
    {
        DPRINT("[Obio] DEVINTERFACE: SymbolicLinkName->Buffer exist\n");
        return STATUS_INVALID_PARAMETER;
    }

    SymbolicLinkName->Length = 0;
    SymbolicLinkName->MaximumLength = 0;

    if (PhysicalDeviceObject->DeviceNode == NULL)
    {
        DPRINT("[Obio] DEVINTERFACE: DeviceNode isn't exist\n");
        DPRINT("PDO %p has no DeviceNode\n", PhysicalDeviceObject);
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    InstancePath = &PhysicalDeviceObject->DeviceNode->InstancePath;
    if (!InstancePath->Buffer || InstancePath->Length == 0)
    {
        DPRINT("[Obio] DEVINTERFACE: DeviceNode instance path is empty\n");
        DPRINT("PDO %p has an empty device instance path\n",
               PhysicalDeviceObject);
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (ReferenceString != NULL)
    {
        for (i = 0; i < ReferenceString->Length; i++)
        {
            if ((ReferenceString->Buffer[i] == '\\') ||
                (ReferenceString->Buffer[i] == '/'))
            {
                DPRINT("Interface reference string contains a path separator\n");
                DPRINT("[Obio] DEVINTERFACE: Reference string contains path separator\n");
                return STATUS_INVALID_DEVICE_REQUEST;
            }
        }
    }

    Status = RtlStringFromGUID(InterfaceClassGuid, &GuidString);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("RtlStringFromGUID() failed with status 0x%08lx\n", Status);
        DPRINT("[Obio] DEVINTERFACE: RtlStringFromGUID failed\n");
        return Status;
    }

    GuidStringAllocated = TRUE;

    if (InstancePath->Length > sizeof(InterfaceNameBuffer) - 6 -
                               GuidString.Length)
    {
        Status = STATUS_NAME_TOO_LONG;
        goto Cleanup;
    }

    InterfaceNameBuffer[0] = '#';
    InterfaceNameBuffer[1] = '#';
    InterfaceNameBuffer[2] = '?';
    InterfaceNameBuffer[3] = '#';
    InterfaceNameLength = 4;
    for (i = 0; i < InstancePath->Length; i++)
    {
        CHAR Character = InstancePath->Buffer[i];
        InterfaceNameBuffer[InterfaceNameLength++] =
            (Character == '\\' || Character == '/') ? '#' : Character;
    }
    InterfaceNameBuffer[InterfaceNameLength++] = '#';
    RtlCopyMemory(&InterfaceNameBuffer[InterfaceNameLength],
                  GuidString.Buffer,
                  GuidString.Length);
    InterfaceNameLength += GuidString.Length;
    InterfaceNameBuffer[InterfaceNameLength] = '\0';

    ReferenceNameBuffer[0] = '#';
    ReferenceNameLength = 1;
    if (ReferenceString && ReferenceString->Length)
    {
        if (ReferenceString->Length > sizeof(ReferenceNameBuffer) - 2)
        {
            Status = STATUS_NAME_TOO_LONG;
            goto Cleanup;
        }
        RtlCopyMemory(&ReferenceNameBuffer[ReferenceNameLength],
                    ReferenceString->Buffer,
                    ReferenceString->Length);
        ReferenceNameLength += ReferenceString->Length;
    }
    ReferenceNameBuffer[ReferenceNameLength] = '\0';

    LinkNameLength = 0;
    for (i = 0; i < InstancePath->Length; i++)
    {
        CHAR Character = InstancePath->Buffer[i];
        LinkNameBuffer[LinkNameLength++] =
            (Character == '\\' || Character == '/') ? '#' : Character;
    }
    LinkNameBuffer[LinkNameLength++] = '#';
    RtlCopyMemory(&LinkNameBuffer[LinkNameLength],
                  GuidString.Buffer,
                  GuidString.Length);
    LinkNameLength += GuidString.Length;
    LinkNameBuffer[LinkNameLength] = '\0';

    OutputLength = LinkNameLength + 1;
    if (ReferenceString && ReferenceString->Length)
    {
        if (OutputLength + 1 + ReferenceString->Length >=
            sizeof(OutputNameBuffer))
        {
            Status = STATUS_NAME_TOO_LONG;
            goto Cleanup;
        }
    }

    OutputNameBuffer[0] = '/';
    RtlCopyMemory(&OutputNameBuffer[1], LinkNameBuffer, LinkNameLength);
    if (ReferenceString && ReferenceString->Length)
    {
        OutputNameBuffer[OutputLength++] = '/';
        RtlCopyMemory(&OutputNameBuffer[OutputLength],
                      ReferenceString->Buffer,
                      ReferenceString->Length);
        OutputLength += ReferenceString->Length;
    }
    if (OutputLength >= sizeof(OutputNameBuffer) || OutputLength > 0xFFFE)
    {
        Status = STATUS_NAME_TOO_LONG;
        goto Cleanup;
    }
    OutputNameBuffer[OutputLength] = '\0';
    OutputLength++;

    AllocatedSymbolicLinkName = UlAllocatePoolWithTag(
        PagedPool,
        OutputLength,
        TAG_RTL_STRING
    );
    if (!AllocatedSymbolicLinkName)
    {
        Status = STATUS_INSUFFICIENT_MEMORY;
        goto Cleanup;
    }
    RtlCopyMemory(AllocatedSymbolicLinkName, OutputNameBuffer, OutputLength);

    RtlInitAnsiString(&ClassPath,
                      "/REGISTRY/MACHINE/SYSTEM/CurrentControlSet");
    Status = ObioOpenVeaKey(NULL,
                            &CurrentControlSetHandle,
                            &ClassPath,
                            KEY_ALL_ACCESS);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[Obio] DEVINTERFACE: Failed to open CCS\n");
        DPRINT("Failed to open CurrentControlSet: 0x%08lx\n", Status);
        goto Cleanup;
    }

    RtlInitAnsiString(&KeyName, "Control");
    InitializeObjectAttributes(&Attributes, &KeyName, 0,
                               CurrentControlSetHandle, NULL);
    Status = VeaCreateKey(&ControlHandle, KEY_ALL_ACCESS, &Attributes,
                          0, NULL, REG_OPTION_NON_VOLATILE, &Disposition);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[Obio] DEVINTERFACE: Failed to open Control\n");
        DPRINT("Failed to create/open DeviceClasses parent: 0x%08lx\n", Status);
        goto Cleanup;
    }

    RtlInitAnsiString(&KeyName, "DeviceClasses");
    InitializeObjectAttributes(&Attributes, &KeyName, 0, ControlHandle, NULL);
    Status = VeaCreateKey(&DeviceClassesHandle, KEY_ALL_ACCESS, &Attributes,
                          0, NULL, REG_OPTION_NON_VOLATILE, &Disposition);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[Obio] DEVINTERFACE: Failed to open DeviceClasses\n");
        DPRINT("Failed to create/open DeviceClasses: 0x%08lx\n", Status);
        goto Cleanup;
    }

    InitializeObjectAttributes(&Attributes, &GuidString, 0,
                               DeviceClassesHandle, NULL);
    Status = VeaCreateKey(&ClassHandle, KEY_ALL_ACCESS, &Attributes,
                          0, NULL, REG_OPTION_NON_VOLATILE, &Disposition);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[Obio] DEVINTERFACE: Failed to open Interface Class key\n");
        DPRINT("Failed to create/open interface class key: 0x%08lx\n", Status);
        goto Cleanup;
    }

    KeyName.Buffer = InterfaceNameBuffer;
    KeyName.Length = (USHORT)InterfaceNameLength;
    KeyName.MaximumLength = (USHORT)(InterfaceNameLength + 1);
    InitializeObjectAttributes(&Attributes, &KeyName, 0, ClassHandle, NULL);
    Status = VeaCreateKey(&InterfaceHandle, KEY_ALL_ACCESS, &Attributes,
                          0, NULL, REG_OPTION_NON_VOLATILE, &Disposition);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[Obio] DEVINTERFACE: Failed to open instance KEY\n");
        DPRINT("Failed to create/open interface instance key: 0x%08lx\n",
               Status);
        goto Cleanup;
    }

    RtlInitAnsiString(&ValueName, "DeviceInstance");
    Status = VeaSetValueKey(InterfaceHandle, &ValueName, 0, FILE_KEY_STRING,
                            InstancePath->Buffer, InstancePath->Length);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[Obio] DEVINTERFACE: Failed to write DeviceInstance\n");
        DPRINT("Failed to write DeviceInstance value: 0x%08lx\n", Status);
        goto Cleanup;
    }

    KeyName.Buffer = ReferenceNameBuffer;
    KeyName.Length = (USHORT)ReferenceNameLength;
    KeyName.MaximumLength = (USHORT)(ReferenceNameLength + 1);
    InitializeObjectAttributes(&Attributes, &KeyName, 0, InterfaceHandle, NULL);

    Status = VeaCreateKey(&ReferenceHandle, KEY_ALL_ACCESS, &Attributes,
                          0, NULL, REG_OPTION_NON_VOLATILE, &Disposition);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[Obio] DEVINTERFACE: Failed to create/open interface refrence key\n");
        DPRINT("Failed to create/open interface reference key: 0x%08lx\n",
               Status);
        goto Cleanup;
    }

    LinkStatus = ObCreateSymbolicLinkToObject(ObpRootDirectoryObject,
                                              LinkNameBuffer,
                                              PhysicalDeviceObject);
    if (LinkStatus == STATUS_OBJECT_NAME_COLLISION)
    {
        DPRINT("[Obio] DEVINTERFACE: Object Name Collision\n");
        Status = ObDeleteSymbolicLink(ObpRootDirectoryObject, LinkNameBuffer);
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("[Obio] DEVINTEFACE: Failed to replace existing interface symbolic link: 0x%08lx\n",
                   Status);
            goto Cleanup;
        }
        LinkStatus = ObCreateSymbolicLinkToObject(ObpRootDirectoryObject,
                                                  LinkNameBuffer,
                                                  PhysicalDeviceObject);
    }
    if (!VEA_SUCCESS(LinkStatus))
    {
        DPRINT("[Obio] DEVINTERFACE: Failed to create Interface Symbolic Link\n");
        DPRINT("Failed to create interface symbolic link: 0x%08lx\n",
               LinkStatus);
        Status = LinkStatus;
        goto Cleanup;
    }
    LinkCreated = TRUE;

    RtlInitAnsiString(&ValueName, "SymbolicLink");
    Status = VeaSetValueKey(ReferenceHandle, &ValueName, 0, FILE_KEY_STRING,
                            OutputNameBuffer, OutputLength - 1);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[Obio] DEVINTERFACE: Failed to write Symbolic Link value\n");
        DPRINT("Failed to write SymbolicLink value: 0x%08lx\n", Status);
        LinkStatus = ObDeleteSymbolicLink(ObpRootDirectoryObject,
                                          LinkNameBuffer);
        if (!VEA_SUCCESS(LinkStatus))
        {
            DPRINT("Failed to roll back interface symbolic link: 0x%08lx\n",
                   LinkStatus);
        }
        LinkCreated = FALSE;
        goto Cleanup;
    }

    SymbolicLinkName->Buffer = AllocatedSymbolicLinkName;
    SymbolicLinkName->Length = (USHORT)(OutputLength - 1);
    SymbolicLinkName->MaximumLength = (USHORT)OutputLength;
    AllocatedSymbolicLinkName = NULL;
    Status = STATUS_SUCCESS;

Cleanup:
    if (ReferenceHandle)
    {
        VeaClose(ReferenceHandle);
    }
    if (InterfaceHandle)
    {
        VeaClose(InterfaceHandle);
    }
    if (ClassHandle)
    {
        VeaClose(ClassHandle);
    }
    if (DeviceClassesHandle)
    {
        VeaClose(DeviceClassesHandle);
    }
    if (ControlHandle)
    {
        VeaClose(ControlHandle);
    }
    if (CurrentControlSetHandle)
    {
        VeaClose(CurrentControlSetHandle);
    }
    if (GuidStringAllocated)
    {
        UlFreePoolWithTag(GuidString.Buffer, TAG_USTR);
    }
    if (AllocatedSymbolicLinkName)
    {
        UlFreePoolWithTag(AllocatedSymbolicLinkName, TAG_RTL_STRING);
    }
    if (!LinkCreated)
    {
        SymbolicLinkName->Length = 0;
        SymbolicLinkName->MaximumLength = 0;
    }

    if (!VEA_SUCCESS(Status)) 
    {
        DPRINT("[Obio] DEVINTERFACE: Failed: 0x%x\n", Status);
    }

    return Status;
}

static
BOOLEAN
ObioAnsiNameEquals(
    IN PCHAR Name,
    IN USHORT NameLength,
    IN PCHAR ExpectedName,
    IN ULONG ExpectedLength
)
{
    ULONG i;

    if ((ULONG)NameLength != ExpectedLength)
    {
        return FALSE;
    }

    for (i = 0; i < ExpectedLength; i++)
    {
        CHAR Left = Name[i];
        CHAR Right = ExpectedName[i];

        if (Left >= 'A' && Left <= 'Z')
        {
            Left += 'a' - 'A';
        }
        if (Right >= 'A' && Right <= 'Z')
        {
            Right += 'a' - 'A';
        }
        if (Left != Right)
        {
            return FALSE;
        }
    }

    return TRUE;
}

static
PKEY_DIRECTORY_NODE
ObioFindSubkey(
    IN PVK_HIVE Hive,
    IN PKEY_DIRECTORY_NODE ParentNode,
    IN PCHAR Name,
    IN ULONG NameLength,
    OUT PBOOLEAN Corrupt
)
{
    ULONG ChildOffset;

    *Corrupt = FALSE;
    ChildOffset = ParentNode->FirstSubKeyOffset;
    while (ChildOffset != 0)
    {
        PKEY_DIRECTORY_NODE ChildNode = (PKEY_DIRECTORY_NODE)
            VkpGetCellPointer(Hive, ChildOffset);

        if (!ChildNode || ChildNode->Signature != KEY_DIR_SIG)
        {
            *Corrupt = TRUE;
            return NULL;
        }

        if (ObioAnsiNameEquals(ChildNode->Name,
                               ChildNode->NameLength,
                               Name,
                               NameLength))
        {
            return ChildNode;
        }

        ChildOffset = ChildNode->NextSiblingOffset;
    }

    return NULL;
}

static
VEASTATUS
ObioGetKeyValueData(
    IN PVK_HIVE Hive,
    IN PKEY_DIRECTORY_NODE KeyNode,
    IN PCHAR ValueName,
    OUT PKEY_FILE_NODE *ValueNode,
    OUT PVOID *ValueData
)
{
    ANSI_STRING Name;
    PKEY_FILE_NODE FoundValue;
    PVOID Data;

    RtlInitAnsiString(&Name, ValueName);
    FoundValue = VkpFindValueNode(Hive, KeyNode, &Name, NULL);
    if (!FoundValue)
    {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    if (FoundValue->Signature != KEY_FILE_SIG)
    {
        return STATUS_UNSUCCESSFUL;
    }

    if ((FoundValue->Flags & KEY_FILE_INLINE) ||
        FoundValue->DataLength <= sizeof(ULONG))
    {
        Data = &FoundValue->DataPOffset;
    }
    else
    {
        PVOID DataCell = VkpGetCellPointer(Hive, FoundValue->DataPOffset);
        if (!DataCell)
        {
            return STATUS_UNSUCCESSFUL;
        }
        Data = (PUCHAR)DataCell + sizeof(LONG);
    }

    *ValueNode = FoundValue;
    *ValueData = Data;
    return STATUS_SUCCESS;
}

static
VEASTATUS
ObioEnsureMultiSzCapacity(
    IN OUT PCHAR *Buffer,
    IN OUT PULONG Capacity,
    IN ULONG Required
)
{
    ULONG NewCapacity;
    PCHAR NewBuffer;

    if (Required <= *Capacity)
    {
        return STATUS_SUCCESS;
    }

    NewCapacity = *Capacity;
    while (NewCapacity < Required)
    {
        if (NewCapacity > ((ULONG)-1) / 2)
        {
            NewCapacity = Required;
            break;
        }
        NewCapacity *= 2;
    }

    NewBuffer = UlAllocatePoolWithTag(PagedPool,
                                      NewCapacity,
                                      TAG_RTL_STRING);
    if (!NewBuffer)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    RtlCopyMemory(NewBuffer, *Buffer, *Capacity);
    UlFreePoolWithTag(*Buffer, TAG_RTL_STRING);
    *Buffer = NewBuffer;
    *Capacity = NewCapacity;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObioGetDeviceInterfaces(
    IN const GUID *InterfaceClassGuid,
    IN PDEVICE_OBJECT PhysicalDeviceObject OPTIONAL,
    IN ULONG Flags,
    OUT PCHAR *SymbolicLinkList
)
{
    static const CHAR ClassPathPrefix[] =
        "/REGISTRY/MACHINE/SYSTEM/CurrentControlSet/Control/DeviceClasses/";
    static const CHAR ControlName[] = "Control";
    static const CHAR LinkedName[] = "Linked";
    static const CHAR DeviceInstanceName[] = "DeviceInstance";
    static const CHAR SymbolicLinkValueName[] = "SymbolicLink";
    VEASTATUS Status;
    ANSI_STRING GuidString;
    ANSI_STRING ClassPath;
    HANDLE ClassHandle = NULL;
    PVK_KEY_OBJECT ClassKey = NULL;
    PANSI_STRING InstancePath = NULL;
    PCHAR ResultBuffer = NULL;
    ULONG ResultCapacity = 2;
    ULONG ResultLength = 0;
    ULONG PrefixLength;
    ULONG i;
    BOOLEAN GuidStringAllocated = FALSE;

    if (!InterfaceClassGuid || !SymbolicLinkList)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *SymbolicLinkList = NULL;
    if (PhysicalDeviceObject)
    {
        if (!PhysicalDeviceObject->DeviceNode ||
            !PhysicalDeviceObject->DeviceNode->InstancePath.Buffer ||
            PhysicalDeviceObject->DeviceNode->InstancePath.Length == 0)
        {
            DPRINT("PDO %p has no usable device instance path\n",
                   PhysicalDeviceObject);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        InstancePath = &PhysicalDeviceObject->DeviceNode->InstancePath;
    }

    ResultBuffer = UlAllocatePoolWithTag(PagedPool,
                                         ResultCapacity,
                                         TAG_RTL_STRING);
    if (!ResultBuffer)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }
    ResultBuffer[0] = '\0';
    ResultBuffer[1] = '\0';

    RtlZeroMemory(&GuidString, sizeof(GuidString));
    Status = RtlStringFromGUID(InterfaceClassGuid, &GuidString);
    if (!VEA_SUCCESS(Status))
    {
        goto Cleanup;
    }
    GuidStringAllocated = TRUE;

    PrefixLength = (ULONG)RtlRawStringLength((PCHAR)ClassPathPrefix);
    if (PrefixLength + GuidString.Length >= 256)
    {
        Status = STATUS_NAME_TOO_LONG;
        goto Cleanup;
    }

    ClassPath.Buffer = (PCHAR)UlAllocatePoolWithTag(
        PagedPool,
        PrefixLength + GuidString.Length + 1,
        TAG_RTL_STRING);
    if (!ClassPath.Buffer)
    {
        Status = STATUS_INSUFFICIENT_MEMORY;
        goto Cleanup;
    }
    RtlCopyMemory(ClassPath.Buffer, (PVOID)ClassPathPrefix, PrefixLength);
    RtlCopyMemory(ClassPath.Buffer + PrefixLength,
                  GuidString.Buffer,
                  GuidString.Length);
    ClassPath.Length = (USHORT)(PrefixLength + GuidString.Length);
    ClassPath.MaximumLength = (USHORT)(ClassPath.Length + 1);
    ClassPath.Buffer[ClassPath.Length] = '\0';

    Status = ObioOpenVeaKey(NULL,
                            &ClassHandle,
                            &ClassPath,
                            KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS);
    UlFreePoolWithTag(ClassPath.Buffer, TAG_RTL_STRING);
    ClassPath.Buffer = NULL;
    if (Status == STATUS_OBJECT_NAME_NOT_FOUND)
    {
        Status = STATUS_SUCCESS;
        goto BuildEmptyResult;
    }
    if (!VEA_SUCCESS(Status))
    {
        goto Cleanup;
    }

    Status = ObReferenceObjectByHandle(ClassHandle,
                                       KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS,
                                       VkKeyObjectType,
                                       KernelMode,
                                       (PVOID *)&ClassKey);
    if (!VEA_SUCCESS(Status))
    {
        goto Cleanup;
    }
    if (!ClassKey->HiveBase || !ClassKey->KeyNode ||
        ClassKey->KeyNode->Signature != KEY_DIR_SIG)
    {
        Status = STATUS_OBJECT_TYPE_MISMATCH;
        goto Cleanup;
    }

    {
        PVK_HIVE Hive = ClassKey->HiveBase;
        PKEY_DIRECTORY_NODE InterfaceNode = ClassKey->KeyNode;
        ULONG InterfaceOffset = InterfaceNode->FirstSubKeyOffset;
        BOOLEAN FoundMatchingDevice = FALSE;

        while (InterfaceOffset != 0)
        {
            PKEY_DIRECTORY_NODE DeviceNode = (PKEY_DIRECTORY_NODE)
                VkpGetCellPointer(Hive, InterfaceOffset);
            ULONG ReferenceOffset;
            BOOLEAN Corrupt;
            PKEY_FILE_NODE ValueNode;
            PVOID ValueData;

            if (!DeviceNode || DeviceNode->Signature != KEY_DIR_SIG)
            {
                Status = STATUS_UNSUCCESSFUL;
                goto Cleanup;
            }

            if (InstancePath)
            {
                Status = ObioGetKeyValueData(Hive,
                                             DeviceNode,
                                             (PCHAR)DeviceInstanceName,
                                             &ValueNode,
                                             &ValueData);
                if (Status == STATUS_OBJECT_NAME_NOT_FOUND)
                {
                    InterfaceOffset = DeviceNode->NextSiblingOffset;
                    continue;
                }
                if (!VEA_SUCCESS(Status))
                {
                    goto Cleanup;
                }
                if (ValueNode->Type != FILE_KEY_STRING ||
                    ValueNode->DataLength != InstancePath->Length)
                {
                    InterfaceOffset = DeviceNode->NextSiblingOffset;
                    continue;
                }
                if (RtlCompareMemory(ValueData,
                                     InstancePath->Buffer,
                                     InstancePath->Length) !=
                    InstancePath->Length)
                {
                    InterfaceOffset = DeviceNode->NextSiblingOffset;
                    continue;
                }
                FoundMatchingDevice = TRUE;
            }

            ReferenceOffset = DeviceNode->FirstSubKeyOffset;
            while (ReferenceOffset != 0)
            {
                PKEY_DIRECTORY_NODE ReferenceNode = (PKEY_DIRECTORY_NODE)
                    VkpGetCellPointer(Hive, ReferenceOffset);
                PKEY_DIRECTORY_NODE ControlNode;
                PKEY_FILE_NODE LinkedNode;
                PVOID LinkedData;
                PKEY_FILE_NODE SymbolicLinkNode;
                PVOID SymbolicLinkData;
                ULONG LinkLength;

                if (!ReferenceNode || ReferenceNode->Signature != KEY_DIR_SIG)
                {
                    Status = STATUS_UNSUCCESSFUL;
                    goto Cleanup;
                }

                if (ObioAnsiNameEquals(ReferenceNode->Name,
                                       ReferenceNode->NameLength,
                                       (PCHAR)ControlName,
                                       sizeof(ControlName) - 1))
                {
                    ReferenceOffset = ReferenceNode->NextSiblingOffset;
                    continue;
                }

                ControlNode = ObioFindSubkey(Hive,
                                             ReferenceNode,
                                             (PCHAR)ControlName,
                                             sizeof(ControlName) - 1,
                                             &Corrupt);
                if (Corrupt)
                {
                    Status = STATUS_UNSUCCESSFUL;
                    goto Cleanup;
                }

                if (!(Flags & DEVICE_INTERFACE_INCLUDE_NONACTIVE) &&
                    ControlNode)
                {
                    Status = ObioGetKeyValueData(Hive,
                                                 ControlNode,
                                                 (PCHAR)LinkedName,
                                                 &LinkedNode,
                                                 &LinkedData);
                    if (Status != STATUS_OBJECT_NAME_NOT_FOUND &&
                        !VEA_SUCCESS(Status))
                    {
                        goto Cleanup;
                    }
                    if (VEA_SUCCESS(Status))
                    {
                        ULONG LinkedValue;
                        if (LinkedNode->Type != FILE_KEY_DWORD ||
                            LinkedNode->DataLength != sizeof(ULONG))
                        {
                            Status = STATUS_UNSUCCESSFUL;
                            goto Cleanup;
                        }
                        RtlCopyMemory(&LinkedValue, LinkedData, sizeof(ULONG));
                        if (LinkedValue == 0)
                        {
                            ReferenceOffset = ReferenceNode->NextSiblingOffset;
                            continue;
                        }
                    }
                }

                Status = ObioGetKeyValueData(Hive,
                                             ReferenceNode,
                                             (PCHAR)SymbolicLinkValueName,
                                             &SymbolicLinkNode,
                                             &SymbolicLinkData);
                if (Status == STATUS_OBJECT_NAME_NOT_FOUND)
                {
                    ReferenceOffset = ReferenceNode->NextSiblingOffset;
                    continue;
                }
                if (!VEA_SUCCESS(Status))
                {
                    goto Cleanup;
                }
                if (SymbolicLinkNode->Type != FILE_KEY_STRING ||
                    SymbolicLinkNode->DataLength == 0)
                {
                    Status = STATUS_UNSUCCESSFUL;
                    goto Cleanup;
                }

                LinkLength = SymbolicLinkNode->DataLength;
                if (((PCHAR)SymbolicLinkData)[LinkLength - 1] == '\0')
                {
                    LinkLength--;
                }
                for (i = 0; i < LinkLength; i++)
                {
                    if (((PCHAR)SymbolicLinkData)[i] == '\0')
                    {
                        Status = STATUS_UNSUCCESSFUL;
                        goto Cleanup;
                    }
                }
                if (LinkLength == 0 ||
                    LinkLength > (ULONG)-1 - 2 ||
                    ResultLength > (ULONG)-1 - LinkLength - 2)
                {
                    Status = STATUS_NAME_TOO_LONG;
                    goto Cleanup;
                }

                Status = ObioEnsureMultiSzCapacity(&ResultBuffer,
                                                   &ResultCapacity,
                                                   ResultLength + LinkLength + 2);
                if (!VEA_SUCCESS(Status))
                {
                    goto Cleanup;
                }
                RtlCopyMemory(ResultBuffer + ResultLength,
                              SymbolicLinkData,
                              LinkLength);
                ResultLength += LinkLength;
                ResultBuffer[ResultLength++] = '\0';

                ReferenceOffset = ReferenceNode->NextSiblingOffset;
            }

            InterfaceOffset = DeviceNode->NextSiblingOffset;
            if (InstancePath && FoundMatchingDevice)
            {
                break;
            }
        }
    }

BuildEmptyResult:
    Status = ObioEnsureMultiSzCapacity(&ResultBuffer,
                                       &ResultCapacity,
                                       ResultLength + 1);
    if (!VEA_SUCCESS(Status))
    {
        goto Cleanup;
    }
    ResultBuffer[ResultLength] = '\0';
    *SymbolicLinkList = ResultBuffer;
    ResultBuffer = NULL;
    Status = STATUS_SUCCESS;

Cleanup:
    if (ClassKey)
    {
        ObDereferenceObject(ClassKey);
    }
    if (ClassHandle)
    {
        VeaClose(ClassHandle);
    }
    if (GuidStringAllocated)
    {
        UlFreePoolWithTag(GuidString.Buffer, TAG_USTR);
    }
    if (ResultBuffer)
    {
        UlFreePoolWithTag(ResultBuffer, TAG_RTL_STRING);
    }
    return Status;
}

static
BOOLEAN
ObioParseGuidField(
    IN PCHAR String,
    IN ULONG Length,
    OUT PULONG Value
)
{
    ULONG i;
    ULONG ParsedValue = 0;

    for (i = 0; i < Length; i++)
    {
        CHAR Character = String[i];
        ULONG Digit;

        if (Character >= '0' && Character <= '9')
        {
            Digit = Character - '0';
        }
        else if (Character >= 'a' && Character <= 'f')
        {
            Digit = Character - 'a' + 10;
        }
        else if (Character >= 'A' && Character <= 'F')
        {
            Digit = Character - 'A' + 10;
        }
        else
        {
            return FALSE;
        }

        ParsedValue = (ParsedValue << 4) | Digit;
    }

    *Value = ParsedValue;
    return TRUE;
}

static
BOOLEAN
ObioParseGuidString(
    IN PCHAR String,
    OUT PGUID Guid
)
{
    ULONG Value;
    ULONG i;

    if (String[0] != '{' || String[9] != '-' ||
        String[14] != '-' || String[19] != '-' ||
        String[24] != '-' || String[37] != '}')
    {
        return FALSE;
    }

    if (!ObioParseGuidField(String + 1, 8, &Guid->Data1))
    {
        return FALSE;
    }

    if (!ObioParseGuidField(String + 10, 4, &Value))
    {
        return FALSE;
    }
    Guid->Data2 = (USHORT)Value;

    if (!ObioParseGuidField(String + 15, 4, &Value))
    {
        return FALSE;
    }
    Guid->Data3 = (USHORT)Value;

    if (!ObioParseGuidField(String + 20, 2, &Value))
    {
        return FALSE;
    }
    Guid->Data4[0] = (UCHAR)Value;

    if (!ObioParseGuidField(String + 22, 2, &Value))
    {
        return FALSE;
    }
    Guid->Data4[1] = (UCHAR)Value;

    for (i = 0; i < 6; i++)
    {
        if (!ObioParseGuidField(String + 25 + (i * 2), 2, &Value))
        {
            return FALSE;
        }
        Guid->Data4[i + 2] = (UCHAR)Value;
    }

    return TRUE;
}

VEASTATUS
VEAPI
ObioSetDeviceInterfaceState(
    IN PANSI_STRING SymbolicLinkName,
    IN BOOLEAN Enable
)
{
    static const CHAR ClassPathPrefix[] =
        "/REGISTRY/MACHINE/SYSTEM/CurrentControlSet/Control/DeviceClasses/";
    static const CHAR DeviceKeyPrefix[] = "##?#";
    static const CHAR ControlKeyName[] = "Control";
    static const CHAR LinkedValueName[] = "Linked";
    ANSI_STRING ClassPath;
    ANSI_STRING GuidString;
    ANSI_STRING DeviceKeyName;
    ANSI_STRING ReferenceKeyName;
    ANSI_STRING DeviceInstance;
    ANSI_STRING ValueName;
    OBJECT_ATTRIBUTES Attributes;
    GUID InterfaceClassGuid;
    PCHAR LinkBody;
    PCHAR LinkName = NULL;
    PCHAR InstanceBuffer = NULL;
    PKEY_FILE_NODE DeviceInstanceNode = NULL;
    PVOID DeviceInstanceData = NULL;
    PDEVICE_OBJECT PhysicalDeviceObject = NULL;
    HANDLE ClassHandle = NULL;
    HANDLE DeviceHandle = NULL;
    HANDLE ReferenceHandle = NULL;
    HANDLE ControlHandle = NULL;
    ULONG BodyLength;
    ULONG GuidMarkerOffset = (ULONG)-1;
    ULONG GuidEndOffset;
    ULONG ReferenceOffset;
    ULONG PrefixLength;
    ULONG DeviceKeyLength;
    ULONG ReferenceLength;
    ULONG Disposition;
    ULONG LinkedValue;
    ULONG i;
    VEASTATUS Status;
    VEASTATUS LinkStatus;
    BOOLEAN LinkChanged = FALSE;
    BOOLEAN LinkRemovedForReplace = FALSE;

    RtlZeroMemory(&ClassPath, sizeof(ClassPath));
    RtlZeroMemory(&GuidString, sizeof(GuidString));
    RtlZeroMemory(&DeviceKeyName, sizeof(DeviceKeyName));
    RtlZeroMemory(&ReferenceKeyName, sizeof(ReferenceKeyName));
    RtlZeroMemory(&DeviceInstance, sizeof(DeviceInstance));

    if (!SymbolicLinkName || !SymbolicLinkName->Buffer ||
        SymbolicLinkName->Length < 2 ||
        SymbolicLinkName->Buffer[0] != '/')
    {
        return STATUS_INVALID_PARAMETER;
    }

    LinkBody = SymbolicLinkName->Buffer + 1;
    BodyLength = SymbolicLinkName->Length - 1;
    for (i = 0; i < BodyLength; i++)
    {
        if (LinkBody[i] == '#' && i + 39 <= BodyLength &&
            LinkBody[i + 1] == '{' &&
            (i + 39 == BodyLength || LinkBody[i + 39] == '/'))
        {
            GuidMarkerOffset = i;
            break;
        }
    }

    if (GuidMarkerOffset == (ULONG)-1 ||
        GuidMarkerOffset == 0 ||
        !ObioParseGuidString(LinkBody + GuidMarkerOffset + 1,
                             &InterfaceClassGuid))
    {
        return STATUS_INVALID_PARAMETER;
    }

    GuidEndOffset = GuidMarkerOffset + 39;
    ReferenceOffset = GuidEndOffset;
    if (ReferenceOffset < BodyLength)
    {
        if (LinkBody[ReferenceOffset] != '/' ||
            ReferenceOffset + 1 == BodyLength)
        {
            return STATUS_INVALID_PARAMETER;
        }
        ReferenceOffset++;
        for (i = ReferenceOffset; i < BodyLength; i++)
        {
            if (LinkBody[i] == '/' || LinkBody[i] == '\\')
            {
                return STATUS_INVALID_PARAMETER;
            }
        }
    }

    if (BodyLength > 257 ||
        GuidEndOffset > 0xFFFE - (sizeof(DeviceKeyPrefix) - 1))
    {
        return STATUS_NAME_TOO_LONG;
    }

    Status = RtlStringFromGUID(&InterfaceClassGuid, &GuidString);
    if (!VEA_SUCCESS(Status))
    {
        goto Cleanup;
    }

    PrefixLength = (ULONG)RtlRawStringLength((PCHAR)ClassPathPrefix);
    ClassPath.Length = (USHORT)(PrefixLength + GuidString.Length);
    ClassPath.MaximumLength = (USHORT)(ClassPath.Length + 1);
    ClassPath.Buffer = UlAllocatePoolWithTag(PagedPool,
                                             ClassPath.MaximumLength,
                                             TAG_RTL_STRING);
    if (!ClassPath.Buffer)
    {
        Status = STATUS_INSUFFICIENT_MEMORY;
        goto Cleanup;
    }
    RtlCopyMemory(ClassPath.Buffer, (PVOID)ClassPathPrefix, PrefixLength);
    RtlCopyMemory(ClassPath.Buffer + PrefixLength,
                  GuidString.Buffer,
                  GuidString.Length);
    ClassPath.Buffer[ClassPath.Length] = '\0';

    Status = ObioOpenVeaKey(NULL,
                            &ClassHandle,
                            &ClassPath,
                            KEY_CREATE_SUB_KEY | KEY_ENUMERATE_SUB_KEYS);
    if (!VEA_SUCCESS(Status))
    {
        goto Cleanup;
    }

    DeviceKeyLength = (sizeof(DeviceKeyPrefix) - 1) + GuidEndOffset;
    DeviceKeyName.Length = (USHORT)DeviceKeyLength;
    DeviceKeyName.MaximumLength = (USHORT)(DeviceKeyLength + 1);
    DeviceKeyName.Buffer = UlAllocatePoolWithTag(PagedPool,
                                                 DeviceKeyName.MaximumLength,
                                                 TAG_RTL_STRING);
    if (!DeviceKeyName.Buffer)
    {
        Status = STATUS_INSUFFICIENT_MEMORY;
        goto Cleanup;
    }
    RtlCopyMemory(DeviceKeyName.Buffer,
                  (PVOID)DeviceKeyPrefix,
                  sizeof(DeviceKeyPrefix) - 1);
    RtlCopyMemory(DeviceKeyName.Buffer + sizeof(DeviceKeyPrefix) - 1,
                  LinkBody,
                  GuidEndOffset);
    DeviceKeyName.Buffer[DeviceKeyLength] = '\0';

    Status = ObioOpenVeaKey(ClassHandle,
                            &DeviceHandle,
                            &DeviceKeyName,
                            KEY_CREATE_SUB_KEY | KEY_ENUMERATE_SUB_KEYS);
    if (!VEA_SUCCESS(Status))
    {
        goto Cleanup;
    }

    {
        PVK_KEY_OBJECT DeviceKeyObject = NULL;

        Status = ObReferenceObjectByHandle(DeviceHandle,
                                           KEY_QUERY_VALUE,
                                           VkKeyObjectType,
                                           KernelMode,
                                           (PVOID *)&DeviceKeyObject);
        if (!VEA_SUCCESS(Status))
        {
            goto Cleanup;
        }

        Status = ObioGetKeyValueData(DeviceKeyObject->HiveBase,
                                     DeviceKeyObject->KeyNode,
                                     "DeviceInstance",
                                     &DeviceInstanceNode,
                                     &DeviceInstanceData);
        ObDereferenceObject(DeviceKeyObject);
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("Device interface key has no valid DeviceInstance value: 0x%08lx\n",
                   Status);
            goto Cleanup;
        }
    }

    if (DeviceInstanceNode->Type != FILE_KEY_STRING ||
        DeviceInstanceNode->DataLength == 0 ||
        DeviceInstanceNode->DataLength == (ULONG)-1 ||
        DeviceInstanceNode->DataLength > 0xFFFE)
    {
        Status = STATUS_UNSUCCESSFUL;
        goto Cleanup;
    }

    InstanceBuffer = UlAllocatePoolWithTag(PagedPool,
                                           DeviceInstanceNode->DataLength + 1,
                                           TAG_RTL_STRING);
    if (!InstanceBuffer)
    {
        Status = STATUS_INSUFFICIENT_MEMORY;
        goto Cleanup;
    }
    RtlCopyMemory(InstanceBuffer,
                  DeviceInstanceData,
                  DeviceInstanceNode->DataLength);
    InstanceBuffer[DeviceInstanceNode->DataLength] = '\0';
    DeviceInstance.Buffer = InstanceBuffer;
    DeviceInstance.Length = (USHORT)DeviceInstanceNode->DataLength;
    DeviceInstance.MaximumLength = (USHORT)(DeviceInstanceNode->DataLength + 1);

    PhysicalDeviceObject =
        ObioGetDeviceObjectFromDeviceInstance(&DeviceInstance);
    if (!PhysicalDeviceObject)
    {
        DPRINT("Unable to find PDO for device instance %s\n",
               DeviceInstance.Buffer);
        Status = STATUS_OBJECT_NAME_NOT_FOUND;
        goto Cleanup;
    }

    ReferenceLength = 1;
    if (ReferenceOffset < BodyLength)
    {
        ReferenceLength += BodyLength - ReferenceOffset;
    }
    ReferenceKeyName.Length = (USHORT)ReferenceLength;
    ReferenceKeyName.MaximumLength = (USHORT)(ReferenceLength + 1);
    ReferenceKeyName.Buffer = UlAllocatePoolWithTag(PagedPool,
                                                    ReferenceKeyName.MaximumLength,
                                                    TAG_RTL_STRING);
    if (!ReferenceKeyName.Buffer)
    {
        Status = STATUS_INSUFFICIENT_MEMORY;
        goto Cleanup;
    }
    ReferenceKeyName.Buffer[0] = '#';
    if (ReferenceLength > 1)
    {
        RtlCopyMemory(ReferenceKeyName.Buffer + 1,
                      LinkBody + ReferenceOffset,
                      ReferenceLength - 1);
    }
    ReferenceKeyName.Buffer[ReferenceLength] = '\0';

    Status = ObioOpenVeaKey(DeviceHandle,
                            &ReferenceHandle,
                            &ReferenceKeyName,
                            KEY_CREATE_SUB_KEY | KEY_QUERY_VALUE |
                                KEY_SET_VALUE);
    if (!VEA_SUCCESS(Status))
    {
        goto Cleanup;
    }

    LinkName = UlAllocatePoolWithTag(PagedPool,
                                     GuidEndOffset + 1,
                                     TAG_RTL_STRING);
    if (!LinkName)
    {
        Status = STATUS_INSUFFICIENT_MEMORY;
        goto Cleanup;
    }
    RtlCopyMemory(LinkName, LinkBody, GuidEndOffset);
    LinkName[GuidEndOffset] = '\0';

    if (Enable)
    {
        LinkStatus = ObCreateSymbolicLinkToObject(ObpRootDirectoryObject,
                                                  LinkName,
                                                  PhysicalDeviceObject);
        if (LinkStatus == STATUS_OBJECT_NAME_COLLISION)
        {
            LinkStatus = ObDeleteSymbolicLink(ObpRootDirectoryObject,
                                              LinkName);
            if (!VEA_SUCCESS(LinkStatus))
            {
                Status = LinkStatus;
                goto Cleanup;
            }
            LinkRemovedForReplace = TRUE;
            LinkStatus = ObCreateSymbolicLinkToObject(ObpRootDirectoryObject,
                                                      LinkName,
                                                      PhysicalDeviceObject);
        }
        if (!VEA_SUCCESS(LinkStatus))
        {
            Status = LinkStatus;
            goto RollbackLink;
        }
        LinkChanged = TRUE;
    }
    else
    {
        LinkStatus = ObDeleteSymbolicLink(ObpRootDirectoryObject, LinkName);
        if (LinkStatus != STATUS_OBJECT_NAME_NOT_FOUND &&
            !VEA_SUCCESS(LinkStatus))
        {
            Status = LinkStatus;
            goto Cleanup;
        }
        LinkChanged = VEA_SUCCESS(LinkStatus);
    }

    RtlInitAnsiString(&ValueName, (PCHAR)ControlKeyName);
    InitializeObjectAttributes(&Attributes,
                               &ValueName,
                               0,
                               ReferenceHandle,
                               NULL);
    Status = VeaCreateKey(&ControlHandle,
                          KEY_ALL_ACCESS,
                          &Attributes,
                          0,
                          NULL,
                          REG_OPTION_VOLATILE,
                          &Disposition);
    if (!VEA_SUCCESS(Status))
    {
        goto RollbackLink;
    }

    RtlInitAnsiString(&ValueName, (PCHAR)LinkedValueName);
    LinkedValue = Enable ? 1 : 0;
    Status = VeaSetValueKey(ControlHandle,
                            &ValueName,
                            0,
                            FILE_KEY_DWORD,
                            &LinkedValue,
                            sizeof(LinkedValue));
    if (!VEA_SUCCESS(Status))
    {
        goto RollbackLink;
    }

    {
        const GUID *EventGuid = Enable
            ? &GUID_DEVICE_INTERFACE_ARRIVAL
            : &GUID_DEVICE_INTERFACE_REMOVAL;
        ObioNotifyDeviceInterfaceChange((PGUID)EventGuid,
                                        &InterfaceClassGuid,
                                        SymbolicLinkName);
    }

    Status = STATUS_SUCCESS;
    goto Cleanup;

RollbackLink:
    if (LinkChanged)
    {
        if (Enable)
        {
            LinkStatus = ObDeleteSymbolicLink(ObpRootDirectoryObject,
                                              LinkName);
            if (VEA_SUCCESS(LinkStatus) && LinkRemovedForReplace)
            {
                LinkStatus = ObCreateSymbolicLinkToObject(
                    ObpRootDirectoryObject,
                    LinkName,
                    PhysicalDeviceObject);
            }
        }
        else
        {
            LinkStatus = ObCreateSymbolicLinkToObject(
                ObpRootDirectoryObject,
                LinkName,
                PhysicalDeviceObject);
        }
        if (!VEA_SUCCESS(LinkStatus))
        {
            DPRINT("Failed to roll back device interface link: 0x%08lx\n",
                   LinkStatus);
        }
    }
    else if (LinkRemovedForReplace)
    {
        LinkStatus = ObCreateSymbolicLinkToObject(ObpRootDirectoryObject,
                                                  LinkName,
                                                  PhysicalDeviceObject);
        if (!VEA_SUCCESS(LinkStatus))
        {
            DPRINT("Failed to restore replaced device interface link: 0x%08lx\n",
                   LinkStatus);
        }
    }

Cleanup:
    if (ControlHandle)
    {
        VeaClose(ControlHandle);
    }
    if (ReferenceHandle)
    {
        VeaClose(ReferenceHandle);
    }
    if (DeviceHandle)
    {
        VeaClose(DeviceHandle);
    }
    if (ClassHandle)
    {
        VeaClose(ClassHandle);
    }
    if (PhysicalDeviceObject)
    {
        ObDereferenceObject(PhysicalDeviceObject);
    }
    if (ClassPath.Buffer)
    {
        UlFreePoolWithTag(ClassPath.Buffer, TAG_RTL_STRING);
    }
    if (GuidString.Buffer)
    {
        UlFreePoolWithTag(GuidString.Buffer, TAG_USTR);
    }
    if (DeviceKeyName.Buffer)
    {
        UlFreePoolWithTag(DeviceKeyName.Buffer, TAG_RTL_STRING);
    }
    if (ReferenceKeyName.Buffer)
    {
        UlFreePoolWithTag(ReferenceKeyName.Buffer, TAG_RTL_STRING);
    }
    if (LinkName)
    {
        UlFreePoolWithTag(LinkName, TAG_RTL_STRING);
    }
    if (InstanceBuffer)
    {
        UlFreePoolWithTag(InstanceBuffer, TAG_RTL_STRING);
    }
    return Status;
}

VOID
VEAPI
ObioInvalidateDeviceRelations(
    IN PDEVICE_OBJECT DeviceObject,
    IN DEVICE_RELATION_TYPE RelationType
)
{
    if (!ObioIsValidPhysicalDeviceObject(DeviceObject))
    {
        KsBugCheckEx(PNP_DETECTED_FATAL_ERROR, 0x2, (ULONG_PTR)DeviceObject, RelationType, 0);
    }

    switch (RelationType)
    {
        case BusRelations:
            ObioQueueDeviceAction(
                DeviceObject,
                PiActionEnumDeviceTree,
                NULL,
                NULL
            );
        default:
            break;
    }
}

/* Note on this:

   Actual BDI Format Symlink path
   if GPT:
   BDI_<Type>%<PartitionGUID>
   
   if MBR:
   BDI_<Type>%<DiskSignature>%<PartitionNumber>
   
   Note: on MBR, Disk signature may same so we should revised BDI */
VEASTATUS
VEAPI
ObioCreateBdiSymlink(IN PBLOCK_BOOT_2 BlockBoot2)
{
    VEASTATUS Status;
    PDEVICE_OBJECT DiskObject = NULL;
    PDEVICE_OBJECT TopDeviceObject;
    PDEVICE_OBJECT PartitionObject;
    PVOID DirectoryObject = NULL;
    ANSI_STRING BdiPath = RTL_CONSTANT_ANSI_STRING("/BDI");
    OBJECT_ATTRIBUTES Attributes;
    PBOOT_DEVICE_IDENTIFIER DeviceIdentifier;
    ULONG DiskSignature;
    ULONG PartitionNumber;
    ULONG DiskIndex = 0;
    PCHAR SymbolicLinkList = NULL;
    PCHAR Link;

    if (BlockBoot2 == NULL)
    {
        DPRINT("BDI: loader block is NULL\n");
        return STATUS_INVALID_PARAMETER;
    }

    DPRINT("BDI: starting boot-device lookup (loader block %p)\n", BlockBoot2);

    InitializeObjectAttributes(
        &Attributes,
        &BdiPath,
        OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
        NULL,
        NULL
    );
    Status = ObReferenceObjectByName(
        &Attributes,
        ObpDirectoryObjectType,
        &DirectoryObject
    );
    if (Status == STATUS_OBJECT_NAME_NOT_FOUND)
    {
        HANDLE DirHandle;
        
        Status = VeaCreateDirectoryNamespace(&DirHandle, 0x10000, &Attributes);
        if (Status == STATUS_OBJECT_NAME_COLLISION)
        {
            Status = STATUS_SUCCESS;
        }
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("BDI: creating /BDI namespace failed, status=0x%x\n", Status);
            return Status;
        }

        VeaClose(DirHandle);

        Status = ObReferenceObjectByName(
            &Attributes,
            ObpDirectoryObjectType,
            &DirectoryObject
        );
    }
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("BDI: opening /BDI directory failed, status=0x%x\n", Status);
        return Status;
    }

    /* Get Device Identifier */
    DeviceIdentifier = &BlockBoot2->BootPartitionSignature;
    DPRINT("BDI: boot identifier type=%u\n", DeviceIdentifier->Type);

    Status = ObioGetDeviceInterfaces(
        &VEA_GUID_DEVINTERFACE_DISK,
        NULL,
        0,
        &SymbolicLinkList
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("BDI: enumerating disk interfaces failed, status=0x%x\n", Status);
        goto Cleanup;
    }

    Status = STATUS_OBJECT_NAME_NOT_FOUND;
    for (Link = SymbolicLinkList; *Link != '\0';)
    {
        ULONG LinkLength = RtlRawStringLength(Link);
        ANSI_STRING DiskString;
        OBJECT_ATTRIBUTES DiskAttributes;
        DISK_STYLE DiskStyle = DiskStyleMbr;

        Link += LinkLength + 1;
        RtlInitAnsiString(&DiskString, Link - LinkLength - 1);
        DPRINT("BDI: checking disk[%u] interface %s\n", DiskIndex, DiskString.Buffer);
        DiskIndex++;

        InitializeObjectAttributes(
            &DiskAttributes,
            &DiskString,
            OBJ_CASE_INSENSITIVE,
            NULL,
            NULL
        );
        Status = ObReferenceObjectByName(
            &DiskAttributes,
            ObpDeviceObjectType,
            (PVOID*)&DiskObject
        );
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("BDI: resolving interface %s failed, status=0x%x\n",
                   DiskString.Buffer, Status);
            continue;
        }

        TopDeviceObject = ObioGetTopDeviceObject(DiskObject);
        if (TopDeviceObject == NULL)
        {
            DPRINT("BDI: interface %s resolved to %p but has no stack top\n",
                   DiskString.Buffer, DiskObject);
            ObDereferenceObject(DiskObject);
            DiskObject = NULL;
            continue;
        }
        ObReferenceObject(TopDeviceObject);
        DPRINT("BDI: disk object=%p, top stack device=%p\n",
               DiskObject, TopDeviceObject);

        {
            POIP Oip;
            KEVENT Event;
            OIP_INFORMATION IoInformation;
            BOOLEAN RequestPending;

            RtlZeroMemory(&IoInformation, sizeof(IoInformation));
            KsInitializeEvent(&Event, NotificationEvent, FALSE);
            Oip = ObioBuildDeviceIoControlRequest(
                IOCTL_DISK_GET_DISK_STYLE,
                TopDeviceObject,
                NULL,
                0,
                &DiskStyle,
                sizeof(DiskStyle),
                FALSE,
                &Event,
                &IoInformation
            );

            if (Oip == NULL)
            {
                Status = STATUS_INSUFFICIENT_MEMORY;
                DPRINT("BDI: allocating disk-style IOCTL request failed for %p, status=0x%x\n",
                       TopDeviceObject, Status);
                goto NextDisk;
            }

            Status = ObioGoInterruptObject(TopDeviceObject, Oip);
            RequestPending = (Status == STATUS_PENDING);
            DPRINT("BDI: disk-style IOCTL dispatch returned 0x%x (pending=%u)\n",
                   Status, RequestPending);
            if (RequestPending)
            {
                KsWaitForSingleObject(&Event);
                Status = IoInformation.Status;
            }
            else
            {
                IoInformation = Oip->StatusInformation;
            }

            if (!VEA_SUCCESS(Status) ||
                IoInformation.Information < sizeof(DiskStyle))
            {
                DPRINT("BDI: disk-style IOCTL failed: status=0x%x information=%u required=%u\n",
                       Status, (ULONG)IoInformation.Information,
                       (ULONG)sizeof(DiskStyle));
                goto NextDisk;
            }
            DPRINT("BDI: disk-style IOCTL succeeded, style=%u\n", DiskStyle);
        }

        if (DiskStyle == DiskStyleMbr)
        {
            PCHAR SectorBuffer;
            POIP ReadOip;
            LARGE_INTEGER StartingOffset;
            KEVENT ReadEvent;
            OIP_INFORMATION IoStatus;
            BOOLEAN IsBdiMatch;
            BOOLEAN RequestPending;

            SectorBuffer = (PCHAR)UlAllocatePoolZero(NonPagedPool, 512, TAG_OBIO);
            if (SectorBuffer == NULL)
            {
                Status = STATUS_INSUFFICIENT_MEMORY;
                DPRINT("BDI: allocating MBR sector buffer failed for disk[%u], status=0x%x\n",
                       DiskIndex - 1, Status);
                goto NextDisk;
            }

            RtlZeroMemory(&IoStatus, sizeof(IoStatus));
            KsInitializeEvent(&ReadEvent, NotificationEvent, FALSE);
            StartingOffset.QuadPart = 0;
            ReadOip = ObioBuildSynchronusFsdRequest(
                OIP_MJ_READ,
                TopDeviceObject,
                SectorBuffer,
                512,
                &StartingOffset,
                &ReadEvent,
                &IoStatus
            );
            if (ReadOip == NULL)
            {
                UlFreePoolWithTag(SectorBuffer, TAG_OBIO);
                Status = STATUS_INSUFFICIENT_MEMORY;
                DPRINT("BDI: building MBR read request failed for device %p, status=0x%x\n",
                       TopDeviceObject, Status);
                goto NextDisk;
            }

            Status = ObioGoInterruptObject(TopDeviceObject, ReadOip);
            RequestPending = (Status == STATUS_PENDING);
            DPRINT("BDI: MBR read dispatch returned 0x%x (pending=%u)\n",
                   Status, RequestPending);
            if (RequestPending)
            {
                KsWaitForSingleObject(&ReadEvent);
                Status = IoStatus.Status;
            }
            else
            {
                IoStatus = ReadOip->StatusInformation;
            }

            if (!VEA_SUCCESS(Status) || IoStatus.Information < 512)
            {
                DPRINT("BDI: MBR read failed: status=0x%x information=%u required=512\n",
                       Status, (ULONG)IoStatus.Information);
                UlFreePoolWithTag(SectorBuffer, TAG_OBIO);
                goto NextDisk;
            }

            PartitionNumber = DeviceIdentifier->u.Mbr.PartitionNumber;
            DiskSignature = DeviceIdentifier->u.Mbr.DiskSignature;
            IsBdiMatch = ObioAnalyzeBdiMatchMbr(
                SectorBuffer,
                PartitionNumber,
                DiskSignature
            );
            UlFreePoolWithTag(SectorBuffer, TAG_OBIO);
            if (!IsBdiMatch)
            {
                Status = STATUS_OBJECT_NAME_NOT_FOUND;
                DPRINT("BDI: disk[%u] MBR does not match boot identifier "
                       "(signature=0x%08x partition=%u)\n",
                       DiskIndex - 1, DiskSignature, PartitionNumber);
                goto NextDisk;
            }
            DPRINT("BDI: disk[%u] MBR matches boot identifier "
                   "(signature=0x%08x partition=%u)\n",
                   DiskIndex - 1, DiskSignature, PartitionNumber);

            {
                POIP DiskNumOip;
                KEVENT Event;
                OIP_INFORMATION DnInformation;
                ULONG HarddiskNumber;
                CHAR BufferHarddisk[50];
                ANSI_STRING PartitionString;
                OBJECT_ATTRIBUTES ObjAttr;
                BOOLEAN RequestPending;

                RtlZeroMemory(&DnInformation, sizeof(DnInformation));
                KsInitializeEvent(&Event, NotificationEvent, FALSE);
                DiskNumOip = ObioBuildDeviceIoControlRequest(
                    IOCTL_DISK_GET_HARDDISK_NUMBER,
                    TopDeviceObject,
                    NULL,
                    0,
                    &HarddiskNumber,
                    sizeof(ULONG),
                    FALSE,
                    &Event,
                    &DnInformation
                );

                if (DiskNumOip == NULL)
                {
                    Status = STATUS_INSUFFICIENT_MEMORY;
                    DPRINT("BDI: allocating harddisk-number IOCTL request failed "
                           "for device %p, status=0x%x\n",
                           TopDeviceObject, Status);
                    goto NextDisk;
                }

                Status = ObioGoInterruptObject(TopDeviceObject, DiskNumOip);
                RequestPending = (Status == STATUS_PENDING);
                DPRINT("BDI: harddisk-number IOCTL dispatch returned 0x%x "
                       "(pending=%u)\n",
                       Status, RequestPending);
                if (RequestPending)
                {
                    KsWaitForSingleObject(&Event);
                    Status = DnInformation.Status;
                }
                else
                {
                    DnInformation = DiskNumOip->StatusInformation;
                }

                if (!VEA_SUCCESS(Status) ||
                    DnInformation.Information < sizeof(HarddiskNumber))
                {
                    DPRINT("BDI: harddisk-number IOCTL failed: status=0x%x "
                           "information=%u required=%u\n",
                           Status, (ULONG)DnInformation.Information,
                           (ULONG)sizeof(HarddiskNumber));
                    goto NextDisk;
                }
                DPRINT("BDI: matched boot partition is Harddisk%u/Partition%u\n",
                       HarddiskNumber, PartitionNumber);

                Status = RtlStringCbPrintfA(
                    BufferHarddisk,
                    50,
                    "/Device/Harddisk%u/Partition%u",
                    HarddiskNumber,
                    PartitionNumber
                );
                if (!VEA_SUCCESS(Status))
                {
                    DPRINT("BDI: formatting partition device path failed, status=0x%x\n",
                           Status);
                    goto NextDisk;
                }
                DPRINT("BDI: resolving partition device %s\n", BufferHarddisk);

                RtlInitAnsiString(&PartitionString, BufferHarddisk);

                InitializeObjectAttributes(
                    &ObjAttr,
                    &PartitionString,
                    OBJ_CASE_INSENSITIVE,
                    NULL,
                    NULL
                );
                PartitionObject = NULL;
                Status = ObReferenceObjectByName(
                    &ObjAttr,
                    ObpDeviceObjectType,
                    (PVOID*)&PartitionObject
                );
                if (!VEA_SUCCESS(Status))
                {
                    DPRINT("BDI: opening partition device %s failed, status=0x%x\n",
                           BufferHarddisk, Status);
                    goto NextDisk;
                }

                CHAR BdiBufferSymlink[100];
                Status = RtlStringCbPrintfA(
                    BdiBufferSymlink,
                    sizeof(BdiBufferSymlink),
                    "BDI_%u%%%08X%%%u",
                    DeviceIdentifier->Type, DiskSignature, PartitionNumber
                );
                if (!VEA_SUCCESS(Status))
                {
                    DPRINT("BDI: formatting symbolic-link name failed, status=0x%x\n",
                           Status);
                    ObDereferenceObject(PartitionObject);
                    goto NextDisk;
                }

                DPRINT("BDI: creating /BDI/%s -> %s\n",
                       BdiBufferSymlink, BufferHarddisk);
                Status = ObCreateSymbolicLinkToObject(
                    DirectoryObject,
                    BdiBufferSymlink,
                    PartitionObject
                );
                ObDereferenceObject(PartitionObject);
                if (VEA_SUCCESS(Status))
                {
                    ObDereferenceObject(TopDeviceObject);
                    ObDereferenceObject(DiskObject);
                    UlFreePoolWithTag(SymbolicLinkList, TAG_RTL_STRING);
                    ObDereferenceObject(DirectoryObject);
                    DPRINT("BDI: boot-device symbolic link created successfully\n");
                    return STATUS_SUCCESS;
                }
                DPRINT("BDI: creating symbolic link /BDI/%s failed, status=0x%x\n",
                       BdiBufferSymlink, Status);
            }
        }
        else if (DiskStyle == DiskStyleGpt)
        {
            /* TODO: Do GPT */
            DPRINT("BDI: boot device candidate %p uses GPT; GPT matching is not implemented\n",
                   TopDeviceObject);
            UNIMPLEMENTED_DBGBREAK;
        }
        else
        {
            DPRINT("BDI: disk[%u] returned unknown disk style %u\n",
                   DiskIndex - 1, DiskStyle);
        }

NextDisk:
        if (TopDeviceObject != NULL)
        {
            ObDereferenceObject(TopDeviceObject);
            TopDeviceObject = NULL;
        }
        if (DiskObject != NULL)
        {
            ObDereferenceObject(DiskObject);
            DiskObject = NULL;
        }
    }

Cleanup:
    if (SymbolicLinkList != NULL)
    {
        UlFreePoolWithTag(SymbolicLinkList, TAG_RTL_STRING);
    }
    if (DirectoryObject != NULL)
    {
        ObDereferenceObject(DirectoryObject);
    }
    DPRINT("BDI: boot-device lookup failed after checking %u disk interface(s), status=0x%x\n",
           DiskIndex, Status);
    return Status;
}

/* EOF */