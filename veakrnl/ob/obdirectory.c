#include <veakrnl.h>

PVOID ObpRootDirectoryObject = NULL;
PVOID ObpDeviceDirectoryObject = NULL;
PVOID ObpDriverDirectoryObject = NULL;
PVOID ObpTypesDirectoryObject = NULL;

POBJECT_TYPE ObpSymbolicLinkObjectType = NULL;

LONG
VEAPI
ObpDirectoryCreateRoutine(
    PVOID ObjectBody
)
{
    if (!ObjectBody) return -1;

    POBJECT_DIRECTORY Dir = (POBJECT_DIRECTORY)ObjectBody;
    
    // Inisialisasi rantai list bawaan direktori baru agar Flink & Blink menunjuk ke dirinya sendiri!
    InitializeListHead(&Dir->Head);

    return 0;
}

VOID
VEAPI
ObpRemoveEntryDirectory(
    PVOID Object
)
{
    if (!Object) return;

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Object);
    if (!(Header->Flags & OB_FLAG_HAS_NAME_INFO)) return;

    POBJECT_HEADER_NAME_INFO NameInfo = OBJECT_HEADER_TO_NAME_INFO(Header);
    POBJECT_DIRECTORY Dir = NameInfo->Directory;

    if (!Dir) return;

    if (Dir->Head.Flink == NULL || Dir->Head.Blink == NULL) {
        InitializeListHead(&Dir->Head);
        NameInfo->Directory = NULL;
        return;
    }

    PLIST_ENTRY Current = Dir->Head.Flink;
    while (Current != &Dir->Head)
    {
        POBJECT_DIRECTORY_ENTRY Entry = CONTAINING_RECORD(Current, OBJECT_DIRECTORY_ENTRY, Chain);
        
        if (Entry->Object == Object)
        {
            RemoveEntryList(&Entry->Chain);
            
            UlFreePoolWithTag(Entry, 'ObEn');
            
            NameInfo->Directory = NULL;
            break;
        }
        Current = Current->Flink;
    }
}

VEASTATUS
VEAPI
ObpInsertDirectory(
    PVOID DirectoryObject,
    PVOID Object
)
{
    if (!DirectoryObject || !Object) {
        return STATUS_INVALID_PARAMETER;
    }

    POBJECT_DIRECTORY Dir = (POBJECT_DIRECTORY)DirectoryObject;

    if (Dir->Head.Flink == NULL || Dir->Head.Blink == NULL) {
        InitializeListHead(&Dir->Head);
    }

    POBJECT_HEADER NewHeader = OBJECT_TO_OBJECT_HEADER(Object);

    if (!(NewHeader->Flags & OB_FLAG_HAS_NAME_INFO)) {
        return STATUS_INVALID_PARAMETER;
    }

    POBJECT_HEADER_NAME_INFO NewNameInfo = OBJECT_HEADER_TO_NAME_INFO(NewHeader);
    PCHAR NewName = NewNameInfo->Name.Buffer;
    PLIST_ENTRY Current = Dir->Head.Flink;

    while (Current != &Dir->Head)
    {
        POBJECT_DIRECTORY_ENTRY ExistingEntry = CONTAINING_RECORD(Current, OBJECT_DIRECTORY_ENTRY, Chain);
        POBJECT_HEADER ExistingHeader = OBJECT_TO_OBJECT_HEADER(ExistingEntry->Object);

        if(ExistingHeader->Flags & OB_FLAG_HAS_NAME_INFO)
        {
            POBJECT_HEADER_NAME_INFO NameInfo = OBJECT_HEADER_TO_NAME_INFO(ExistingHeader);
            // Perbandingan string case-insensitive
            PCHAR S1 = NameInfo->Name.Buffer;
            PCHAR S2 = NewName;
            BOOLEAN Match = TRUE;

            while (*S1 != '\0' || *S2 != '\0') {
                CHAR c1 = *S1;
                CHAR c2 = *S2;

                // Konversi ke lowercase untuk perbandingan aman
                if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
                if (c2 >= 'A' && c2 <= 'Z') c2 += 32;

                if (c1 != c2) {
                    Match = FALSE;
                    break;
                }
                S1++;
                S2++;
            }

            // Jika nama ditemukan, batalkan dan return status tabrakan nama!
            if (Match) {
                return STATUS_OBJECT_NAME_COLLISION;
            }
        }
        Current = Current->Flink;
    }

    POBJECT_DIRECTORY_ENTRY Entry = UlAllocatePoolWithTag(NonPagedPool, sizeof(OBJECT_DIRECTORY_ENTRY), 'ObEn');
    if (!Entry) return STATUS_INSUFFICIENT_MEMORY;

    Entry->Object = Object;
    ObReferenceObject(Object);

    InsertTailList(&Dir->Head, &Entry->Chain);

    return STATUS_SUCCESS;
}

static
VEASTATUS
VEAPI
ObpCreateDirectory(
    IN  PCHAR  Name OPTIONAL,          /* NULL = root */
    IN  PVOID  Parent OPTIONAL,
    OUT PVOID *Directory
)
{
    OBJECT_ATTRIBUTES Attr;
    ANSI_STRING       NameAnsi;
    PVOID             Dir = NULL;
    VEASTATUS         Status;

    *Directory = NULL;

    if (Name)
    {
        RtlInitAnsiString(&NameAnsi, Name);
        InitializeObjectAttributes(&Attr, &NameAnsi, OBJ_CASE_INSENSITIVE, Parent, NULL);
    }

    Status = ObCreateObject(KernelMode, ObpDirectoryObjectType,
                            Name ? &Attr : NULL, KernelMode,
                            NULL, 0, 0, 0, &Dir);
    if (!VEA_SUCCESS(Status))
        return Status;

    InitializeListHead(&((POBJECT_DIRECTORY)Dir)->Head);   /* hapus jika CreateRoutine sudah */

    /* Pegang ref pembuat selamanya (objek permanent): minta pointer balik */
    Status = ObInsertObject(Dir, NULL, 0, 0, &Dir, NULL);
    if (!VEA_SUCCESS(Status))
        return Status;               /* ObInsertObject sudah melepas objek saat gagal */

    *Directory = Dir;
    return STATUS_SUCCESS;
}

#if 0

VEASTATUS
VEAPI
ObpCreateRootStructure(VOID)
{
    VEASTATUS Status;

    if (!ObpDirectoryObjectType)
    {
        kdp_print("OB: FATAL: Directory type missing\n\r");
        return STATUS_UNSUCCESSFUL;
    }

    /* 1. Root "\" : tanpa nama, jadi ObInsertObject tidak masuk ke namespace */
    Status = ObpCreateDirectory(NULL, NULL, &ObpRootDirectoryObject);
    if (!VEA_SUCCESS(Status)) goto fatal;

    /* 2-4. Sub-direktori, semuanya anak dari root */
    Status = ObpCreateDirectory("Device", ObpRootDirectoryObject, &ObpDeviceDirectoryObject);
    if (!VEA_SUCCESS(Status)) goto fatal;

    Status = ObpCreateDirectory("Driver", ObpRootDirectoryObject, &ObpDriverDirectoryObject);
    if (!VEA_SUCCESS(Status)) goto fatal;

    Status = ObpCreateDirectory("Types", ObpRootDirectoryObject, &ObpTypesDirectoryObject);
    if (!VEA_SUCCESS(Status)) goto fatal;

    /* 5. Type yang sudah terdaftar sebelum /Types ada */
    for (PLIST_ENTRY Curr = ObpTypeObjectList.Flink;
         Curr != &ObpTypeObjectList;
         Curr = Curr->Flink)
    {
        POBJECT_TYPE   TypeObj = CONTAINING_RECORD(Curr, OBJECT_TYPE, TypeList);
        POBJECT_HEADER Header  = OBJECT_TO_OBJECT_HEADER(TypeObj);

        if (!(Header->Flags & OB_FLAG_HAS_NAME_INFO))
            continue;

        POBJECT_HEADER_NAME_INFO NI = OBJECT_HEADER_TO_NAME_INFO(Header);
        if (!NI->Name.Buffer)
            continue;                      /* belum punya nama final */

        NI->Directory = (POBJECT_DIRECTORY)ObpTypesDirectoryObject;
        Status = ObpInsertDirectory(ObpTypesDirectoryObject, TypeObj);
        if (!VEA_SUCCESS(Status)) goto fatal;
    }

    kdp_print("OB: Root tree structure (/, /Device, /Driver, /Types) created!\n\r");
    return STATUS_SUCCESS;

fatal:
    DPRINT("OB: FATAL: root structure failed, status=%x\n\r", Status);
    return Status;
}

#endif // Turn off legacy code (in case it still needed)

VEASTATUS
VEAPI
ObpCreateRootStructure(VOID)
{
    VEASTATUS Status;
    ANSI_STRING String;
    OBJECT_ATTRIBUTES Attributes;
    PLIST_ENTRY Curr;
    HANDLE RootHandle, DeviceHandle, DriverHandle, TypeHandle;
    
    if (!ObpDirectoryObjectType)
    {
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    /* Make "/" */
    RtlInitAnsiString(&String, "/");
    InitializeObjectAttributes(&Attributes, &String, OBJ_PERMANENT, NULL, NULL);

    Status = VeaCreateDirectoryNamespace(&RootHandle, 0x10000, &Attributes);
    if (!VEA_SUCCESS(Status))
        goto Failed;


    /* Make "/Device"  */
    RtlZeroMemory(&Attributes, sizeof(OBJECT_ATTRIBUTES));
    RtlInitAnsiString(&String, "/Device");
    InitializeObjectAttributes(&Attributes, &String, OBJ_PERMANENT, NULL, NULL);

    Status = VeaCreateDirectoryNamespace(&DeviceHandle, 0x10000, &Attributes);
    if (!VEA_SUCCESS(Status))
        goto Failed;
    

    /* Make "/Driver" */
    RtlZeroMemory(&Attributes, sizeof(OBJECT_ATTRIBUTES));
    RtlInitAnsiString(&String, "/Driver");
    InitializeObjectAttributes(&Attributes, &String, OBJ_PERMANENT, NULL, NULL);

    Status = VeaCreateDirectoryNamespace(&DriverHandle, 0x10000, &Attributes);
    if (!VEA_SUCCESS(Status))
        goto Failed;


    /* Make "/Types" */
    RtlZeroMemory(&Attributes, sizeof(OBJECT_ATTRIBUTES));
    RtlInitAnsiString(&String, "/Types");
    InitializeObjectAttributes(&Attributes, &String, OBJ_PERMANENT, NULL, NULL);

    Status = VeaCreateDirectoryNamespace(&TypeHandle, 0x10000, &Attributes);
    if (!VEA_SUCCESS(Status))
        goto Failed;


    /* Reference it incase some Legacy Code needed */

    // Root Handle
    Status = ObReferenceObjectByHandle(
        RootHandle,
        0x10000,
        ObpDirectoryObjectType,
        KernelMode,
        (PVOID*)&ObpRootDirectoryObject
    );
    if (!VEA_SUCCESS(Status)) goto Failed;

    VeaClose(RootHandle);

    // Device Handle
    Status = ObReferenceObjectByHandle(
        DeviceHandle,
        0x10000,
        ObpDirectoryObjectType,
        KernelMode,
        (PVOID*)&ObpDeviceDirectoryObject
    );
    if (!VEA_SUCCESS(Status)) goto Failed;

    VeaClose(DeviceHandle);

    // Driver Handle
    Status = ObReferenceObjectByHandle(
        DriverHandle,
        0x10000,
        ObpDirectoryObjectType,
        KernelMode,
        (PVOID*)&ObpDriverDirectoryObject
    );
    if (!VEA_SUCCESS(Status)) goto Failed;

    VeaClose(DriverHandle);

    // Type Handle
    Status = ObReferenceObjectByHandle(
        TypeHandle,
        0x10000,
        ObpDirectoryObjectType,
        KernelMode,
        (PVOID*)&ObpTypesDirectoryObject
    );
    if (!VEA_SUCCESS(Status)) goto Failed;

    VeaClose(TypeHandle);

    /* Insert every object */
    for (PLIST_ENTRY Curr = ObpTypeObjectList.Flink;
         Curr != &ObpTypeObjectList;
         Curr = Curr->Flink)
    {
        POBJECT_TYPE   TypeObj = CONTAINING_RECORD(Curr, OBJECT_TYPE, TypeList);
        POBJECT_HEADER Header  = OBJECT_TO_OBJECT_HEADER(TypeObj);

        if (!(Header->Flags & OB_FLAG_HAS_NAME_INFO))
            continue;

        POBJECT_HEADER_NAME_INFO NI = OBJECT_HEADER_TO_NAME_INFO(Header);
        if (!NI->Name.Buffer)
            continue;

        NI->Directory = (POBJECT_DIRECTORY)ObpTypesDirectoryObject;
        Status = ObpInsertDirectory(ObpTypesDirectoryObject, TypeObj);
        if (!VEA_SUCCESS(Status)) goto Failed;
    }

    return STATUS_SUCCESS;

Failed:
    DPRINT("VeaCreateDirectoryNamespace failed: 0x%x\n", Status);
    return Status;
}

VEASTATUS
VEAPI
ObCreateSymbolicLink(
    PVOID ParentDirectory,
    PCHAR LinkName,
    PCHAR TargetPath
)
{
    if (!ParentDirectory || !LinkName || !TargetPath ||
        !ObpSymbolicLinkObjectType)
    {
        return STATUS_INVALID_PARAMETER;
    }

    ANSI_STRING NameAnsi;
    RtlInitAnsiString(&NameAnsi, LinkName);
    if (NameAnsi.Length == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    OBJECT_ATTRIBUTES Attr;
    // FIX 1: Set OBJ_PERMANENT agar Symbolic Link bertahan di namespace directory
    InitializeObjectAttributes(&Attr, &NameAnsi, OBJ_PERMANENT, NULL, NULL);

    USHORT PathLength = 0;
    while (TargetPath[PathLength] != '\0') {
        if (PathLength == 0xFFFE)
        {
            return STATUS_NAME_TOO_LONG;
        }
        PathLength++;
    }

    PVOID SymlinkObject = NULL;
    VEASTATUS Status;

    Status = ObAllocateObject(ObpSymbolicLinkObjectType, &Attr, sizeof(OBJECT_SYMBOLIC_LINK), &SymlinkObject);
    if (!VEA_SUCCESS(Status)) {
        return (VEASTATUS)Status;
    }

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(SymlinkObject);
    Header->Flags |= OB_FLAG_PERMANENT; // Pastikan flag terpasang di Header

    POBJECT_SYMBOLIC_LINK SymlinkBody = (POBJECT_SYMBOLIC_LINK)SymlinkObject;
    SymlinkBody->LinkTarget.Length = PathLength;
    SymlinkBody->LinkTarget.MaximumLength = PathLength + 1;
    SymlinkBody->LinkTarget.Buffer = UlAllocatePoolWithTag(
        NonPagedPool,
        SymlinkBody->LinkTarget.MaximumLength,
        RTL_TAG
    );
    if (!SymlinkBody->LinkTarget.Buffer) {
        ObDereferenceObject(SymlinkObject);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    RtlCopyMemory(SymlinkBody->LinkTarget.Buffer, TargetPath, SymlinkBody->LinkTarget.MaximumLength);

    if (Header->Flags & OB_FLAG_HAS_NAME_INFO)
    {
        POBJECT_HEADER_NAME_INFO NameInfo = OBJECT_HEADER_TO_NAME_INFO(Header);
        NameInfo->Directory = ParentDirectory;
        NameInfo->Name.Length = NameAnsi.Length;
        NameInfo->Name.MaximumLength = NameAnsi.Length + 1;
        NameInfo->Name.Buffer = UlAllocatePoolWithTag(PagedPool, NameInfo->Name.MaximumLength, 'NmOb');
        if (!NameInfo->Name.Buffer)
        {
            ObDereferenceObject(SymlinkObject);
            return STATUS_INSUFFICIENT_MEMORY;
        }
        RtlCopyMemory(NameInfo->Name.Buffer, LinkName, NameAnsi.Length);
        NameInfo->Name.Buffer[NameAnsi.Length] = '\0';
    }
    else
    {
        ObDereferenceObject(SymlinkObject);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    Status = ObpInsertDirectory(ParentDirectory, SymlinkObject);
    if (!VEA_SUCCESS(Status))
    {
        ObDereferenceObject(SymlinkObject);
        return Status;
    }

    ObDereferenceObject(SymlinkObject);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObCreateSymbolicLinkToObject(
    PVOID ParentDirectory,
    PCHAR LinkName,
    PVOID TargetObject
)
{
    ANSI_STRING Name;
    OBJECT_ATTRIBUTES Attributes;
    POBJECT_SYMBOLIC_LINK LinkObject;
    POBJECT_HEADER Header;
    POBJECT_HEADER_NAME_INFO NameInfo;
    VEASTATUS Status;

    if (!ParentDirectory || !LinkName || !TargetObject ||
        !ObpSymbolicLinkObjectType)
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlInitAnsiString(&Name, LinkName);
    if (Name.Length == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // FIX 1: Set OBJ_PERMANENT
    InitializeObjectAttributes(&Attributes, &Name, OBJ_PERMANENT, NULL, NULL);
    Status = ObAllocateObject(ObpSymbolicLinkObjectType,
                              &Attributes,
                              sizeof(OBJECT_SYMBOLIC_LINK),
                              (PVOID *)&LinkObject);
    if (!VEA_SUCCESS(Status))
    {
        return (VEASTATUS)Status;
    }

    Header = OBJECT_TO_OBJECT_HEADER(LinkObject);
    Header->Flags |= OB_FLAG_PERMANENT; // Pastikan flag permanent aktif di Header

    if (!(Header->Flags & OB_FLAG_HAS_NAME_INFO))
    {
        ObDereferenceObject(LinkObject);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    NameInfo = OBJECT_HEADER_TO_NAME_INFO(Header);
    NameInfo->Directory = ParentDirectory;
    NameInfo->Name.Length = Name.Length;
    NameInfo->Name.MaximumLength = Name.Length + 1;
    NameInfo->Name.Buffer = UlAllocatePoolWithTag(
        PagedPool,
        NameInfo->Name.MaximumLength,
        'NmOb'
    );
    if (!NameInfo->Name.Buffer)
    {
        ObDereferenceObject(LinkObject);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    RtlCopyMemory(NameInfo->Name.Buffer, LinkName, Name.Length);
    NameInfo->Name.Buffer[Name.Length] = '\0';
    LinkObject->TargetObject = TargetObject;
    ObReferenceObject(TargetObject);

    Status = ObpInsertDirectory(ParentDirectory, LinkObject);
    ObDereferenceObject(LinkObject);
    return Status;
}

VEASTATUS
VEAPI
ObDeleteSymbolicLink(
    PVOID ParentDirectory,
    PCHAR LinkName
)
{
    POBJECT_DIRECTORY Directory;
    ANSI_STRING Name;
    PLIST_ENTRY Current;

    if (!ParentDirectory || !LinkName)
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlInitAnsiString(&Name, LinkName);
    if (Name.Length == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Directory = (POBJECT_DIRECTORY)ParentDirectory;
    Current = Directory->Head.Flink;
    while (Current != &Directory->Head && Current != NULL)
    {
        POBJECT_DIRECTORY_ENTRY Entry;
        POBJECT_HEADER Header;
        POBJECT_HEADER_NAME_INFO NameInfo;
        BOOLEAN Match;

        Entry = CONTAINING_RECORD(Current, OBJECT_DIRECTORY_ENTRY, Chain);
        Current = Current->Flink;
        Header = OBJECT_TO_OBJECT_HEADER(Entry->Object);

        if (!(Header->Flags & OB_FLAG_HAS_NAME_INFO))
        {
            continue;
        }

        NameInfo = OBJECT_HEADER_TO_NAME_INFO(Header);
        if (!NameInfo->Name.Buffer || NameInfo->Name.Length != Name.Length)
        {
            continue;
        }

        Match = TRUE;
        for (USHORT i = 0; i < Name.Length; i++)
        {
            CHAR ExistingCharacter = NameInfo->Name.Buffer[i];
            CHAR RequestedCharacter = Name.Buffer[i];

            if (ExistingCharacter >= 'A' && ExistingCharacter <= 'Z')
            {
                ExistingCharacter += 'a' - 'A';
            }
            if (RequestedCharacter >= 'A' && RequestedCharacter <= 'Z')
            {
                RequestedCharacter += 'a' - 'A';
            }

            if (ExistingCharacter != RequestedCharacter)
            {
                Match = FALSE;
                break;
            }
        }

        if (Match)
        {
            PVOID Object = Entry->Object;

            if (Header->Type != ObpSymbolicLinkObjectType)
            {
                return STATUS_OBJECT_TYPE_MISMATCH;
            }

            // FIX 2: Manfaatkan ObMakeTemporaryObject!
            // Fungsi ini akan menghapus flag OBJ_PERMANENT, mencabut node dari Directory, 
            // melepaskan Directory Reference, dan membebas memori secara aman.
            ObMakeTemporaryObject(Object);
            return STATUS_SUCCESS;
        }
    }

    return STATUS_OBJECT_NAME_NOT_FOUND;
}

VOID
VEAPI
ObpDeleteSymbolicLink(
    PVOID Object
)
{
    POBJECT_SYMBOLIC_LINK SymLink;

    if (!Object)
    {
        return;
    }

    SymLink = (POBJECT_SYMBOLIC_LINK)Object;
    if (SymLink->LinkTarget.Buffer != NULL)
    {
        UlFreePoolWithTag(SymLink->LinkTarget.Buffer, RTL_TAG);
        SymLink->LinkTarget.Buffer = NULL;
    }

    if (SymLink->TargetObject != NULL)
    {
        ObDereferenceObject(SymLink->TargetObject);
        SymLink->TargetObject = NULL;
    }
}

VEASTATUS
VEAPI
ObpParseSymbolicLink(
    PVOID ParseObject,
    POB_LOOKUP_CONTEXT LookupContext,
    PVOID Context,
    PVOID *ResolvedObject
)
{
    if (!ParseObject || !LookupContext || !ResolvedObject) {
        return STATUS_INVALID_PARAMETER;
    }

    POBJECT_SYMBOLIC_LINK SymLink = (POBJECT_SYMBOLIC_LINK)ParseObject;
    if (SymLink->TargetObject)
    {
        *ResolvedObject = SymLink->TargetObject;
        return STATUS_SUCCESS;
    }

    ULONG TempLen = 0;
    ULONG MaxLen = sizeof(LookupContext->ReparseBuffer) - 1;

    // 1. Salin string target dari Symlink (LinkTarget) ke ReparseBuffer
    USHORT LinkLen = SymLink->LinkTarget.Length;
    PCHAR LinkBuf = SymLink->LinkTarget.Buffer;

    if (LinkBuf != NULL) {
        for (USHORT i = 0; i < LinkLen && TempLen < MaxLen; i++) {
            LookupContext->ReparseBuffer[TempLen++] = LinkBuf[i];
        }
    }

    // 2. Sambungkan dengan sisa string path (RemainingName) yang belum di-parse
    USHORT RemLen = LookupContext->RemainingName.Length;
    PCHAR RemBuf = LookupContext->RemainingName.Buffer;

    if (RemBuf != NULL) {
        for (USHORT i = 0; i < RemLen && TempLen < MaxLen; i++) {
            LookupContext->ReparseBuffer[TempLen++] = RemBuf[i];
        }
    }

    LookupContext->ReparseBuffer[TempLen] = '\0';

    // 3. Arahkan RemainingName ke ReparseBuffer internal di dalam LookupContext
    LookupContext->RemainingName.Buffer = LookupContext->ReparseBuffer;
    LookupContext->RemainingName.Length = (USHORT)TempLen;
    LookupContext->RemainingName.MaximumLength = sizeof(LookupContext->ReparseBuffer);

    // 4. Kembalikan sinyal REPARSE agar ObpLookupObjectName mereset loop-nya
    return STATUS_REPARSE;
}

/* Dump function */
VOID
VEAPI
ObpDumpObjectTreeRecursive(
    PVOID DirectoryObject,
    ULONG Depth
)
{
    if (!DirectoryObject) return;

    POBJECT_DIRECTORY Dir = (POBJECT_DIRECTORY)DirectoryObject;
    PLIST_ENTRY Current = Dir->Head.Flink;

    while (Current != &Dir->Head) {
        POBJECT_DIRECTORY_ENTRY Entry = CONTAINING_RECORD(Current, OBJECT_DIRECTORY_ENTRY, Chain);
        
        ULONG_PTR ObjectAddr = (ULONG_PTR)Entry->Object;
        if (ObjectAddr < 0xC0000000) {
            Current = Current->Flink;
            continue;
        }

        POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Entry->Object);
        PCHAR TypeName = "UNKNOWN";
        PCHAR ObjectName = "UNNAMED_OBJECT"; // Default jika tidak punya nama

        if (Header && Header->Type && ((ULONG_PTR)Header->Type >= 0xC0000000)) {
            TypeName = Header->Type->TypeName;
        }
        
        if (Header && (Header->Flags & OB_FLAG_HAS_NAME_INFO)) {
            POBJECT_HEADER_NAME_INFO NameInfo = OBJECT_HEADER_TO_NAME_INFO(Header);
            if (NameInfo->Name.Buffer) {
                ObjectName = NameInfo->Name.Buffer;
            }
        }

        for (ULONG i = 0; i < Depth; i++) {
            kdp_print("    ");
        }

        KdPrintf("|--/ %s [%s] [Reference: %u]\n\r", ObjectName, TypeName, Header->ReferenceCount);

        if (Header && Header->Type == ObpDirectoryObjectType) {
            ObpDumpObjectTreeRecursive(Entry->Object, Depth + 1);
        }

        Current = Current->Flink;
    }
}

VOID
VEAPI
ObDumpObjectTree(VOID)
{
    kdp_print("\n\r=== VeaOS Object Tree Dump ===\n\r");
    kdp_print("/ [Directory]\n\r");
    
    // Mulai rekursi dari root direktori dengan kedalaman 0
    if (ObpRootDirectoryObject) {
        ObpDumpObjectTreeRecursive(ObpRootDirectoryObject, 0);
    }
    
    kdp_print("==============================\n\r\n\r");
}

/* Parsing */
BOOLEAN
VEAPI
ObpGetNextPathToken(
    POB_LOOKUP_CONTEXT Context,
    PCHAR *TokenStart,
    ULONG *TokenLength
)
{
    if (!Context || !TokenStart || !TokenLength) {
        return FALSE;
    }

    PCHAR Path = Context->RemainingName.Buffer;
    USHORT Length = Context->RemainingName.Length;

    // Lewati semua pemisah path '/' atau '\' di awal
    while (Length > 0 && (*Path == '/' || *Path == '\\')) {
        Path++;
        Length--;
    }

    // Jika string sudah habis
    if (Length == 0 || *Path == '\0') {
        Context->RemainingName.Buffer = Path;
        Context->RemainingName.Length = 0;
        return FALSE;
    }

    // Tandai posisi awal token
    *TokenStart = Path;
    ULONG LocalTokenLen = 0;

    // Jalan terus sampai ketemu '/' atau '\' berikutnya atau batas Length
    while (Length > 0 && *Path != '/' && *Path != '\\' && *Path != '\0') {
        LocalTokenLen++;
        Path++;
        Length--;
    }

    *TokenLength = LocalTokenLen;

    // Update RemainingName di dalam Context (geser pointer & kurangi length)
    Context->RemainingName.Buffer = Path;
    Context->RemainingName.Length = Length;

    return TRUE; // Masih ada token yang berhasil dibaca
}

BOOLEAN
VEAPI
ObpMatchToken(
    PCHAR TokenStart,
    ULONG TokenLength,
    PCHAR EntryName,
    BOOLEAN CaseInsensitive
)
{
    ULONG i;
    
    for(i = 0; i < TokenLength; i++)
    {
        CHAR c1 = EntryName[i];
        CHAR c2 = TokenStart[i];

        if (CaseInsensitive) {
            if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        }

        if (EntryName[i] == '\0' || c1 != c2) {
            return FALSE;
        }
    }
    return (EntryName[i] == '\0');
}

PVOID
VEAPI
ObpLookupEntryDirectory(
    POBJECT_DIRECTORY Directory,
    PCHAR TokenStart,
    ULONG TokenLength,
    BOOLEAN CaseInsensitive,
    POB_LOOKUP_CONTEXT Context // Dimasukin buat akses LockObject nantinya
)
{   // NANTI DISINI: ObpAcquireLookupContextLock(Context, Directory);

    PLIST_ENTRY CurrentEntry = Directory->Head.Flink;
    PVOID FoundObject = NULL;

    while(CurrentEntry != &Directory->Head)
    {
        POBJECT_DIRECTORY_ENTRY Entry = CONTAINING_RECORD(CurrentEntry, OBJECT_DIRECTORY_ENTRY, Chain);
        POBJECT_HEADER EntryHeader = OBJECT_TO_OBJECT_HEADER(Entry->Object);

        if (EntryHeader->Flags & OB_FLAG_HAS_NAME_INFO) {
            POBJECT_HEADER_NAME_INFO NameInfo = OBJECT_HEADER_TO_NAME_INFO(EntryHeader);
            
            if (!NameInfo->Name.Buffer)
            {
                DPRINT("OB: BAD dir entry obj=%p type=%s ref=%d dir=%p\n",
                        Entry->Object,
                        EntryHeader->Type ? EntryHeader->Type->TypeName : "?",
                        EntryHeader->ReferenceCount,
                        NameInfo->Directory);
                CurrentEntry = CurrentEntry->Flink;
                continue;
            }

            if(ObpMatchToken(TokenStart, TokenLength, NameInfo->Name.Buffer, CaseInsensitive))
            {
                FoundObject = Entry->Object;
                break;
            }
        }
        CurrentEntry = CurrentEntry->Flink;
    }

    // NANTI DISINI: ObpReleaseLookupContextLock(Context);

    return FoundObject;
}

VEASTATUS
VEAPI
ObpLookupObjectName(
    POBJECT_ATTRIBUTES ObjectAttributes,
    POB_LOOKUP_CONTEXT LookupContext,
    PVOID *FoundObject
)
{
    PVOID          RootObject = NULL;
    POBJECT_HEADER FinalHeader;
    VEASTATUS      Status;

    if (!ObjectAttributes || !ObjectAttributes->ObjectName ||
        !LookupContext || !FoundObject) {
        return STATUS_INVALID_PARAMETER;
    }

    *FoundObject = NULL;

    LookupContext->RootDirectory   = ObjectAttributes->RootDirectory;   /* HANDLE */
    LookupContext->RootObject      = NULL;
    LookupContext->CaseInsensitive = (ObjectAttributes->Attributes & OBJ_CASE_INSENSITIVE) != 0;
    LookupContext->ReparseCount    = 0;

    if (ObjectAttributes->RootDirectory)
    {
        Status = ObReferenceObjectByHandle(ObjectAttributes->RootDirectory, 0, NULL,
                                           LookupContext->AccessMode, &RootObject);
        if (!VEA_SUCCESS(Status))
            return Status;                    /* belum ada yang perlu dilepas */

        LookupContext->RootObject       = RootObject;
        LookupContext->CurrentDirectory = RootObject;          /* <-- yang hilang */
    }
    else
    {
        LookupContext->CurrentDirectory = ObpRootDirectoryObject;
    }

    LookupContext->RemainingName = *ObjectAttributes->ObjectName;

    /* Loop utama Object Manager */
    while (TRUE)
    {
        POBJECT_HEADER CurrentHeader = OBJECT_TO_OBJECT_HEADER(LookupContext->CurrentDirectory);

        if (CurrentHeader->Type->ParseRoutine != NULL)
        {
            PVOID ResolvedObject = NULL;

            VEASTATUS ParseStatus = CurrentHeader->Type->ParseRoutine(
                LookupContext->CurrentDirectory,
                LookupContext,
                LookupContext->ParseContext,
                &ResolvedObject
            );

            if (ParseStatus == STATUS_SUCCESS)
            {
                if (ResolvedObject == NULL)
                {
                    Status = LookupContext->InsertMode ? STATUS_SUCCESS
                                                       : STATUS_OBJECT_NAME_NOT_FOUND;
                    goto Exit;                /* *FoundObject sudah NULL */
                }

                if (CurrentHeader->Type == ObpSymbolicLinkObjectType)
                {
                    while (LookupContext->RemainingName.Buffer != NULL &&
                           LookupContext->RemainingName.Length > 0 &&
                           (*LookupContext->RemainingName.Buffer == '/' ||
                            *LookupContext->RemainingName.Buffer == '\\'))
                    {
                        LookupContext->RemainingName.Buffer++;
                        LookupContext->RemainingName.Length--;
                    }

                    if (++LookupContext->ReparseCount > 32)
                    {
                        DPRINT("Object symbolic-link chain exceeded the reparse limit\n");
                        Status = STATUS_REPARSE_OBJECT_MAY_LOOP;
                        goto Exit;
                    }

                    LookupContext->CurrentDirectory = ResolvedObject;
                    continue;
                }

                if (LookupContext->ExpectedType != NULL &&
                    OBJECT_TO_OBJECT_HEADER(ResolvedObject)->Type != LookupContext->ExpectedType)
                {
                    DPRINT("[OB] Mismatch in ExpectedType: got %s, expected %s\n",
                           OBJECT_TO_OBJECT_HEADER(ResolvedObject)->Type->TypeName,
                           LookupContext->ExpectedType->TypeName);
                    Status = STATUS_OBJECT_TYPE_MISMATCH;
                    goto Exit;
                }

                ObReferenceObject(ResolvedObject);
                *FoundObject = ResolvedObject;
                Status = STATUS_SUCCESS;
                goto Exit;
            }
            else if (ParseStatus == STATUS_REPARSE)
            {
                if (++LookupContext->ReparseCount > 32)
                {
                    Status = STATUS_REPARSE_OBJECT_MAY_LOOP;
                    goto Exit;
                }

                if (LookupContext->ReparseBuffer[0] == '/' ||
                    LookupContext->ReparseBuffer[0] == '\\')
                {
                    LookupContext->CurrentDirectory = ObpRootDirectoryObject;
                }
                else if (LookupContext->RootObject != NULL)
                {
                    LookupContext->CurrentDirectory = LookupContext->RootObject;
                }

                continue;
            }
            else
            {
                Status = ParseStatus;
                goto Exit;
            }
        }

        if (CurrentHeader->Type != ObpDirectoryObjectType)
        {
            DPRINT("Object path has remaining components but current object type is %s\n",
                   CurrentHeader->Type ? CurrentHeader->Type->TypeName : "<null>");
            Status = STATUS_OBJECT_TYPE_MISMATCH;
            goto Exit;
        }

        PCHAR TokenStart = NULL;
        ULONG TokenLength = 0;

        if (!ObpGetNextPathToken(LookupContext, &TokenStart, &TokenLength))
        {
            break;      /* path habis: hasilnya CurrentDirectory sendiri */
        }

        PVOID NextObject = ObpLookupEntryDirectory(
            (POBJECT_DIRECTORY)LookupContext->CurrentDirectory,
            TokenStart,
            TokenLength,
            LookupContext->CaseInsensitive,
            LookupContext
        );

        if (!NextObject)
        {
            Status = (LookupContext->InsertMode && LookupContext->RemainingName.Length == 0)
                     ? STATUS_SUCCESS : STATUS_OBJECT_NAME_NOT_FOUND;
            goto Exit;  /* *FoundObject masih NULL */
        }

        LookupContext->CurrentDirectory = NextObject;

        while (LookupContext->RemainingName.Length > 0 &&
               (LookupContext->RemainingName.Buffer[0] == '/' ||
                LookupContext->RemainingName.Buffer[0] == '\\'))
        {
            LookupContext->RemainingName.Buffer++;
            LookupContext->RemainingName.Length--;
        }

        if (LookupContext->RemainingName.Length == 0 &&
            OBJECT_TO_OBJECT_HEADER(NextObject)->Type->ParseRoutine == NULL)
        {
            break;
        }
    }

    /* Hasil akhir = CurrentDirectory */
    FinalHeader = OBJECT_TO_OBJECT_HEADER(LookupContext->CurrentDirectory);
    if (LookupContext->ExpectedType != NULL && FinalHeader->Type != LookupContext->ExpectedType)
    {
        DPRINT("Object path resolved to type %s, expected %s\n",
               FinalHeader->Type ? FinalHeader->Type->TypeName : "<null>",
               LookupContext->ExpectedType->TypeName);
        Status = STATUS_OBJECT_TYPE_MISMATCH;
        goto Exit;
    }

    ObReferenceObject(LookupContext->CurrentDirectory);   /* ref hasil DULU */
    *FoundObject = LookupContext->CurrentDirectory;
    Status = STATUS_SUCCESS;

Exit:
    if (RootObject)
        ObDereferenceObject(RootObject);                  /* lepas ref root TERAKHIR */
    return Status;
}

VOID
VEAPI
ObpDeleteNameCheck(IN PVOID Object)
{
    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Object);
    POBJECT_HEADER_NAME_INFO NameInfo;

    if (Header->Flags & OB_FLAG_PERMANENT) {
        return;
    }

    if (Header->Flags & OB_FLAG_HAS_NAME_INFO)
    {
        NameInfo = OBJECT_HEADER_TO_NAME_INFO(Header);

        if (NameInfo->Directory != NULL)
        {
            ObpRemoveEntryDirectory(Object);
        }
    }
}

VOID
VEAPI
ObMakeTemporaryObject(IN PVOID Object)
{
    POBJECT_HEADER Header;

    if (!Object) return;

    Header = OBJECT_TO_OBJECT_HEADER(Object);
    Header->Flags &= ~OB_FLAG_PERMANENT;

    ObpDeleteNameCheck(Object);
}

VEASTATUS
VEAPI
VeaCreateDirectoryNamespace(
    OUT PHANDLE Handle,
    IN ACCESS_MASK Mask,
    IN POBJECT_ATTRIBUTES ObjectAttributes)
{
    VEASTATUS Status;
    PVOID ReturnedObject;
    HANDLE PointerHandle;

    Status = ObCreateObject(
        KernelMode,
        ObpDirectoryObjectType,
        ObjectAttributes,
        KernelMode,
        NULL,
        0,
        0,
        0,
        &ReturnedObject
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("VeaCreateDirectoryNamespace ObCreateObject failed: 0x%x\n");
        return Status;
    }

    // Pakai 6 parameter versi baru
    Status = ObInsertObject(
        ReturnedObject,
        NULL,   // AccessState
        Mask,      // DesiredAccess
        0,      // ObjectPointerBias
        NULL,   // NewObject
        &PointerHandle
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("VeaCreateDirectoryNamespace ObInsertObject failed: 0x%x\n", Status);
        return Status;
    }

    if (Handle)
    {
        *Handle = PointerHandle;
    }
    else
    {
        VeaClose(PointerHandle);
    }

    return STATUS_SUCCESS;
}