#include <veakrnl.h>

BOOLEAN ObReportLifecycleReady = FALSE;
POB_LIFECYCLE_REPORT_TABLE ObLifecycleTable = NULL;
ULONG ObLifecycleTableFreeIndex = 0;

VEASTATUS
VEAPI
ObpLookupObjectName(
    POBJECT_ATTRIBUTES ObjectAttributes,
    POB_LOOKUP_CONTEXT LookupContext,
    PVOID *FoundObject
);

static VEASTATUS
ObpCheckAccess(
    POBJECT_HEADER  Header,
    KPROCESSOR_MODE AccessMode,
    PVEA_TOKEN      Token,
    ACCESS_MASK     DesiredAccess,
    PACCESS_MASK    GrantedAccess)
{
    if (AccessMode != UserMode)
    {
        *GrantedAccess = DesiredAccess;
        return STATUS_SUCCESS;
    }

    if (!Token)
        Token = PtGetCurrentProcess()->Token;

    if (!SfAccessCheck(Header->SecurityDescriptor, Token, DesiredAccess,
                       &Header->Type->GenericMapping, GrantedAccess))
        return STATUS_ACCESS_DENIED;

    return STATUS_SUCCESS;
}

VOID
VEAPI
ObpReportLifecycle(
    IN PVOID Object,
    IN ULONG CurrentReference,
    IN BOOLEAN IsReferenced,
    IN BOOLEAN IsHandle,
    IN PVOID OwnerThread,
    IN PVOID TargetObject
)
{
    /* Init ObReportLifecycleTable */
    if (ObReportLifecycleReady == FALSE ||
        !ObLifecycleTable)
    {
        ObLifecycleTable = (POB_LIFECYCLE_REPORT_TABLE)UlAllocatePoolZero(
            NonPagedPool,
            100 * sizeof(OB_LIFECYCLE_REPORT_TABLE),
            TAG_OB
        );

        if (!ObLifecycleTable)
        {
            DPRINT("WARN: ObLifecycleTable cannot be used because lack of memory.\n");
            return;
        }

        ObReportLifecycleReady = TRUE;
    }

    /* Make sure isn't full */
    if (ObLifecycleTableFreeIndex == 100)
    {
        return;
    }

    /* Insert new report */
    POB_LIFECYCLE_REPORT_TABLE ObjectReportTable = &ObLifecycleTable[ObLifecycleTableFreeIndex];
    ObjectReportTable->ObjectOrHandle = Object;
    ObjectReportTable->TargetObject = TargetObject;
    ObjectReportTable->OwnerThread = OwnerThread;
    ObjectReportTable->IsHandle = IsHandle;
    ObjectReportTable->CurrentReference = CurrentReference;
    ObjectReportTable->IsReferenced = IsReferenced;
    
    /* Increment next free index */
    ObLifecycleTableFreeIndex++;
}

LONG
VEAPI
ObReferenceObject(
    PVOID Object
)
{
    if (!Object) return -1;

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Object);
    Header->ReferenceCount++;

    /* Report */
    ObpReportLifecycle(
        Object,
        Header->ReferenceCount,
        TRUE,
        FALSE,
        NULL,
        Object
    );

    return Header->ReferenceCount;
}

LONG
VEAPI
ObDereferenceObject(
    PVOID Object
)
{
    if (!Object) return -1;

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Object);
    Header->ReferenceCount--;

    LONG NewRefCount = Header->ReferenceCount;

    /* Report */
    ObpReportLifecycle(
        Object,
        NewRefCount,
        FALSE,
        FALSE,
        NULL,
        Object
    );

    if (NewRefCount == 0) {
        if (Header->Flags & OB_FLAG_PERMANENT) {
            DPRINT("[OB] CRITICAL ERROR: Permanent object reference hit 0! Object: %p\n", Object);
            return NewRefCount; 
        }

        if (Header->Type && Header->Type->DeleteRoutine != NULL) {
            OB_CALLOUT_RECORD Tracker;
            Tracker.Object = Object;
            Tracker.ObjectType = Header->Type;
            Tracker.CalloutType = ObCalloutDelete;
            Tracker.RoutineAddress = (PVOID)Header->Type->DeleteRoutine;

            ObpPushCalloutTracker(NULL, &Tracker); // Ganti NULL dengan CurrentThread nanti
            Header->Type->DeleteRoutine(Object);
            ObpPopCalloutTracker(NULL, &Tracker);
        }

        /* Release the separately allocated object name before freeing the
           object allocation that contains the header and body. */
        if (Header->Flags & OB_FLAG_HAS_NAME_INFO)
        {
            POBJECT_HEADER_NAME_INFO NameInfo = OBJECT_HEADER_TO_NAME_INFO(Header);

            /* Jika objek ternyata masih terhubung ke Directory, cabut dulu! */
            if (NameInfo->Directory != NULL)
            {
                ObpRemoveEntryDirectory(Object);
            }

            if (NameInfo->Name.Buffer != NULL)
            {
                UlFreePoolWithTag(NameInfo->Name.Buffer, 'NmOb');
                NameInfo->Name.Buffer = NULL;
            }
        }

        /* Named objects place name metadata before the object header. */
        PVOID AllocationBase = (PVOID)Header;
        if (Header->Flags & OB_FLAG_HAS_NAME_INFO)
        {
            AllocationBase = (PVOID)((PUCHAR)Header - sizeof(OBJECT_HEADER_NAME_INFO));
        }

        if (Header->CreateInfo)
        {
            UlFreePoolWithTag(Header->CreateInfo, TAG_OB);
            Header->CreateInfo = NULL;
        }

        UlFreePoolWithTag(AllocationBase, Header->Type->PoolTag);
    }

    return NewRefCount;
}

VEASTATUS
VEAPI
ObReferenceObjectByName(
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    IN POBJECT_TYPE ObjectType,
    OUT PVOID *Object
)
{
    if (!ObjectAttributes || !Object) {
        return STATUS_INVALID_PARAMETER;
    }

    OB_LOOKUP_CONTEXT LookupContext;
    RtlZeroMemory(&LookupContext, sizeof(OB_LOOKUP_CONTEXT));

    LookupContext.ExpectedType = ObjectType;

    // Panggil mesin lookup yang udah terupdate
    return ObpLookupObjectName(ObjectAttributes, &LookupContext, Object);
}

/* DEBUG: */

VEASTATUS
VEAPI
ObCreateObject(
    IN KPROCESSOR_MODE ProcessorMode,
    IN POBJECT_TYPE ObjectType,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    IN KPROCESSOR_MODE OwnershipMode,
    IN OUT PVOID ParseContext OPTIONAL,
    IN ULONG ObjectSize,
    IN ULONG PagedPoolCharge,
    IN ULONG NonPagedPoolCharge,
    OUT PVOID *ReturnedObject
)
{
    if (!ObjectType || !ReturnedObject) {
        return STATUS_INVALID_PARAMETER;
    }

    PVOID ObjectBody = NULL;

    VEASTATUS AllocStatus = ObAllocateObject(ObjectType, ObjectAttributes, ObjectSize, &ObjectBody);
    if(!VEA_SUCCESS(AllocStatus))
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(ObjectBody);
    ULONG NameLen = (ObjectAttributes && ObjectAttributes->ObjectName)
                        ? ObjectAttributes->ObjectName->Length : 0;

    POB_CREATE_INFO Info = (POB_CREATE_INFO)UlAllocatePoolZero(
        NonPagedPool, sizeof(OB_CREATE_INFO) + NameLen + 1, TAG_OB);
    if (!Info)
    {
        ObDereferenceObject(ObjectBody);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    if (ObjectAttributes)
        Info->Attributes = *ObjectAttributes;

    if (NameLen)
    {
        PCHAR Buf = (PCHAR)(Info + 1);
        RtlCopyMemory(Buf, ObjectAttributes->ObjectName->Buffer, NameLen);
        Buf[NameLen] = '\0';
        Info->Name.Buffer        = Buf;
        Info->Name.Length        = (USHORT)NameLen;
        Info->Name.MaximumLength = (USHORT)(NameLen + 1);
        Info->Attributes.ObjectName = &Info->Name;
    }
    else
    {
        Info->Attributes.ObjectName = NULL;
    }

    Info->ProcessorMode      = ProcessorMode;
    Info->OwnershipMode      = OwnershipMode;
    Info->ParseContext       = ParseContext;
    Info->PagedPoolCharge    = PagedPoolCharge;
    Info->NonPagedPoolCharge = NonPagedPoolCharge;
    Header->CreateInfo       = Info;

    *ReturnedObject = ObjectBody;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObInsertObject(
    PVOID Object,
    IN PACCESS_STATE AccessState OPTIONAL,
    IN ACCESS_MASK DesiredAccess,
    IN ULONG ObjectPointerBias,
    OUT PVOID *NewObject OPTIONAL,
    OUT PHANDLE Handle OPTIONAL
)
{
    POBJECT_HEADER   Header;
    POB_CREATE_INFO  Info;
    KPROCESSOR_MODE  AccessMode;
    ACCESS_MASK      GrantedAccess = DesiredAccess;
    PVEA_TOKEN       Token = AccessState ? AccessState->Token : NULL;
    HANDLE           NewHandle = NULL;
    VEASTATUS        Status;
    BOOLEAN          Named;
    ULONG            Attr;
    PVOID            TargetDirectory;

    if (Handle)    *Handle = NULL;
    if (NewObject) *NewObject = NULL;

    if (!Object) 
    {
        DPRINT("[OB] ObInsertObject FAIL: Object pointer is NULL!\n");
        return STATUS_INVALID_PARAMETER;
    }

    Header = OBJECT_TO_OBJECT_HEADER(Object);
    Info   = Header->CreateInfo;
    Attr   = Info ? Info->Attributes.Attributes : 0;

    if (Attr & OBJ_PERMANENT)
    {
        Header->Flags |= OB_FLAG_PERMANENT;
    }

    AccessMode = Info ? Info->ProcessorMode : KernelMode;
    Named = (Info && Info->Name.Buffer && Info->Name.Length != 0);

    if (ObjectPointerBias != 0 && !NewObject)
    {
        DPRINT("[OB] ObInsertObject FAIL: Bias != 0 but NewObject is NULL!\n");
        Status = STATUS_INVALID_PARAMETER;
        goto fail;
    }

    if (Named && ObpCheckUnsecureName(&Info->Name, AccessMode))
    {
        DPRINT("[OB] ObInsertObject FAIL: ObpCheckUnsecureName failed for '%Z'\n", 
               &Info->Name);
        Status = STATUS_OBJECT_NAME_INVALID;
        goto fail;
    }

    if (Handle && AccessMode == UserMode && (Attr & OBJ_KERNEL_HANDLE))
    {
        DPRINT("[OB] ObInsertObject FAIL: UserMode requested OBJ_KERNEL_HANDLE!\n");
        Status = STATUS_INVALID_PARAMETER;
        goto fail;
    }

    /* Access check dulu, sebelum ada state yang berubah */
    if (Handle)
    {
        Status = ObpCheckAccess(Header, AccessMode, Token, DesiredAccess, &GrantedAccess);
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("[OB] ObInsertObject FAIL: ObpCheckAccess failed: 0x%X\n", Status);
            goto fail;
        }

        Status = ObpCreateHandle(Object, Attr, AccessMode, GrantedAccess, &NewHandle);
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("[OB] ObInsertObject FAIL: ObpCreateHandle failed: 0x%X\n", Status);
            goto fail;
        }
    }

    if (Named && ObpRootDirectoryObject == NULL)
    {
        /* Bootstrap: satu-satunya objek yang boleh masuk tanpa root adalah "/" itu sendiri */
        if (Header->Type != ObpDirectoryObjectType ||
            Info->Name.Length != 1 || Info->Name.Buffer[0] != '/')
        {
            Status = STATUS_OBJECT_PATH_NOT_FOUND;
            goto fail;
        }

        /* Root tidak punya parent dan tidak punya nama di NameInfo */
        if (Header->Flags & OB_FLAG_HAS_NAME_INFO)
        {
            POBJECT_HEADER_NAME_INFO NI = OBJECT_HEADER_TO_NAME_INFO(Header);
            NI->Directory = NULL;
        }

        ObpRootDirectoryObject = Object;
    }
    else if (Named)
    {
        OB_LOOKUP_CONTEXT LookupContext;
        PVOID FoundObject = NULL;
        PCHAR FinalName;

        Status = ObInitializeLookupContext(&LookupContext, &Info->Attributes,
                                           Header->Type, 0, AccessMode);
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("[OB] ObInsertObject FAIL: ObInitializeLookupContext failed: 0x%X\n", Status);
            goto fail;
        }

        LookupContext.InsertMode = TRUE;

        Status = ObpLookupObjectName(&Info->Attributes, &LookupContext, &FoundObject);

        if (Status == STATUS_SUCCESS && FoundObject != NULL)
        {
            DPRINT("[OB] ObInsertObject FAIL: Name collision for '%Z' (FoundObject=%p)\n", 
                   &Info->Name, FoundObject);
            ObDereferenceObject(FoundObject);
            Status = STATUS_OBJECT_NAME_COLLISION;
            goto fail;
        }
        
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("[OB] ObInsertObject FAIL: ObpLookupObjectName failed: 0x%X for path '%Z'\n", 
                   Status, &Info->Name);
            goto fail;
        }

        TargetDirectory = LookupContext.CurrentDirectory;

        if (!TargetDirectory)
        {
            DPRINT("[OB] ObInsertObject FAIL: TargetDirectory is NULL after lookup for '%Z'!\n",
                   &Info->Name);
            Status = STATUS_OBJECT_PATH_NOT_FOUND;
            goto fail;
        }

        FinalName = Info->Name.Buffer;
        for (LONG i = (LONG)Info->Name.Length - 1; i >= 0; i--)
        {
            if (Info->Name.Buffer[i] == '/' || Info->Name.Buffer[i] == '\\')
            {
                FinalName = &Info->Name.Buffer[i + 1];
                break;
            }
        }

        POBJECT_HEADER TargetHeader = OBJECT_TO_OBJECT_HEADER(TargetDirectory);

        if (Header->Flags & OB_FLAG_HAS_NAME_INFO)
        {
            POBJECT_HEADER_NAME_INFO NameInfo = OBJECT_HEADER_TO_NAME_INFO(Header);
            USHORT Len = (USHORT)RtlRawStringLength(FinalName);

            NameInfo->Name.Buffer = UlAllocatePoolWithTag(NonPagedPool, Len + 1, 'NmOb');
            if (!NameInfo->Name.Buffer)
            {
                DPRINT("[OB] ObInsertObject FAIL: Failed to allocate NameInfo buffer for '%s'!\n", FinalName);
                Status = STATUS_INSUFFICIENT_MEMORY;
                goto fail;
            }
            if (TargetHeader->Type == ObpDirectoryObjectType)
            {
                NameInfo->Directory = (POBJECT_DIRECTORY)TargetDirectory;
            }
            else
            {
                NameInfo->Directory = NULL; // Namespace khusus (misal Registry Key)
            }
            NameInfo->Name.Length        = Len;
            NameInfo->Name.MaximumLength = Len + 1;
            RtlCopyRawString(NameInfo->Name.Buffer, FinalName);
            NameInfo->Name.Buffer[Len]   = '\0';
        }

        if (TargetHeader->Type == ObpDirectoryObjectType)
        {
            Status = ObpInsertDirectory(TargetDirectory, Object);
            if (!VEA_SUCCESS(Status))
            {
                DPRINT("[OB] ObInsertObject FAIL: ObpInsertDirectory failed: 0x%X\n", Status);
                goto fail;
            }
        }
    }

    if (NewObject)
    {
        for (ULONG i = 0; i < ObjectPointerBias; i++)
            ObReferenceObject(Object);
        *NewObject = Object;
    }
    else
    {
        ObDereferenceObject(Object);   /* lepas ref pembuat */
    }

    if (Handle) *Handle = NewHandle;

    if (Header->CreateInfo)
    {
        UlFreePoolWithTag(Header->CreateInfo, TAG_OB);
        Header->CreateInfo = NULL;
    }

    return STATUS_SUCCESS;

fail:
    /* Objek belum masuk directory di titik mana pun yang sampai ke sini */
    if (Header->Flags & OB_FLAG_HAS_NAME_INFO)
        OBJECT_HEADER_TO_NAME_INFO(Header)->Directory = NULL;

    if (NewHandle)
        ObCloseHandle(NewHandle, KernelMode);

    ObDereferenceObject(Object); 

    DPRINT("[OB] ObInsertObject FAILED with status 0x%X (Named: %d, Path: %s)\n", 
           Status, Named, (Info && Info->Name.Buffer) ? Info->Name.Buffer : "<unnamed>");
    return Status;
}

VEASTATUS
VEAPI
ObReferenceObjectByPointer(
    PVOID Object,
    ULONG DesiredAccess,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode
)
{
    (VOID)DesiredAccess;

    if (!Object) {
        return STATUS_INVALID_PARAMETER;
    }

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Object);

    if (ObjectType != NULL && Header->Type != ObjectType) {
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    if (AccessMode == UserMode) {
        // User Mode tidak boleh mengirim pointer objek yang mengarah ke alamat yang tidak valid
        // (Misal: Memastikan pointer berada di batas alamat kernel VeaOS >= 0xC0000000)
        if ((ULONG_PTR)Object < 0xC0000000) {
            return STATUS_ACCESS_VIOLATION;
        }

        // NANTI DI SINI: Tempat validasi DesiredAccess vs SecurityDescriptor milik Header
    }

    ObReferenceObject(Object);

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObReferenceObjectByHandle(
    HANDLE Handle,
    ULONG DesiredAccess,
    POBJECT_TYPE ExpectedType,
    KPROCESSOR_MODE AccessMode,
    PVOID *Object
)
{
    if (!Handle || !Object) {
        return STATUS_INVALID_PARAMETER;
    }

    *Object = NULL;

    PHANDLE_TABLE Table = ObpGetHandleTable(&Handle, AccessMode);
    if (!Table) {
        return STATUS_INVALID_HANDLE;
    }

    ULONG Index = HANDLE_TO_INDEX(Handle);

    if (Index == 0 || Index >= Table->MaxEntries) {
        return STATUS_INVALID_HANDLE;
    }

    PHANDLE_TABLE_ENTRY Entry = &Table->Entries[Index];

    if (Entry->Object == NULL) {
        return STATUS_INVALID_HANDLE;
    }

    PVOID FoundObject = Entry->Object;
    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(FoundObject);


    if (ExpectedType != NULL && Header->Type != ExpectedType) {
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    if (AccessMode == UserMode) {
        if ((Entry->GrantedAccess & DesiredAccess) != DesiredAccess) {
            return STATUS_ACCESS_DENIED;
        }
    }

    ObReferenceObject(FoundObject);

    *Object = FoundObject;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObOpenObjectByName(
    POBJECT_ATTRIBUTES ObjectAttributes,
    POBJECT_TYPE       ExpectedType,
    KPROCESSOR_MODE    AccessMode,
    ULONG              DesiredAccess,
    PVOID              ParseContext,
    PHANDLE            ReturnedHandle
)
{
    OB_LOOKUP_CONTEXT LookupContext;
    PVOID             FoundObject = NULL;
    POBJECT_HEADER    Header;
    ACCESS_MASK       GrantedAccess = 0;
    VEASTATUS         Status;

    if (!ObjectAttributes || !ReturnedHandle)
        return STATUS_INVALID_PARAMETER;

    *ReturnedHandle = NULL;

    Status = ObInitializeLookupContext(&LookupContext, ObjectAttributes,
                                       ExpectedType, DesiredAccess, AccessMode);
    if (!VEA_SUCCESS(Status))
        return Status;

    LookupContext.ParseContext = ParseContext;

    Status = ObpLookupObjectName(ObjectAttributes, &LookupContext, &FoundObject);
    if (!VEA_SUCCESS(Status))
        return Status;
    
    if (!FoundObject)
        return STATUS_OBJECT_NAME_NOT_FOUND;

    Header = OBJECT_TO_OBJECT_HEADER(FoundObject);
    Status = ObpCheckAccess(Header, AccessMode, NULL, DesiredAccess, &GrantedAccess);
    if (!VEA_SUCCESS(Status))
    {
        ObDereferenceObject(FoundObject);
        return Status;
    }

    Status = ObpCreateHandle(FoundObject, ObjectAttributes->Attributes,
                             AccessMode, GrantedAccess, ReturnedHandle);

    ObDereferenceObject(FoundObject);

    return Status;
}

VOID 
VEAPI 
ObCloseHandle(IN HANDLE Handle, IN KPROCESSOR_MODE Mode)
{
    PHANDLE_TABLE Table;

    if (!Handle) return;
    Table = ObpGetHandleTable(&Handle, Mode);
    if (!Table) return;

    UlDestroyHandle(Table, Handle);
}

VOID
VEAPI
VeaClose(IN HANDLE Handle)
{    
    /* Call internal API */
    /* TODO: Get Previous Mode for safety */
    ObCloseHandle(Handle, KernelMode);
}