#include <veakrnl.h>

VEASTATUS
VEAPI
ObpLookupObjectName(
    POBJECT_ATTRIBUTES ObjectAttributes,
    POB_LOOKUP_CONTEXT LookupContext,
    PVOID *FoundObject
);

LONG
VEAPI
ObReferenceObject(
    PVOID Object
)
{
    if (!Object) return -1;

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Object);
    Header->ReferenceCount++;

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

    if (NewRefCount == 0) {
        if (Header->Type->DeleteRoutine != NULL) {
            OB_CALLOUT_RECORD Tracker;
            Tracker.Object = Object;
            Tracker.ObjectType = Header->Type;
            Tracker.CalloutType = ObCalloutDelete;
            Tracker.RoutineAddress = (PVOID)Header->Type->DeleteRoutine;

            ObpPushCalloutTracker(NULL, &Tracker); // Ganti NULL dengan CurrentThread nanti

            // Eksekusi kode driver
            Header->Type->DeleteRoutine(Object);

            ObpPopCalloutTracker(NULL, &Tracker);
        }

        UlFreePoolWithTag(Header, Header->Type->PoolTag);
    }

    return NewRefCount;
}

VEASTATUS
VEAPI
ObReferenceObjectByName(
    POBJECT_ATTRIBUTES ObjectAttributes,
    POBJECT_TYPE ObjectType,
    PVOID *Object
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

    *ReturnedObject = ObjectBody;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObInsertObject(
    PVOID Object,
    POBJECT_ATTRIBUTES ObjectAttributes
)
{
    if (!Object) {
        return STATUS_INVALID_PARAMETER;
    }

    // 1. Biarkan objek tanpa nama (Anonymous Object)
    if (!ObjectAttributes || !ObjectAttributes->ObjectName || !ObjectAttributes->ObjectName->Buffer) {
        return STATUS_SUCCESS;
    }

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Object);
    KPROCESSOR_MODE AccessMode = UserMode; // <- contoh

    if (ObpCheckUnsecureName(ObjectAttributes->ObjectName, AccessMode)) {
        ObDereferenceObject(Object);
        return STATUS_OBJECT_NAME_INVALID;
    }

    PCHAR FullNameStr = ObjectAttributes->ObjectName->Buffer;
    USHORT NameLen = ObjectAttributes->ObjectName->Length;

    if (NameLen == 0 || FullNameStr[0] == '\0') {
        return STATUS_SUCCESS;
    }

    // 2. Gunakan Mesin Lookup dengan InsertMode = TRUE
    OB_LOOKUP_CONTEXT LookupContext;
    
    // Asumsi lu udah ngubah ObInitializeLookupContext buat nerima 3 parameter 
    // seperti di obmanager.c yang lu kasih sebelumnya
    ObInitializeLookupContext(
        &LookupContext, 
        ObjectAttributes, 
        Header->Type,     // Tipe Objek diambil dari Header
        0,               // DesiredAccess = 0 (karena hanya insert, bukan buka handle)
        AccessMode                     // AccessMode pemanggil
    );
    LookupContext.InsertMode = TRUE; // Aktifkan mode penanaman

    PVOID FoundObject = NULL;
    VEASTATUS Status = ObpLookupObjectName(ObjectAttributes, &LookupContext, &FoundObject);

    // 3. Evaluasi Hasil Pencarian
    if (Status == STATUS_SUCCESS && FoundObject != NULL) {
        // Objek dengan nama tersebut SUDAH ADA! Tabrakan nama.
        ObDereferenceObject(FoundObject);
        ObDereferenceObject(Object);
        return STATUS_OBJECT_NAME_COLLISION;
    }

    if (Status != STATUS_SUCCESS) {
        // Parent directory gagal ditemukan, atau path tidak valid
        ObDereferenceObject(Object);
        return Status;
    }

    // 4. Sukses! Mesin lookup berhenti tepat di Parent Directory
    PVOID TargetDirectory = LookupContext.CurrentDirectory;

    // Cari Final Item Name (Kita tetap butuh pointer ke nama lokal objeknya)
    // Scan mundur sebentar aja cuma buat nyari posisi batas '/'
    PCHAR FinalItemName = FullNameStr;
    for (LONG i = (LONG)NameLen - 1; i >= 0; i--) {
        if (FullNameStr[i] == '/' || FullNameStr[i] == '\\') {
            FinalItemName = &FullNameStr[i + 1];
            break;
        }
    }

    // 5. Rakit Name Info

    if(Header->Flags & OB_FLAG_HAS_NAME_INFO)
    {
        POBJECT_HEADER_NAME_INFO NameInfo = OBJECT_HEADER_TO_NAME_INFO(Header);
        
        NameInfo->Directory = (POBJECT_DIRECTORY)TargetDirectory;
        NameInfo->Name.Length = (USHORT)RtlRawStringLength(FinalItemName);
        NameInfo->Name.MaximumLength = NameInfo->Name.Length + 1;
        NameInfo->Name.Buffer = UlAllocatePoolWithTag(PagedPool, NameInfo->Name.MaximumLength, 'NmOb');
        
        RtlCopyRawString(NameInfo->Name.Buffer, FinalItemName);
    }

    // 6. Eksekusi Penanaman
    Status = ObpInsertDirectory(TargetDirectory, Object);

    // 7. Handle Kegagalan Insert
    if (Status != STATUS_SUCCESS) {
        ObDereferenceObject(Object); 
        return Status;
    }

    return STATUS_SUCCESS;
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
    PHANDLE_TABLE Table,
    HANDLE Handle,
    ULONG DesiredAccess,
    POBJECT_TYPE ExpectedType,
    KPROCESSOR_MODE AccessMode,
    PVOID *Object
)
{
    if (!Table || !Handle || !Object) {
        return STATUS_INVALID_PARAMETER;
    }

    // 1. Ekstrak Index
    ULONG Index = HANDLE_TO_INDEX(Handle);

    // TODO: AcquireSpinLock(&Table->Lock)

    // 2. Validasi Batas Index
    if (Index == 0 || Index >= Table->MaxEntries) {
        // ReleaseSpinLock(&Table->Lock);
        return STATUS_INVALID_HANDLE;
    }

    PHANDLE_TABLE_ENTRY Entry = &Table->Entries[Index];

    // 3. Pastikan handle menunjuk ke objek yang valid
    if (Entry->Object == NULL) {
        // ReleaseSpinLock(&Table->Lock);
        return STATUS_INVALID_HANDLE;
    }

    PVOID FoundObject = Entry->Object;
    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(FoundObject);

    // 4. Validasi Tipe Objek (Type Enforcement)
    if (ExpectedType != NULL && Header->Type != ExpectedType) {
        // ReleaseSpinLock(&Table->Lock);
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    // 5. Validasi Hak Akses (Access Control)
    // Jika dari UserMode, pastikan GrantedAccess di Handle memiliki akses yang diminta
    if (AccessMode == UserMode) {
        // Periksa apakah semua bit di DesiredAccess tersedia di GrantedAccess
        if ((Entry->GrantedAccess & DesiredAccess) != DesiredAccess) {
            // ReleaseSpinLock(&Table->Lock);
            return STATUS_ACCESS_DENIED;
        }
    }

    // 6. Tambahkan Reference Count sebelum dikembalikan ke caller
    ObReferenceObject(FoundObject);

    // TODO: ReleaseSpinLock(&Table->Lock)

    *Object = FoundObject;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObOpenObjectByName(
    PHANDLE_TABLE Table,
    POBJECT_ATTRIBUTES ObjectAttributes,
    POBJECT_TYPE ExpectedType,
    KPROCESSOR_MODE AccessMode,
    ULONG DesiredAccess,
    PHANDLE ReturnedHandle
)
{
    if (!ObjectAttributes || !ReturnedHandle) {
        return STATUS_INVALID_PARAMETER;
    }

    *ReturnedHandle = NULL;

    // Inisialisasi Lookup Context
    OB_LOOKUP_CONTEXT LookupContext;
    ObInitializeLookupContext(
        &LookupContext, 
        ObjectAttributes, 
        ExpectedType, 
        DesiredAccess, 
        AccessMode
    );
    LookupContext.AccessMode = AccessMode;

    // Eksekusi Pencarian Objek via Path String
    PVOID FoundObject = NULL;
    VEASTATUS Status = ObpLookupObjectName(ObjectAttributes, &LookupContext, &FoundObject);

    if (!VEA_SUCCESS(Status)) {
        return Status; // Objek gak ketemu / type mismatch / path invalid
    }

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(FoundObject);
    ULONG GrantedAccess = 0;

    if(AccessMode == UserMode)
    {
        PVEA_TOKEN ClientToken = PtGetCurrentProcess()->Token;

        BOOLEAN AccessAllowed = SfAccessCheck(
            Header->SecurityDescriptor,
            ClientToken,
            DesiredAccess,
            &Header->Type->GenericMapping,
            &GrantedAccess
        );

        if (!AccessAllowed) {
            ObDereferenceObject(FoundObject);
            return STATUS_ACCESS_DENIED; // AKSED DITOLAK BY SF!
        }
    }
    else
    {
        GrantedAccess = DesiredAccess;
    }

    // Daftarkan Objek ke Handle Table untuk Menerbitkan HANDLE
    HANDLE NewHandle = UlCreateHandle(Table, FoundObject, DesiredAccess);

    if (!NewHandle) {
        // Jika pembuatan handle gagal (misal tabel penuh),
        // Batalkan reference count yang ditambah oleh ObpLookupObjectName!
        ObDereferenceObject(FoundObject);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    // Return Handle Baru ke Pemanggil
    *ReturnedHandle = NewHandle;

    return STATUS_SUCCESS;
}