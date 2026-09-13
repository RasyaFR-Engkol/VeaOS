#include <veakrnl.h>

POBJECT_TYPE ObpTypeObjectType = NULL;
POBJECT_TYPE ObpDirectoryObjectType = NULL;
LIST_ENTRY ObpTypeObjectList;  // Rantai buat nyimpen semua tipe
ULONG ObpNextTypeId = 2;       // ID 1 udah dipake ObpTypeObjectType tadi

LONG
VEAPI
ObpDirectoryCreateRoutine(
    PVOID ObjectBody
);

VOID
VEAPI
ObpInitializeObjectManager(
    VOID
)
{
    InitializeListHead(&ObpTypeObjectList);

    // Alokasi memori mentah dari pool untuk Header + Body dari ObpTypeObjectType
    ULONG TypeObjSize = sizeof(OBJECT_HEADER_NAME_INFO) + sizeof(OBJECT_HEADER) + sizeof(OBJECT_TYPE);
    PVOID RawTypeMem = UlAllocatePoolWithTag(NonPagedPool, TypeObjSize, 'TpOj');
    RtlZeroMemory(RawTypeMem, TypeObjSize);

    // Set Header-nya
    POBJECT_HEADER TypeHeader = (POBJECT_HEADER)RawTypeMem;
    TypeHeader->ReferenceCount = 1;
    TypeHeader->StackDepth = 1;
    TypeHeader->Flags = OB_FLAG_HAS_NAME_INFO;
    
    // PENTING: Solusi ayam & telur. Type dari "Type" adalah dirinya sendiri!
    POBJECT_TYPE TypeBody = (POBJECT_TYPE)(TypeHeader + 1);
    TypeHeader->Type = TypeBody; 

    // Isi data body ObpTypeObjectType
    TypeBody->TypeId = 1; // ID pertama
    RtlCopyMemory(TypeBody->TypeName, "Type", 5);
    TypeBody->PoolTag = 'TpOj';
    TypeBody->ObjectSize = sizeof(OBJECT_TYPE);
    TypeBody->TotalObjectCount = 1;
    InitializeListHead(&TypeBody->TypeList);

    // Isi info nama untuk objek tipe
    POBJECT_HEADER_NAME_INFO NameInfo = OBJECT_HEADER_TO_NAME_INFO(TypeHeader);
    NameInfo->Name.Length = 4;
    NameInfo->Name.MaximumLength = 5;
    NameInfo->Name.Buffer = UlAllocatePoolWithTag(PagedPool, 5, 'NmOb');
    RtlCopyMemory(NameInfo->Name.Buffer, "Type", 5);

    // Simpan ke pointer global
    ObpTypeObjectType = TypeBody;

    InsertTailList(&ObpTypeObjectList, &TypeBody->TypeList);
    
    OBJECT_TYPE_INITIALIZER DirInitializer;
    RtlZeroMemory(&DirInitializer, sizeof(OBJECT_TYPE_INITIALIZER));
    DirInitializer.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    DirInitializer.PoolTag = 'DiOb';
    DirInitializer.ObjectSize = sizeof(OBJECT_DIRECTORY);
    DirInitializer.AllowAttachByDefault = FALSE;
    DirInitializer.InterruptHandler = NULL;
    DirInitializer.CreateRoutine = ObpDirectoryCreateRoutine;
    DirInitializer.DeleteRoutine = NULL;
    DirInitializer.ParseRoutine = NULL;

    // Panggil fungsi create type (fungsi pembungkus ObAllocateObject yang lu buat)
    // Fungsi ini sekarang aman dipanggil karena ObpTypeObjectType sudah eksis!
    ObCreateObjectType("Directory", &DirInitializer, &ObpDirectoryObjectType);

    // Inisialisasi SymLink
    OBJECT_TYPE_INITIALIZER SymlinkInitializer;
    RtlZeroMemory(&SymlinkInitializer, sizeof(OBJECT_TYPE_INITIALIZER));
    SymlinkInitializer.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    SymlinkInitializer.PoolTag = 'SymL'; // Tag alokasi
    SymlinkInitializer.ObjectSize = sizeof(OBJECT_SYMBOLIC_LINK);
    SymlinkInitializer.ParseRoutine = NULL;
    SymlinkInitializer.DeleteRoutine = ObpDeleteSymbolicLink;
    SymlinkInitializer.ParseRoutine = ObpParseSymbolicLink;

    ObCreateObjectType("SymbolicLink", &SymlinkInitializer, &ObpSymbolicLinkObjectType);
    
    kdp_print("OB: Object Manager successfully initialized!\n\r");
}

BOOLEAN
VEAPI
ObInitSystem(VOID)
{
    ObpInitializeObjectManager();

    ObpCreateRootStructure();

    ObioInitSubsystem();

    return TRUE;
}