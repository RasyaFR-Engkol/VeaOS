#include <veakrnl.h>

LONG
VEAPI
ObAllocateObject(
    POBJECT_TYPE ObjectType,
    POBJECT_ATTRIBUTES ObjectAttributes,
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

    ULONG TotalSize = NameInfoSize + sizeof(OBJECT_HEADER) + ObjectType->ObjectSize;

    PVOID RawMemory = UlAllocatePoolWithTag(NonPagedPool, TotalSize, ObjectType->PoolTag);
    if (!RawMemory) {
        return -1; // OOM!
    }

    RtlZeroMemory(RawMemory, TotalSize);

    POBJECT_HEADER Header = (POBJECT_HEADER)((PUCHAR)RawMemory + NameInfoSize);

    Header->ReferenceCount = 1;        // Baru lahir, yang megang 1 (Sistem)
    Header->Type = ObjectType;
    Header->Flags = 0;

    if(HasName)
    {
        Header->Flags |= OB_FLAG_HAS_NAME_INFO;
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

    Status = ObAllocateObject(ObpTypeObjectType, &Attr, (PVOID*)&NewType);
    if (Status != 0) {
        return Status;
    }

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

    NewType->InterruptHandler = Initializer->InterruptHandler;
    NewType->CreateRoutine = Initializer->CreateRoutine;
    NewType->DeleteRoutine = Initializer->DeleteRoutine;
    NewType->ParseRoutine = Initializer->ParseRoutine;

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
    POBJECT_TYPE ExpectedType
)
{
    if (!LookupContext || !ObjectAttributes || !ObjectAttributes->ObjectName) {
        return STATUS_INVALID_PARAMETER;
    }

    LookupContext->ExpectedType = ExpectedType;
    LookupContext->ReparseCount = 0;
    LookupContext->CaseInsensitive = (ObjectAttributes->Attributes & OBJ_CASE_INSENSITIVE) != 0;
    LookupContext->RootDirectory = ObjectAttributes->RootDirectory;

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
        if (c == '*' || c == '?' || c == '<' || c == '>' || c == '"' || c == '|') {
            return TRUE; // TERDETEKSI UNSECURE!
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