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

VOID 
VEAPI
ObpCreateRootStructure(VOID)
{
    OBJECT_ATTRIBUTES Attr;
    ANSI_STRING NameAnsi;
    
    // 1. Bikin Root Object "/" (ObjectSize = 0)
    ObAllocateObject(ObpDirectoryObjectType, NULL, 0, &ObpRootDirectoryObject);
    InitializeListHead(&((POBJECT_DIRECTORY)ObpRootDirectoryObject)->Head);
    
    // 2. Bikin sub-direktori "/Device"
    RtlInitAnsiString(&NameAnsi, "Device");
    InitializeObjectAttributes(&Attr, &NameAnsi, OBJ_CASE_INSENSITIVE, ObpRootDirectoryObject, NULL);

    ObAllocateObject(ObpDirectoryObjectType, &Attr, 0, &ObpDeviceDirectoryObject);
    InitializeListHead(&((POBJECT_DIRECTORY)ObpDeviceDirectoryObject)->Head);
    ObInsertObject(ObpDeviceDirectoryObject, &Attr);
    
    // 3. Bikin sub-direktori "/Driver"
    RtlInitAnsiString(&NameAnsi, "Driver");
    InitializeObjectAttributes(&Attr, &NameAnsi, OBJ_CASE_INSENSITIVE, ObpRootDirectoryObject, NULL);

    ObAllocateObject(ObpDirectoryObjectType, &Attr, 0, &ObpDriverDirectoryObject);
    InitializeListHead(&((POBJECT_DIRECTORY)ObpDriverDirectoryObject)->Head);
    ObInsertObject(ObpDriverDirectoryObject, &Attr);

    // 4. Bikin sub-direktori "/Types"
    RtlInitAnsiString(&NameAnsi, "Types");
    InitializeObjectAttributes(&Attr, &NameAnsi, OBJ_CASE_INSENSITIVE, ObpRootDirectoryObject, NULL);

    ObAllocateObject(ObpDirectoryObjectType, &Attr, 0, &ObpTypesDirectoryObject);
    InitializeListHead(&((POBJECT_DIRECTORY)ObpTypesDirectoryObject)->Head);
    ObInsertObject(ObpTypesDirectoryObject, &Attr);

    // Insert semua registered OBJECT_TYPE ke /Types directory
    PLIST_ENTRY Curr = ObpTypeObjectList.Flink;
    while (Curr != &ObpTypeObjectList) 
    {
        POBJECT_TYPE TypeObj = CONTAINING_RECORD(Curr, OBJECT_TYPE, TypeList);
        POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(TypeObj);

        // Hanya masukkan objek yang memiliki NameInfo
        if (Header->Flags & OB_FLAG_HAS_NAME_INFO) 
        {
            POBJECT_HEADER_NAME_INFO NameInfo = OBJECT_HEADER_TO_NAME_INFO(Header);
            NameInfo->Directory = ObpTypesDirectoryObject;
            
            // Masukkan ke folder /Types
            ObpInsertDirectory(ObpTypesDirectoryObject, TypeObj);
        }

        Curr = Curr->Flink;
    }
    
    kdp_print("OB: Root tree structure (/, /Device, /Driver, /Types) created!\n\r");
}

VEASTATUS
VEAPI
ObCreateSymbolicLink(
    PVOID ParentDirectory,
    PCHAR LinkName,
    PCHAR TargetPath
)
{
    if (!ParentDirectory || !LinkName || !TargetPath) {
        return STATUS_INVALID_PARAMETER;
    }

    ANSI_STRING NameAnsi;
    RtlInitAnsiString(&NameAnsi, LinkName);

    OBJECT_ATTRIBUTES Attr;
    InitializeObjectAttributes(&Attr, &NameAnsi, 0, NULL, NULL);

    // Hitung panjang string TargetPath secara manual
    USHORT PathLength = 0;
    while (TargetPath[PathLength] != '\0') {
        PathLength++;
    }

    PVOID SymlinkObject = NULL;
    LONG Status;

    // Alokasi Objek Symlink
    Status = ObAllocateObject(ObpSymbolicLinkObjectType, &Attr, sizeof(OBJECT_SYMBOLIC_LINK), &SymlinkObject);
    if (Status != 0) {
        return Status;
    }

    POBJECT_SYMBOLIC_LINK SymlinkBody = (POBJECT_SYMBOLIC_LINK)SymlinkObject;

    // Atur properti ANSI_STRING
    SymlinkBody->LinkTarget.Length = PathLength;
    SymlinkBody->LinkTarget.MaximumLength = PathLength + 1; // +1 untuk '\0'

    // Alokasi buffer memori terpisah untuk menampung teks string-nya
    SymlinkBody->LinkTarget.Buffer = UlAllocatePoolWithTag(
        NonPagedPool, 
        SymlinkBody->LinkTarget.MaximumLength, 
        RTL_TAG // Tag buat String Buffer
    );

    if (!SymlinkBody->LinkTarget.Buffer) {
        // Jika alokasi buffer teks gagal, lepas kembali objek symlink yang tadi dibuat
        // Lu bisa panggil fungsi free pool untuk membuang (SymlinkObject - 1)
        return -1;
    }

    // Copy data string asli ke buffer baru memakai fungsi lu
    RtlCopyMemory(SymlinkBody->LinkTarget.Buffer, TargetPath, SymlinkBody->LinkTarget.MaximumLength);

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(SymlinkObject);
    if (Header->Flags & OB_FLAG_HAS_NAME_INFO) 
    {
        POBJECT_HEADER_NAME_INFO NameInfo = OBJECT_HEADER_TO_NAME_INFO(Header);
        NameInfo->Directory = ParentDirectory;
        NameInfo->Name.Length = NameAnsi.Length;
        NameInfo->Name.MaximumLength = NameAnsi.Length + 1;
        NameInfo->Name.Buffer = UlAllocatePoolWithTag(PagedPool, NameInfo->Name.MaximumLength, 'NmOb');
        RtlCopyRawString(NameInfo->Name.Buffer, LinkName);
    }

    return ObpInsertDirectory(ParentDirectory, SymlinkObject);
}

VOID
VEAPI
ObpDeleteSymbolicLink(
    PVOID Object
)
{
    POBJECT_SYMBOLIC_LINK SymLink = (POBJECT_SYMBOLIC_LINK)Object;
    if (SymLink->LinkTarget.Buffer != NULL) {
        UlFreePoolWithTag(SymLink->LinkTarget.Buffer, RTL_TAG);
    }
}

VEASTATUS
VEAPI
ObpParseSymbolicLink(
    PVOID ParseObject,
    POB_LOOKUP_CONTEXT LookupContext,
    PVOID *ResolvedObject
)
{
    if (!ParseObject || !LookupContext) {
        return STATUS_INVALID_PARAMETER;
    }

    // Parameter ini tidak dipakai langsung dalam proses reparse
    (VOID)ResolvedObject;

    POBJECT_SYMBOLIC_LINK SymLink = (POBJECT_SYMBOLIC_LINK)ParseObject;
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

        KdPrintf("|--/ %s [%s]\n\r", ObjectName, TypeName);

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
            
            if(ObpMatchToken(TokenStart, TokenLength, NameInfo->Name.Buffer, CaseInsensitive))
            {
                FoundObject = Entry->Object;
                break; // Ketemu!
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
    if (!ObjectAttributes || !ObjectAttributes->ObjectName || !FoundObject) {
        return STATUS_INVALID_PARAMETER;
    }

    LookupContext->RootDirectory = ObjectAttributes->RootDirectory;
    LookupContext->CaseInsensitive = (ObjectAttributes->Attributes & OBJ_CASE_INSENSITIVE) != 0;
    LookupContext->ReparseCount = 0;

    if (ObjectAttributes->RootDirectory) {
        LookupContext->CurrentDirectory = ObjectAttributes->RootDirectory;
    } else {
        LookupContext->CurrentDirectory = ObpRootDirectoryObject;
    }

    LookupContext->RemainingName = *ObjectAttributes->ObjectName;
    
    PCHAR TokenStart = NULL;
    ULONG TokenLength = 0;

    while(ObpGetNextPathToken(LookupContext, &TokenStart, &TokenLength))
    {
        POBJECT_HEADER CurrentHeader = OBJECT_TO_OBJECT_HEADER(LookupContext->CurrentDirectory);

        if(CurrentHeader->Type != ObpDirectoryObjectType)
        {
            return STATUS_OBJECT_TYPE_MISMATCH;
        }

        PVOID NextObject = ObpLookupEntryDirectory(
            (POBJECT_DIRECTORY)LookupContext->CurrentDirectory,
            TokenStart,
            TokenLength,
            LookupContext->CaseInsensitive,
            LookupContext
        );

        if (!NextObject) {
            // Kalau ini token terakhir dan kita ada di InsertMode,
            // return sukses (tapi FoundObject = NULL) supaya ObInsertObject bisa langsung tanam di CurrentDirectory!
            if (LookupContext->InsertMode && LookupContext->RemainingName.Length == 0) {
                *FoundObject = NULL; 
                return STATUS_SUCCESS; 
            }
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }
        
        POBJECT_HEADER NextHeader = OBJECT_TO_OBJECT_HEADER(NextObject);

        if(NextHeader->Type->ParseRoutine != NULL)
        {
            PVOID ResolvedObject = NULL;

            VEASTATUS ParseStatus = NextHeader->Type->ParseRoutine(
                NextObject, 
                LookupContext, 
                &ResolvedObject
            );

            if (ParseStatus == STATUS_SUCCESS) 
            { 
                if (LookupContext->ExpectedType != NULL)
                {
                    POBJECT_HEADER ResolvedHeader = OBJECT_TO_OBJECT_HEADER(ResolvedObject);
                    if (ResolvedHeader->Type != LookupContext->ExpectedType)
                    {
                        return STATUS_OBJECT_TYPE_MISMATCH;
                    }
                }

                *FoundObject = ResolvedObject;
                return STATUS_SUCCESS;
            } 
            else if (ParseStatus == STATUS_REPARSE)
            {
                LookupContext->ReparseCount++;

                if (LookupContext->ReparseCount > 32)
                {
                    return STATUS_REPARSE_OBJECT_MAY_LOOP;
                }

                // Copy sisa path ke ReparseBuffer milik struct LookupContext (bukan stack lokal!)
                USHORT CopyLen = LookupContext->RemainingName.Length;
                if (CopyLen >= sizeof(LookupContext->ReparseBuffer)) {
                    CopyLen = sizeof(LookupContext->ReparseBuffer) - 1;
                }

                if (LookupContext->RemainingName.Buffer != NULL) {
                    for (ULONG i = 0; i < CopyLen; i++) {
                        LookupContext->ReparseBuffer[i] = LookupContext->RemainingName.Buffer[i];
                    }
                }
                LookupContext->ReparseBuffer[CopyLen] = '\0';

                // Arahkan RemainingName ke ReparseBuffer internal
                LookupContext->RemainingName.Buffer = LookupContext->ReparseBuffer;
                LookupContext->RemainingName.Length = CopyLen;
                LookupContext->RemainingName.MaximumLength = sizeof(LookupContext->ReparseBuffer);

                // Reset directory jika path bersifat Absolute
                if (LookupContext->ReparseBuffer[0] == '/' || LookupContext->ReparseBuffer[0] == '\\')
                {
                    LookupContext->CurrentDirectory = ObpRootDirectoryObject;
                }
                else if (LookupContext->RootDirectory != NULL)
                {
                    LookupContext->CurrentDirectory = LookupContext->RootDirectory;
                }

                continue;
            }
            else 
            {
                // ParseRoutine gagal (misal file gak ketemu di disk)
                return ParseStatus; 
            }
        }

        LookupContext->CurrentDirectory = NextObject;
    }

    POBJECT_HEADER FinalHeader = OBJECT_TO_OBJECT_HEADER(LookupContext->CurrentDirectory);
    if (LookupContext->ExpectedType != NULL && FinalHeader->Type != LookupContext->ExpectedType)
    {
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    ObReferenceObject(LookupContext->CurrentDirectory);

    *FoundObject = LookupContext->CurrentDirectory;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VeaCreateDirectoryNamespace(POBJECT_ATTRIBUTES ObjectAttributes)
{
    VEASTATUS Status;
    PVOID ReturnedObject;

    Status = ObCreateObject(
        KernelMode,
        ObpDirectoryObjectType,
        ObjectAttributes,
        KernelMode,
        NULL,
        0, // 0 = Fallback ke ObpDirectoryObjectType->ObjectSize (sizeof(OBJECT_DIRECTORY))
        0,
        0,
        &ReturnedObject
    );

    if(!VEA_SUCCESS(Status))
    {
        return Status;
    }

    Status = ObInsertObject(ReturnedObject, ObjectAttributes);
    if(!VEA_SUCCESS(Status))
    {
        return Status;
    }

    return STATUS_SUCCESS;
}