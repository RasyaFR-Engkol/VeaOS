#include <veakrnl.h>

LONG
VEAPI
ObAllocateObject(
    POBJECT_TYPE ObjectType,
    POBJECT_ATTRIBUTES ObjectAttributes,
    ULONG ObjectSize,
    PVOID *ReturnedObject // Output: Pointer ke badan objek
)
{
    if (!ObjectType || !ReturnedObject) {
        return -1;
    }

    ULONG NameInfoSize = 0;
    BOOLEAN HasName = FALSE;

    if(ObjectAttributes && ObjectAttributes->ObjectName && ObjectAttributes->ObjectName->Length != 0)
    {
        NameInfoSize = sizeof(OBJECT_HEADER_NAME_INFO);
        HasName = TRUE;
    }

    ULONG BodySize = (ObjectSize > 0) ? ObjectSize : ObjectType->ObjectSize;
    ULONG TotalSize = NameInfoSize + sizeof(OBJECT_HEADER) + BodySize;

    PVOID RawMemory = UlAllocatePoolWithTag(NonPagedPool, TotalSize, ObjectType->PoolTag);
    if (!RawMemory) {
        return -1; // OOM!
    }

    RtlZeroMemory(RawMemory, TotalSize);

    POBJECT_HEADER Header = (POBJECT_HEADER)((PUCHAR)RawMemory + NameInfoSize);

    if(ObjectAttributes && ObjectAttributes->SecurityDescriptor)
    {
        Header->SecurityDescriptor = ObjectAttributes->SecurityDescriptor;
    }
    else  
    {
        Header->SecurityDescriptor = SfCreateNormalSecurityDescriptor();
    }

    Header->ReferenceCount = 1;        // Baru lahir, yang megang 1 (Sistem)
    Header->Type = ObjectType;
    Header->Flags = 0;

    if(HasName)
    {
        Header->Flags |= OB_FLAG_HAS_NAME_INFO;
    }
    else
    {
        Header->Flags &= ~OB_FLAG_HAS_NAME_INFO;
    }

    Header->StackDepth = 1;            // Default kedalaman adalah 1 (dirinya sendiri)
    Header->LowerAttachedObject = NULL; 
    Header->UpperAttachedObject = NULL;

    PVOID ObjectBody = (PVOID)(Header + 1);

    if (ObjectType->CreateRoutine) {
        OB_CALLOUT_RECORD Tracker;
        Tracker.Object = ObjectBody;
        Tracker.ObjectType = ObjectType;
        Tracker.CalloutType = ObCalloutCreate;
        Tracker.RoutineAddress = (PVOID)ObjectType->CreateRoutine;

        ObpPushCalloutTracker(NULL, &Tracker);

        LONG Status = ObjectType->CreateRoutine(ObjectBody);

        ObpPopCalloutTracker(NULL, &Tracker);

        if (Status != 0) {
            UlFreePoolWithTag(RawMemory, ObjectType->PoolTag);
            return Status;
        }
    }

    ObjectType->TotalObjectCount++;

    *ReturnedObject = ObjectBody;

    return 0;
}

LONG
VEAPI
ObCreateObjectType(
    PCHAR TypeName,
    POBJECT_TYPE_INITIALIZER Initializer,
    POBJECT_TYPE *ReturnedObjectType // Output pointer
)
{
    if (!TypeName || !Initializer || !ReturnedObjectType) {
        return -1;
    }

    if (Initializer->Length != sizeof(OBJECT_TYPE_INITIALIZER)) {
        return -1;
    }

    ANSI_STRING Name;
    RtlInitAnsiString(&Name, TypeName);

    OBJECT_ATTRIBUTES Attr;
    InitializeObjectAttributes(&Attr, &Name, 0, NULL, NULL);

    POBJECT_TYPE NewType = NULL;
    LONG Status;

    Status = ObAllocateObject(
        ObpTypeObjectType, 
        &Attr, 
        sizeof(OBJECT_TYPE), // ObjectSize untuk struct OBJECT_TYPE itu sendiri
        (PVOID*)&NewType
    );
    
    if (Status != 0) {
        return Status;
    }

    RtlCopyMemory(
        &NewType->GenericMapping,
        &Initializer->GenericMapping,
        sizeof(GENERIC_MAPPING)
    );

    NewType->TypeId = ObpNextTypeId++;
    NewType->TotalObjectCount = 0; // Belum ada instansinya

    ULONG i = 0;
    while (TypeName[i] != '\0' && i < sizeof(NewType->TypeName) - 1) {
        NewType->TypeName[i] = TypeName[i];
        i++;
    }
    NewType->TypeName[i] = '\0';

    NewType->PoolTag = Initializer->PoolTag;
    NewType->ObjectSize = Initializer->ObjectSize;
    NewType->AllowAttachByDefault = Initializer->AllowAttachByDefault;
    NewType->ValidAccessMask = Initializer->ValidAccessMask;
    NewType->InterruptHandler = Initializer->InterruptHandler;
    NewType->CreateRoutine = Initializer->CreateRoutine;
    NewType->DeleteRoutine = Initializer->DeleteRoutine;
    NewType->ParseRoutine = Initializer->ParseRoutine;
    NewType->QueryNameMethod = Initializer->QueryNameRoutine;
    NewType->CloseRoutine = Initializer->CloseRoutine;

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(NewType);
    if(Header->Flags & OB_FLAG_HAS_NAME_INFO)
    {
        POBJECT_HEADER_NAME_INFO NameInfo = OBJECT_HEADER_TO_NAME_INFO(Header);
        NameInfo->Directory = ObpTypesDirectoryObject;
        NameInfo->Name.Length = Name.Length;
        NameInfo->Name.MaximumLength = Name.Length + 1;
        NameInfo->Name.Buffer = UlAllocatePoolWithTag(PagedPool, NameInfo->Name.MaximumLength, 'NmOb');
        RtlCopyRawString(NameInfo->Name.Buffer, TypeName);
    }

    InsertTailList(&ObpTypeObjectList, &NewType->TypeList);

    if (ObpTypesDirectoryObject != NULL) {
        ObpInsertDirectory(ObpTypesDirectoryObject, NewType);
    }

    *ReturnedObjectType = NewType;

    return 0;
}   

VEASTATUS
ObInitializeLookupContext(
    POB_LOOKUP_CONTEXT LookupContext,
    POBJECT_ATTRIBUTES ObjectAttributes,
    POBJECT_TYPE ExpectedType,
    ACCESS_MASK DesiredAccess,
    KPROCESSOR_MODE AccessMode
)
{
    if (!LookupContext || !ObjectAttributes || !ObjectAttributes->ObjectName) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(LookupContext, sizeof(*LookupContext));

    LookupContext->ExpectedType     = ExpectedType;
    LookupContext->DesiredAccess    = DesiredAccess; 
    LookupContext->AccessMode       = AccessMode;    
    LookupContext->ReparseCount     = 0;
    LookupContext->CaseInsensitive  = (ObjectAttributes->Attributes & OBJ_CASE_INSENSITIVE) != 0;
    LookupContext->RootDirectory    = ObjectAttributes->RootDirectory;
    LookupContext->Attributes       = ObjectAttributes->Attributes;
    LookupContext->InsertMode       = FALSE;

    // Set awal CurrentDirectory berdasarkan RootDirectory
    if (ObjectAttributes->RootDirectory) {
        LookupContext->CurrentDirectory = ObjectAttributes->RootDirectory;
    } else {
        LookupContext->CurrentDirectory = ObpRootDirectoryObject;
    }

    // Assign ANSI_STRING (Copy struct Header, bukan copy buffer string)
    LookupContext->RemainingName = *ObjectAttributes->ObjectName;

    return STATUS_SUCCESS;
}

BOOLEAN
VEAPI
ObpCheckUnsecureName(
    PANSI_STRING Name,
    KPROCESSOR_MODE AccessMode
)
{
    if (!Name || !Name->Buffer || Name->Length == 0) {
        return FALSE; // Nama kosong bukan unsecure, tapi invalid parameter
    }

    PCHAR Buffer = Name->Buffer;
    USHORT Length = Name->Length;

    CHAR LastChar = Buffer[Length - 1];
    if (LastChar == ' ' || LastChar == '.') {
        // Jika dari UserMode, trailing space/dot Sangat Dilarang!
        if (AccessMode == UserMode) {
            return TRUE; // TERDETEKSI UNSECURE!
        }
    }

    for (USHORT i = 0; i < Length; i++) {
        UCHAR c = (UCHAR)Buffer[i];

        // A. Cek Control Characters (0x01 - 0x1F dan 0x7F)
        if (c < 0x20 || c == 0x7F) {
            return TRUE; // TERDETEKSI UNSECURE!
        }

        // B. Cek Illegal/Wildcard Characters
        if (c == '*' || c == '<' || c == '>' || c == '"' || c == '|') {
            return TRUE; // TERDETEKSI UNSECURE!
        }

        if (c == '?') {
            if (AccessMode == UserMode) {
                return TRUE;
            }
        }

        // C. Cek Double Slash yang tidak sengaja terlewat (// atau \\)
        if ((c == '/' || c == '\\') && (i + 1 < Length)) {
            CHAR NextChar = Buffer[i + 1];
            if (NextChar == '/' || NextChar == '\\') {
                return TRUE; // TERDETEKSI UNSECURE!
            }
        }
    }

    // Cek apakah token nama persis "." atau ".."
    if (Length == 1 && Buffer[0] == '.') {
        return TRUE;
    }
    if (Length == 2 && Buffer[0] == '.' && Buffer[1] == '.') {
        return TRUE;
    }

    return FALSE; // Nama aman!
}

VOID
VEAPI
ObpPushCalloutTracker(
    PVOID CurrentThread, // Nanti ganti dengan PKTHREAD/PETHREAD
    POB_CALLOUT_RECORD Tracker
)
{
    if (!Tracker) return;

    // Jika sistem Thread VeaOS lu belum siap, lu bisa biarkan kosong dulu
    // atau simpan di global array untuk sementara. 
    // Format idealnya: InsertHeadList(&CurrentThread->CalloutListHead, &Tracker->Chain);
    
    // Untuk tujuan debug via console:
    // kdp_print("OB: [TRACE] Pushing Callout. Type: %d, Routine: %p\n", Tracker->CalloutType, Tracker->RoutineAddress);
}

VOID
VEAPI
ObpPopCalloutTracker(
    PVOID CurrentThread,
    POB_CALLOUT_RECORD Tracker
)
{
    if (!Tracker) return;

    // Format idealnya: RemoveEntryList(&Tracker->Chain);
    
    // kdp_print("OB: [TRACE] Popping Callout. Type: %d\n", Tracker->CalloutType);
}

PVOID
VEAPI
ObGetTopAttachedObject(IN PVOID Object)
{
    PVOID CurrentObject;
    PVOID NextObject;
    POBJECT_HEADER ObjectHeader;
    KIRQL OldIrql;

    if (!Object)
    {
        return NULL;
    }

    CurrentObject = Object;
    ObReferenceObject(CurrentObject);

    while (TRUE)
    {
        KsAcquireSpinLock(&ObioDeviceTreeLock, &OldIrql);

        ObjectHeader = OBJECT_TO_OBJECT_HEADER(CurrentObject);
        NextObject = ObjectHeader->UpperAttachedObject;

        if (NextObject != NULL)
        {
            ObReferenceObject(NextObject);
        }

        KsReleaseSpinLock(&ObioDeviceTreeLock, OldIrql);

        if (NextObject == NULL)
        {
            break;
        }

        // 5. Lepas reference objek lama, pindah ke objek baru
        ObDereferenceObject(CurrentObject);
        CurrentObject = NextObject;
    }

    // Return objek paling atas DENGAN 1 ekstra Reference Count
    // (Caller wajib memanggil ObDereferenceObject setelah selesai panggil OIP/IRP)
    return CurrentObject;
}

VEASTATUS
VEAPI
ObQueryNameString(
    IN PVOID Object,
    OUT PANSI_STRING ObjectNameInfo,
    IN ULONG Length,
    OUT PULONG ReturnLength
)
{
    POBJECT_HEADER ObjectHeader;
    POBJECT_HEADER_NAME_INFO NameInfo;
    POBJECT_DIRECTORY ParentDirectory;
    ULONG NameSize;
    ULONG RequiredLength;
    PCHAR ObjectName;

    if (!Object || !ObjectNameInfo || !ReturnLength)
    {
        return STATUS_INVALID_PARAMETER;
    }

    ObjectHeader = OBJECT_TO_OBJECT_HEADER(Object);
    NameInfo = NULL;
    if (ObjectHeader->Flags & OB_FLAG_HAS_NAME_INFO)
    {
        NameInfo = OBJECT_HEADER_TO_NAME_INFO(ObjectHeader);
    }

    if (ObjectHeader->Type && ObjectHeader->Type->QueryNameMethod)
    {
        BOOLEAN ObjectIsNamed = (NameInfo && NameInfo->Name.Length > 0);

        return ObjectHeader->Type->QueryNameMethod(
            Object,
            ObjectIsNamed,
            ObjectNameInfo,
            Length,
            ReturnLength,
            KernelMode
        );
    }

    if ((!NameInfo || !NameInfo->Name.Buffer) && Object != ObpRootDirectoryObject)
    {
        *ReturnLength = sizeof(ANSI_STRING);
        if (*ReturnLength > Length)
        {
            return STATUS_BUFFER_TOO_SMALL;
        }

        ObjectNameInfo->Length = 0;
        ObjectNameInfo->MaximumLength = 0;
        ObjectNameInfo->Buffer = NULL;
        return STATUS_SUCCESS;
    }

    if (Object == ObpRootDirectoryObject)
    {
        NameSize = sizeof(CHAR);
    }
    else
    {
        NameSize = NameInfo->Name.Length;
        ParentDirectory = NameInfo->Directory;

        while (ParentDirectory &&
               ParentDirectory != ObpRootDirectoryObject)
        {
            POBJECT_HEADER ParentHeader;
            POBJECT_HEADER_NAME_INFO ParentNameInfo;
            ULONG ComponentSize;

            ParentHeader = OBJECT_TO_OBJECT_HEADER(ParentDirectory);
            ParentNameInfo = NULL;
            if (ParentHeader->Flags & OB_FLAG_HAS_NAME_INFO)
            {
                ParentNameInfo = OBJECT_HEADER_TO_NAME_INFO(ParentHeader);
            }

            if (ParentNameInfo && ParentNameInfo->Name.Buffer)
            {
                ComponentSize = sizeof(CHAR) + ParentNameInfo->Name.Length;
                ParentDirectory = ParentNameInfo->Directory;
            }
            else
            {
                ComponentSize = sizeof(CHAR) + 3;
                ParentDirectory = NULL;
            }

            if (NameSize > (ULONG)-1 - ComponentSize)
            {
                return STATUS_NAME_TOO_LONG;
            }
            NameSize += ComponentSize;
        }

        if (NameSize > (ULONG)-1 - sizeof(CHAR))
        {
            return STATUS_NAME_TOO_LONG;
        }
        NameSize += sizeof(CHAR);
    }

    if (NameSize > 0xFFFE)
    {
        return STATUS_NAME_TOO_LONG;
    }

    RequiredLength = sizeof(ANSI_STRING) + NameSize + sizeof(CHAR);
    *ReturnLength = RequiredLength;
    if (RequiredLength > Length)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    ObjectName = (PCHAR)((ULONG_PTR)ObjectNameInfo + RequiredLength);
    *--ObjectName = '\0';

    if (Object == ObpRootDirectoryObject)
    {
        *--ObjectName = '\\';
    }
    else
    {
        ObjectName -= NameInfo->Name.Length;
        RtlCopyMemory(ObjectName,
                      NameInfo->Name.Buffer,
                      NameInfo->Name.Length);

        ParentDirectory = NameInfo->Directory;
        while (ParentDirectory &&
               ParentDirectory != ObpRootDirectoryObject)
        {
            POBJECT_HEADER ParentHeader;
            POBJECT_HEADER_NAME_INFO ParentNameInfo;

            *--ObjectName = '\\';
            ParentHeader = OBJECT_TO_OBJECT_HEADER(ParentDirectory);
            ParentNameInfo = NULL;
            if (ParentHeader->Flags & OB_FLAG_HAS_NAME_INFO)
            {
                ParentNameInfo = OBJECT_HEADER_TO_NAME_INFO(ParentHeader);
            }

            if (ParentNameInfo && ParentNameInfo->Name.Buffer)
            {
                ObjectName -= ParentNameInfo->Name.Length;
                RtlCopyMemory(ObjectName,
                              ParentNameInfo->Name.Buffer,
                              ParentNameInfo->Name.Length);
                ParentDirectory = ParentNameInfo->Directory;
            }
            else
            {
                ObjectName -= 3;
                RtlCopyMemory(ObjectName, "...", 3);
                break;
            }
        }

        *--ObjectName = '\\';
    }

    ObjectNameInfo->Length = (USHORT)NameSize;
    ObjectNameInfo->MaximumLength = (USHORT)(NameSize + sizeof(CHAR));
    ObjectNameInfo->Buffer = ObjectName;
    return STATUS_SUCCESS;
}