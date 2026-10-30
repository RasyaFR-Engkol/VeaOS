/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : section.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Mm Section Handling
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 29-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file section.c
 * --------------------------------------------------------------------- */

POBJECT_TYPE MmSectionObjectType;
CHAR MmUserProtectionToMask1[16] =
{
    0,
    MM_NOACCESS,
    MM_READONLY,
    (CHAR)MM_INVALID_PROTECTION,
    MM_READWRITE,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION,
    MM_WRITECOPY,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION
};

CHAR MmUserProtectionToMask2[16] =
{
    0,
    MM_EXECUTE,
    MM_EXECUTE_READ,
    (CHAR)MM_INVALID_PROTECTION,
    MM_EXECUTE_READWRITE,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION,
    MM_EXECUTE_WRITECOPY,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION,
    (CHAR)MM_INVALID_PROTECTION
};

ULONG MmCompatibleProtectionMask[8] =
{
    PAGE_NOACCESS,

    PAGE_NOACCESS | PAGE_READONLY | PAGE_WRITECOPY,

    PAGE_NOACCESS | PAGE_EXECUTE,

    PAGE_NOACCESS | PAGE_READONLY | PAGE_WRITECOPY | PAGE_EXECUTE |
    PAGE_EXECUTE_READ,

    PAGE_NOACCESS | PAGE_READONLY | PAGE_WRITECOPY | PAGE_READWRITE,

    PAGE_NOACCESS | PAGE_READONLY | PAGE_WRITECOPY,

    PAGE_NOACCESS | PAGE_READONLY | PAGE_WRITECOPY | PAGE_READWRITE |
    PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
    PAGE_EXECUTE_WRITECOPY,

    PAGE_NOACCESS | PAGE_READONLY | PAGE_WRITECOPY | PAGE_EXECUTE |
    PAGE_EXECUTE_READ | PAGE_EXECUTE_WRITECOPY
};

ACCESS_MASK MmMakeSectionAccess[8] =
{
    SECTION_MAP_READ,
    SECTION_MAP_READ,
    SECTION_MAP_EXECUTE,
    SECTION_MAP_EXECUTE | SECTION_MAP_READ,
    SECTION_MAP_WRITE,
    SECTION_MAP_READ,
    SECTION_MAP_EXECUTE | SECTION_MAP_WRITE,
    SECTION_MAP_EXECUTE | SECTION_MAP_READ
};

ACCESS_MASK MmMakeFileAccess[8] =
{
    FILE_READ_DATA,
    FILE_READ_DATA,
    FILE_EXECUTE,
    FILE_EXECUTE | FILE_READ_DATA,
    FILE_WRITE_DATA | FILE_READ_DATA,
    FILE_READ_DATA,
    FILE_EXECUTE | FILE_WRITE_DATA | FILE_READ_DATA,
    FILE_EXECUTE | FILE_READ_DATA
};

extern KSPIN_LOCK MmVmmLock;

ULONG
VEAPI
MiMakeProtectionMask(IN ULONG Protect)
{
    ULONG Mask1, Mask2, ProtectMask;

    /* PAGE_EXECUTE_WRITECOMBINE is theoretically the maximum */
    if (Protect >= (PAGE_WRITECOMBINE * 2)) return MM_INVALID_PROTECTION;

    /*
     * Windows API protection mask can be understood as two bitfields, differing
     * by whether or not execute rights are being requested
     */
    Mask1 = Protect & 0xF;
    Mask2 = (Protect >> 4) & 0xF;

    /* Check which field is there */
    if (!Mask1)
    {
        /* Mask2 must be there, use it to determine the PTE protection */
        if (!Mask2) return MM_INVALID_PROTECTION;
        ProtectMask = MmUserProtectionToMask2[Mask2];
    }
    else
    {
        /* Mask2 should not be there, use Mask1 to determine the PTE mask */
        if (Mask2) return MM_INVALID_PROTECTION;
        ProtectMask = MmUserProtectionToMask1[Mask1];
    }

    /* Make sure the final mask is a valid one */
    if (ProtectMask == MM_INVALID_PROTECTION) return MM_INVALID_PROTECTION;

    /* Check for PAGE_GUARD option */
    if (Protect & PAGE_GUARD)
    {
        /* It's not valid on no-access, nocache, or writecombine pages */
        if ((ProtectMask == MM_NOACCESS) ||
            (Protect & (PAGE_NOCACHE | PAGE_WRITECOMBINE)))
        {
            /* Fail such requests */
            return MM_INVALID_PROTECTION;
        }

        /* This actually turns on guard page in this scenario! */
        ProtectMask |= MM_GUARDPAGE;
    }

    /* Check for nocache option */
    if (Protect & PAGE_NOCACHE)
    {
        /* The earlier check should've eliminated this possibility */

        /* Check for no-access page or write combine page */
        if ((ProtectMask == MM_NOACCESS) || (Protect & PAGE_WRITECOMBINE))
        {
            /* Such a request is invalid */
            return MM_INVALID_PROTECTION;
        }

        /* Add the PTE flag */
        ProtectMask |= MM_NOCACHE;
    }

    /* Check for write combine option */
    if (Protect & PAGE_WRITECOMBINE)
    {
        /* The two earlier scenarios should've caught this */

        /* Don't allow on no-access pages */
        if (ProtectMask == MM_NOACCESS) return MM_INVALID_PROTECTION;

        /* This actually turns on write-combine in this scenario! */
        ProtectMask |= MM_NOACCESS;
    }

    /* Return the final MM PTE protection mask */
    return ProtectMask;
}

static
RTL_GENERIC_COMPARE_RESULTS
VEAPI
MiSectionPageTableCompare(PRTL_GENERIC_TABLE Table,
                          PVOID PtrA,
                          PVOID PtrB)
{
    PLARGE_INTEGER A = PtrA, B = PtrB;
    BOOLEAN Result = (A->QuadPart < B->QuadPart) ? GenericLessThan :
        (A->QuadPart == B->QuadPart) ? GenericEqual : GenericGreaterThan;

    return Result;
}

static
PVOID
VEAPI
MiSectionPageTableAllocate(PRTL_GENERIC_TABLE Table, LONG Bytes)
{
    PVOID Result;
    Result = UlAllocatePoolWithTag(NonPagedPool, Bytes, 'tPmM');
    return Result;
}

static
VOID
VEAPI
MiSectionPageTableFree(PRTL_GENERIC_TABLE Table, PVOID Data)
{
    UlFreePoolWithTag(Data, 'tPmM');
}

VOID
VEAPI
MiInitializeSectionPageTable(IN PMM_SECTION_SEGMENT Segment)
{
    RtlInitializeGenericTable(&Segment->PageTable,
                              MiSectionPageTableCompare,
                              MiSectionPageTableAllocate,
                              MiSectionPageTableFree,
                              NULL);

    DPRINT("MiInitializeSectionPageTable(%p)\n", &Segment->PageTable);
}

VOID
VEAPI
MmCreatePhysicalMemorySection(VOID)
{
    PSECTION PhysSection;
    VEASTATUS Status;
    OBJECT_ATTRIBUTES Obj;
    ANSI_STRING Name = RTL_CONSTANT_ANSI_STRING("\\Device\\PhysicalMemory");
    LARGE_INTEGER SectionSize;
    HANDLE Handle;
    PMM_SECTION_SEGMENT Segment;

    SectionSize.QuadPart = MmHighestPhysicalPage * PAGE_SIZE;
    InitializeObjectAttributes(&Obj,
                               &Name,
                               OBJ_PERMANENT,
                               NULL,
                               NULL);

    Status = ObCreateObject(
        KernelMode,
        MmSectionObjectType,
        &Obj,
        KernelMode,
        NULL,
        sizeof(*PhysSection),
        0,
        0,
        (PVOID*)&PhysSection
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[MM] Failed: Failed to create object: 0x%x\n", Status);
        return;
    }

    RtlZeroMemory(PhysSection, sizeof(*PhysSection));

    PhysSection->Flags.filler = 1;
    PhysSection->InitialPageProtection = PAGE_EXECUTE_READWRITE;
    PhysSection->Flags.PhysicalMemory = 1;
    PhysSection->SizeOfSection = SectionSize;
    Segment = UlAllocatePoolZero(NonPagedPool, sizeof(MM_SECTION_SEGMENT), TAG_MM_SECTION_SEGMENT);
    if (!Segment)
    {
        ObDereferenceObject(PhysSection);
        DPRINT("[MM] Failed: Failed to Allocate Pool for Section Segment\n");
        return;
    }

    PhysSection->Segment = (PSEGMENT)Segment;
    Segment->RefCount = 1;
    Segment->ReferenceCount = &Segment->RefCount;
    Segment->Flags = &Segment->SegFlags;
    Segment->Image.FileOffset = 0;
    Segment->Protection = PAGE_EXECUTE_READWRITE;
    Segment->RawLength = SectionSize;
    Segment->Length = SectionSize;
    Segment->SegFlags = MM_PHYSICALMEMORY_SEGMENT;
    Segment->WriteCopy = FALSE;
    Segment->Image.VirtualAddress = 0;
    Segment->Image.Characteristics = 0;
    MiInitializeSectionPageTable(Segment);

    Status = ObInsertObject(
        PhysSection,
        NULL,
        SECTION_ALL_ACCESS,
        0,
        NULL,
        &Handle
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[MM] Failed: Failed to insert Physical Object: 0x%x\n", Status);
        return;
    }
    ObCloseHandle(Handle, KernelMode);

    return;
}

VOID
VEAPI
MmDereferenceSegmentWithLock(
    IN PMM_SECTION_SEGMENT Segment,
    IN KIRQL OldIrql
)
{
    PFILE_OBJECT FileObject;

    // 1. Guard check NULL pointer
    if (!Segment)
    {
        KsReleaseDispatcherLock(OldIrql);
        return;
    }

    // 2. Dekremen Reference Count
    Segment->RefCount--;

    // 3. Jika masih ada yang pakai, lepas lock dan keluar
    if (Segment->RefCount > 0)
    {
        KsReleaseDispatcherLock(OldIrql);
        return;
    }

    FileObject = Segment->FileObject;

    // Lepas Dispatcher Lock SEBELUM dereference object atau free pool
    KsReleaseDispatcherLock(OldIrql);

    // 4. Clean up File Object jika ini file-backed segment
    if (FileObject != NULL)
    {
        ObDereferenceObject(FileObject);
    }

    // 5. Free memori segmen dari pool
    UlFreePool(Segment);
}

VOID
VEAPI
MmpDeleteSection(IN PVOID ObjectBody)
{
    PSECTION Section = (PSECTION)ObjectBody;

    if (!Section) return;

    // A. Hapus dari MmSectionBasedTree jika section ini bertipe SEC_BASED
    if (Section->Flags.Based)
    {
        // KeAcquireGuardedMutex(&MmSectionBasedMutex);
        RtlDeleteElementAvl(&MmSectionBasedTree, (PRTL_BALANCED_NODE)Section);
        // KeReleaseGuardedMutex(&MmSectionBasedMutex);
    }

    // B. Cleanup Segment & Image Objects
    if (Section->Flags.Image)
    {
        PMM_IMAGE_SECTION_OBJECT ImageSectionObject = (PMM_IMAGE_SECTION_OBJECT)Section->Segment;
        KIRQL OldIrql;

        if (!ImageSectionObject) return;

        OldIrql = KsAcquireDispatcherLock();

        if (ImageSectionObject->Segments)
        {
            MmDereferenceSegmentWithLock(ImageSectionObject->Segments, OldIrql);
        }
        else
        {
            KsReleaseDispatcherLock(OldIrql);
        }

        // Free kontainer Image Section Object itu sendiri
        UlFreePool(ImageSectionObject);
    }
    else
    {
        PMM_SECTION_SEGMENT Segment = (PMM_SECTION_SEGMENT)Section->Segment;
        KIRQL OldIrql;

        if (!Segment) return;

        OldIrql = KsAcquireDispatcherLock();
        
        if (Segment->SectionCount > 0)
        {
            Segment->SectionCount--;
        }

        MmDereferenceSegmentWithLock(Segment, OldIrql);
    }
}

VOID
VEAPI
MmpCloseSection(
    IN PEPROCESS Process OPTIONAL,
    IN PVOID ObjectBody,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG_PTR ProcessHandleCount,
    IN ULONG_PTR SystemHandleCount
)
{
    PSECTION Section = (PSECTION)ObjectBody;
    PCONTROL_AREA ControlArea;
    KIRQL OldIrql;

    if (!Section) return;

    // Cek apakah ini handle terakhir yang ditutup di seluruh OS
    if (SystemHandleCount == 1)
    {
        OldIrql = KsAcquireDispatcherLock();

        // Jika section ini dibuka/dibuat dari User Mode
        if (Section->Flags.UserReference)
        {
            Section->Flags.UserReference = 0;

            // Kalo lu pake Control Area untuk tracking File-backed / Shared Section
            if (Section->Segment && Section->Segment->ControlArea)
            {
                ControlArea = Section->Segment->ControlArea;
                
                if (ControlArea->NumberOfUserReferences > 0)
                {
                    ControlArea->NumberOfUserReferences--;
                }

                // Kalo udah ga ada user reference & section reference, tandai siap di-purge
                if (ControlArea->NumberOfUserReferences == 0 && 
                    ControlArea->NumberOfSectionReferences == 0)
                {
                    ControlArea->u.Flags.BeingDeleted = 1;
                }
            }
        }

        // Kalo ada flag DeleteOnClose (misal file temporary / swap)
        if (Section->Flags.DeleteOnClose)
        {
            Section->Flags.BeingDeleted = 1;
        }

        KsReleaseDispatcherLock(OldIrql);
    }
}

VOID
VEAPI
MmInitSectionImplementation(VOID)
{
    OBJECT_TYPE_INITIALIZER ObjectTypeInitializer;

    RtlZeroMemory(&MmSectionBasedTree, sizeof(RTL_AVL_TREE));
    MmSectionBasedTree.Root = NULL;
    MmSectionBasedTree.CompareRoutine = MmpCompareBasedNode;

    RtlZeroMemory(&ObjectTypeInitializer, sizeof(ObjectTypeInitializer));
    ObjectTypeInitializer.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    ObjectTypeInitializer.DefaultNonPagedPoolCharge = sizeof(SECTION);
    ObjectTypeInitializer.ObjectSize = sizeof(SECTION);
    ObjectTypeInitializer.AllowAttachByDefault = FALSE;
    ObjectTypeInitializer.ValidAccessMask = SECTION_ALL_ACCESS;
    ObjectTypeInitializer.InterruptHandler = NULL;
    ObjectTypeInitializer.DeleteRoutine = MmpDeleteSection;
    ObjectTypeInitializer.CloseRoutine = MmpCloseSection;
    ObCreateObjectType("Section", &ObjectTypeInitializer, &MmSectionObjectType);

    MmCreatePhysicalMemorySection();
}

VEASTATUS
VEAPI
MiCreatePagingFileMap(
    OUT PSEGMENT *SegmentObject,
    IN ULONGLONG SectionSize,
    IN ULONG ProtectionMask,
    IN ULONG AllocationAttributes
)
{
    PSEGMENT Segment;
    PCONTROL_AREA ControlArea;
    ULONG TotalPages;
    SIZE_T SegmentSize;

    if (SectionSize == 0) return STATUS_INVALID_PARAMETER;

    TotalPages = (ULONG)((SectionSize + PAGE_SIZE - 1) >> PAGE_SHIFT);
    SegmentSize = sizeof(SEGMENT) + (TotalPages * sizeof(PAGE_ENTRY));

    // Alokasi Segment + Prototype PTEs
    Segment = (PSEGMENT)UlAllocatePoolWithTag(NonPagedPool, SegmentSize, 'SgMm');
    if (!Segment) return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(Segment, SegmentSize);

    // Alokasi Control Area + Subsection
    ControlArea = (PCONTROL_AREA)UlAllocatePoolWithTag(NonPagedPool, sizeof(CONTROL_AREA) + sizeof(SUBSECTION), 'CaMm');
    if (!ControlArea)
    {
        UlFreePool(Segment);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(ControlArea, sizeof(CONTROL_AREA) + sizeof(SUBSECTION));

    // Setup Control Area Flags
    ControlArea->Segment = Segment;
    ControlArea->NumberOfSectionReferences = 1;
    ControlArea->NumberOfUserReferences = 1;
    if (AllocationAttributes & SEC_COMMIT) ControlArea->u.Flags.Commit = 1;

    // Setup Segment
    Segment->ControlArea = ControlArea;
    Segment->TotalNumberOfPtes = TotalPages;
    Segment->SizeOfSegment = SectionSize;

    *SegmentObject = Segment;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
MiCreateDataFileMap(
    IN PFILE_OBJECT File,
    OUT PSEGMENT *SegmentObject,
    IN PLARGE_INTEGER MaximumSize,
    IN ULONG SectionPageProtection,
    IN ULONG AllocationAttributes,
    IN BOOLEAN KernelCall
)
{
    PSEGMENT Segment;
    PCONTROL_AREA ControlArea;
    ULONGLONG FileSize = 0; // Nanti dibaca via File->FileSize atau FsRtlQuery
    ULONG TotalPages;

    if (MaximumSize && MaximumSize->QuadPart > 0)
    {
        FileSize = MaximumSize->QuadPart;
    }

    if (FileSize == 0) return STATUS_INVALID_FILE_FOR_SECTION;

    TotalPages = (ULONG)((FileSize + PAGE_SIZE - 1) >> PAGE_SHIFT);

    Segment = (PSEGMENT)UlAllocatePoolWithTag(NonPagedPool, sizeof(SEGMENT), 'SgMm');
    if (!Segment) return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(Segment, sizeof(SEGMENT));

    // Ambil CA yang udah dialokasikan di MmCreateArm3Section
    ControlArea = File->SectionObjectPointer->DataSectionObject;

    ControlArea->Segment = Segment;
    ControlArea->FilePointer = File;
    ControlArea->u.Flags.File = 1;

    Segment->ControlArea = ControlArea;
    Segment->TotalNumberOfPtes = TotalPages;
    Segment->SizeOfSegment = FileSize;

    *SegmentObject = Segment;
    return STATUS_SUCCESS;
}

VOID
VEAPI
MiCheckControlArea(
    IN PCONTROL_AREA ControlArea,
    IN KIRQL OldIrql
)
{
    // Jika refcount section & user menyentuh angka 0, free ControlArea & Segment
    if (ControlArea->NumberOfSectionReferences == 0 &&
        ControlArea->NumberOfUserReferences == 0)
    {
        PSEGMENT Segment = ControlArea->Segment;
        
        MiReleasePfnLock(OldIrql);

        if (Segment)
        {
            UlFreePool(Segment);
        }

        UlFreePool(ControlArea);
        return;
    }

    MiReleasePfnLock(OldIrql);
}

VEASTATUS
VEAPI
MmCreateArm3Section(
    OUT PVOID *SectionObject,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN PLARGE_INTEGER InputMaximumSize,
    IN ULONG SectionPageProtection,
    IN ULONG AllocationAttributes,
    IN HANDLE FileHandle OPTIONAL,
    IN PFILE_OBJECT FileObject OPTIONAL
)
{
    SECTION Section;
    PSECTION NewSection;
    PSUBSECTION Subsection;
    PSEGMENT NewSegment = NULL, Segment = NULL;
    VEASTATUS Status;
    PCONTROL_AREA ControlArea;
    ULONG ProtectionMask, ControlAreaSize, Size, NonPagedCharge, PagedCharge;
    KPROCESSOR_MODE PreviousMode = KernelMode; // TODO: UlGetPreviousMode();
    BOOLEAN FileLock = FALSE, KernelCall = FALSE;
    KIRQL OldIrql;
    PFILE_OBJECT File = FileObject;
    BOOLEAN UserRefIncremented = FALSE;
    PVOID PreviousSectionPointer;

    /* Conversion flag */
    if (AllocationAttributes & SEC_NOCACHE)
        SectionPageProtection |= PAGE_NOCACHE;

    /* Validasi Protection Mask */
    ProtectionMask = MiMakeProtectionMask(SectionPageProtection);
    if (ProtectionMask == MM_INVALID_PROTECTION)
    {
        return STATUS_INVALID_PAGE_PROTECTION;
    }

    /* 1. JIKA SECTION TERIKAT KE FILE (File-Backed Section) */
    if ((FileHandle) || (File))
    {
        if (AllocationAttributes & SEC_LARGE_PAGES) 
            return STATUS_INVALID_PARAMETER;

        /* Jika belum berupa FileObject, ambil dari FileHandle */
        if (!File)
        {
            Status = ObReferenceObjectByHandle(
                FileHandle,
                MmMakeFileAccess[ProtectionMask],
                ObpFileObjectType,
                PreviousMode,
                (PVOID*)&File
            );
            if (!VEA_SUCCESS(Status)) return Status;
        }

        /* Hitung ukuran Control Area & Subsection */
        ControlAreaSize = sizeof(CONTROL_AREA) + sizeof(MSUBSECTION);
        ControlArea = (PCONTROL_AREA)UlAllocatePoolWithTag(NonPagedPool, ControlAreaSize, 'aCmM');
        if (!ControlArea)
        {
            if (FileHandle) ObDereferenceObject(File);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(ControlArea, ControlAreaSize);

        if (FileHandle)
        {
            // ObioSetTopLevelIrp((PIRP)FSRTL_FSP_TOP_LEVEL_IRP);
            FileLock = TRUE;
        }

        /* Lock PFN/Section Database */
        OldIrql = MiAcquirePfnLock();

        ControlArea->u.Flags.BeingCreated = TRUE;
        PreviousSectionPointer = File->SectionObjectPointer;
        
        if (File->SectionObjectPointer)
        {
            File->SectionObjectPointer->DataSectionObject = ControlArea;
        }

        MiReleasePfnLock(OldIrql);

        /* Buat Mapping File Data */
        Status = MiCreateDataFileMap(
            File,
            &Segment,
            InputMaximumSize,
            SectionPageProtection,
            AllocationAttributes,
            KernelCall
        );

        if (!VEA_SUCCESS(Status))
        {
            OldIrql = MiAcquirePfnLock();
            
            if (File->SectionObjectPointer)
            {
                File->SectionObjectPointer->DataSectionObject = NULL;
            }
            
            ControlArea->u.Flags.BeingCreated = FALSE;
            MiReleasePfnLock(OldIrql);

            // TODO: if (FileLock) ObioSetTopLevelIrp(NULL);

            UlFreePool(ControlArea);
            if (FileHandle) ObDereferenceObject(File);

            return Status;
        }

        /* Set Ukuran Section */
        if (!InputMaximumSize || !InputMaximumSize->QuadPart)
        {
            Section.SizeOfSection.QuadPart = (LONGLONG)Segment->SizeOfSegment;
        }
        else
        {
            Section.SizeOfSection.QuadPart = InputMaximumSize->QuadPart;
        }
    }
    /* 2. JIKA SECTION TIDAK PUNYA FILE (Anonymous / Pagefile-Backed Section) */
    else
    {
        if (AllocationAttributes & SEC_IMAGE) 
            return STATUS_INVALID_FILE_FOR_SECTION;

        Status = MiCreatePagingFileMap(
            &NewSegment,
            InputMaximumSize ? InputMaximumSize->QuadPart : 0,
            ProtectionMask,
            AllocationAttributes
        );

        if (!VEA_SUCCESS(Status)) return Status;

        Section.SizeOfSection.QuadPart = NewSegment->SizeOfSegment;
        ControlArea = NewSegment->ControlArea;
        UserRefIncremented = TRUE;
    }

    if (!NewSegment)
    {
        NewSegment = Segment;
        OldIrql = MiAcquirePfnLock();
        ControlArea->u.Flags.BeingCreated = FALSE;
        MiReleasePfnLock(OldIrql);
    }

    if (FileLock)
    {
        // ObioSetTopLevelIrp(NULL);
        FileLock = FALSE;
    }

    /* Setup Struktur Header SECTION sementara di Stack */
    RtlZeroMemory(&Section, sizeof(SECTION));
    Section.SizeOfSection.QuadPart = NewSegment->SizeOfSegment;
    Section.InitialPageProtection = SectionPageProtection;
    Section.Segment = NewSegment;
    Section.LongFlags = ControlArea->u.LongFlags;

    if (!FileObject &&
        (SectionPageProtection & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) &&
        !(ControlArea->u.Flags.Image) &&
        (ControlArea->FilePointer))
    {
        Section.Flags.UserWritable = TRUE;
        InterlockedIncrement((volatile PLONG)&ControlArea->WritableUserReferences);
    }

    if ((ControlArea->u.Flags.Image) || !(ControlArea->FilePointer))
    {
        PagedCharge = sizeof(SECTION) + NewSegment->TotalNumberOfPtes * sizeof(PAGE_ENTRY);
        Size = sizeof(SUBSECTION);
    }
    else
    {
        PagedCharge = 0;
        Size = sizeof(MSUBSECTION);
    }

    NonPagedCharge = sizeof(CONTROL_AREA) + Size;
    Subsection = (PSUBSECTION)(ControlArea + 1);

    /* Alokasikan Objek SECTION utama dari Object Manager VeaOS */
    Status = ObCreateObject(
        PreviousMode,
        MmSectionObjectType,
        ObjectAttributes,
        PreviousMode,
        NULL,
        sizeof(SECTION),
        PagedCharge,
        NonPagedCharge,
        (PVOID*)&NewSection
    );

    if (!VEA_SUCCESS(Status))
    {
        if (UserRefIncremented)
        {
            OldIrql = MiAcquirePfnLock();
            ControlArea->NumberOfSectionReferences--;
            ControlArea->NumberOfUserReferences--;
            MiCheckControlArea(ControlArea, OldIrql);
        }
        return Status;
    }

    /* Salin data dari Stack ke Objek SECTION asli */
    RtlCopyMemory(NewSection, &Section, sizeof(SECTION));
    NewSection->Address.StartingVpn = 0;
    NewSection->Flags.UserReference = TRUE;

    /* Handle SEC_BASED (Alokasi Alamat Virtual Terikat via AVL Tree) */
    if (AllocationAttributes & SEC_BASED)
    {
        if (ControlArea->u.Flags.BeingCreated == 1)
        {
            Status = MiFindEmptyAddressRangeDownBasedTree(
                (SIZE_T)ControlArea->Segment->SizeOfSegment,
                0x7FFF0000, // Top of User Virtual Memory
                _64K,
                &MmSectionBasedTree,
                (ULONG_PTR*)&ControlArea->Segment->BasedAddress
            );

            if (!VEA_SUCCESS(Status))
            {
                ObDereferenceObject(NewSection);
                return Status;
            }

            NewSection->Address.StartingVpn = (ULONG_PTR)ControlArea->Segment->BasedAddress >> PAGE_SHIFT;
            NewSection->Address.EndingVpn = NewSection->Address.StartingVpn + (NewSection->SizeOfSection.LowPart >> PAGE_SHIFT) - 1;
            MiInsertBasedSection(NewSection);
        }
    }

    if (ControlArea->u.Flags.BeingCreated == 1)
    {
        OldIrql = MiAcquirePfnLock();
        ControlArea->u.Flags.BeingCreated = 0;
        NewSection->Flags.BeingCreated = 0;
        MiReleasePfnLock(OldIrql);
    }

    if (AllocationAttributes & SEC_NO_CHANGE) NewSection->Flags.NoChange = TRUE;

    if (!(SectionPageProtection & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)))
    {
        NewSection->Flags.CopyOnWrite = TRUE;
    }

    *SectionObject = (PVOID)NewSection;
    return Status;
}

VEASTATUS
VEAPI
MmCreateSection(
    OUT PVOID *SectionObject,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN PLARGE_INTEGER MaximumSize,
    IN ULONG SectionPageProtection,
    IN ULONG AllocationAttributes,
    IN HANDLE FileHandle OPTIONAL,
    IN PFILE_OBJECT FileObject OPTIONAL
)
{
    VEASTATUS Status;
    PFILE_OBJECT LocalFileObject = FileObject;
    BOOLEAN DereferenceFile = FALSE;
    BOOLEAN FileLocked = FALSE;

    if (AllocationAttributes & SEC_LARGE_PAGES)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (!FileHandle && !LocalFileObject)
    {
        if (AllocationAttributes & SEC_IMAGE)
        {
            // SEC_IMAGE Wajib punya file!
            return STATUS_INVALID_FILE_FOR_SECTION;
        }

        return MmCreateArm3Section(
            SectionObject,
            DesiredAccess,
            ObjectAttributes,
            MaximumSize,
            SectionPageProtection,
            AllocationAttributes,
            NULL,
            NULL
        );
    }

    return STATUS_NOT_IMPLEMENTED;
}

VEASTATUS
VEAPI
MmMapViewOfSection(
    IN PSECTION SectionObject,
    IN PEPROCESS Process,
    IN OUT PVOID *BaseAddress,
    IN ULONG_PTR ZeroBits,
    IN SIZE_T CommitSize,
    IN OUT PLARGE_INTEGER SectionOffset OPTIONAL,
    IN OUT PSIZE_T ViewSize,
    IN SECTION_INHERIT InheritDisposition,
    IN ULONG AllocationType,
    IN ULONG Protect
)
{
    PSECTION Section = SectionObject;
    PCONTROL_AREA ControlArea;
    PSEGMENT Segment;
    PMM_VAD Vad = NULL;
    VEASTATUS Status = STATUS_SUCCESS;
    BOOLEAN IsAttached = FALSE;
    ULONGLONG ViewOffset = 0;
    ULONG PageCount;
    ULONG_PTR TargetVa = 0;
    KIRQL OldIrql;

    if (!Section || !Process || !BaseAddress || !ViewSize)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (!Protect || MiMakeProtectionMask(Protect) == MM_INVALID_PROTECTION)
    {
        return STATUS_INVALID_PAGE_PROTECTION;
    }

    // Cek Akses Write
    if ((Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) &&
        !(Section->InitialPageProtection & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)))
    {
        return STATUS_SECTION_PROTECTION;
    }

    // Cek Akses Read
    if ((Protect & (PAGE_READONLY | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_WRITECOPY)) &&
        !(Section->InitialPageProtection & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
    {
        return STATUS_SECTION_PROTECTION;
    }

    // Cek Akses Execute
    if ((Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) &&
        !(Section->InitialPageProtection & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
    {
        return STATUS_SECTION_PROTECTION;
    }

    if ((PEPROCESS)PtGetCurrentProcess() != Process)
    {
        PtAttachProcess((PKPROCESS)Process);
        IsAttached = TRUE;
    }

    Segment = Section->Segment;
    ControlArea = Segment ? Segment->ControlArea : NULL;

    if (SectionOffset)
    {
        SectionOffset->QuadPart &= ~(PAGE_SIZE - 1);
        ViewOffset = (ULONGLONG)SectionOffset->QuadPart;
    }

    // A. Overflow Check
    if ((ViewOffset + *ViewSize) < ViewOffset)
    {
        Status = STATUS_INVALID_VIEW_SIZE;
        goto Exit;
    }

    // B. Auto-calculate ViewSize jika caller minta 0
    if (*ViewSize == 0)
    {
        if ((ULONGLONG)Section->SizeOfSection.QuadPart <= ViewOffset)
        {
            Status = STATUS_INVALID_VIEW_SIZE;
            goto Exit;
        }

        *ViewSize = (SIZE_T)(Section->SizeOfSection.QuadPart - ViewOffset);
    }

    // C. Check Boundary terhadap ukuran Section asli
    if ((ViewOffset + *ViewSize) > (ULONGLONG)Section->SizeOfSection.QuadPart)
    {
        Status = STATUS_INVALID_VIEW_SIZE;
        goto Exit;
    }

    *ViewSize = PAGE_ROUND_UP(*ViewSize);
    PageCount = (ULONG)(*ViewSize >> PAGE_SHIFT);

    TargetVa = (ULONG_PTR)*BaseAddress;
    if (TargetVa != 0)
    {
        TargetVa = PAGE_ALIGN_DOWN(TargetVa);
    }

    // -----------------------------------------------------------------
    // MANAJEMEN VIRTUAL ADDRESS & VAD TREE (TANPA DOUBLE SPINLOCK)
    // -----------------------------------------------------------------
    if (TargetVa == 0)
    {
        // 1. Jalur Alokasi Otomatis: MmAllocateVirtualRange mengelola VAD & MmVmmLock sendiri
        TargetVa = MmAllocateVirtualRange(PageCount);
        if (TargetVa == 0)
        {
            Status = STATUS_NO_MEMORY;
            goto Exit;
        }
    }
    else
    {
        // 2. Jalur Fixed Target VA: Buat VAD manual lalu insert ke VMM Tree
        Vad = (PMM_VAD)UlAllocatePoolWithTag(NonPagedPool, sizeof(MM_VAD), 'daVM');
        if (!Vad)
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Exit;
        }

        RtlZeroMemory(Vad, sizeof(MM_VAD));
        Vad->StartingVpn = TargetVa >> PAGE_SHIFT;
        Vad->EndingVpn = Vad->StartingVpn + PageCount - 1;
        Vad->ControlArea = ControlArea;

        KsAcquireSpinLock(&MmVmmLock, &OldIrql);
        RtlInsertElementAvl(&MmKernelVadTree, (PRTL_BALANCED_NODE)Vad);
        KsReleaseSpinLock(&MmVmmLock, OldIrql);
    }

    if (ControlArea)
    {
        InterlockedIncrement((PLONG)&ControlArea->NumberOfMappedViews);
        InterlockedIncrement((PLONG)&ControlArea->NumberOfSectionReferences);
    }

    *BaseAddress = (PVOID)TargetVa;

    DPRINT("[MM] Section 0x%p Mapped into Process '%s' at VA 0x%08X (Size: %u bytes)\n",
           Section, Process->ImageFileName, TargetVa, *ViewSize);

Exit:
    if (IsAttached)
    {
        PtDetachProcess();
    }

    return Status;
}

VEASTATUS
VEAPI
MmUnmapViewOfSection(
    IN PEPROCESS Process,
    IN PVOID BaseAddress
)
{
    PMM_VAD Vad = NULL;
    VEASTATUS Status = STATUS_SUCCESS;
    BOOLEAN IsAttached = FALSE;
    ULONG_PTR TargetVa = (ULONG_PTR)BaseAddress;
    ULONG_PTR VpnTarget;
    ULONG PageCount;
    ULONG_PTR CurrentVa;
    PCONTROL_AREA ControlArea = NULL;
    KIRQL OldIrql;

    if (!Process || !BaseAddress)
    {
        return STATUS_INVALID_PARAMETER;
    }

    TargetVa = PAGE_ALIGN_DOWN(TargetVa);
    VpnTarget = TargetVa >> PAGE_SHIFT;

    // 1. Switch Process Context jika unmap di proses lain
    if ((PEPROCESS)PtGetCurrentProcess() != Process)
    {
        PtAttachProcess((PKPROCESS)Process);
        IsAttached = TRUE;
    }

    // 2. Lock VMM Tree & Cari Node VAD
    KsAcquireSpinLock(&MmVmmLock, &OldIrql);

    Vad = MmpFindVadByVpn(MmKernelVadTree.Root, VpnTarget);
    if (!Vad)
    {
        KsReleaseSpinLock(&MmVmmLock, OldIrql);
        Status = STATUS_NOT_MAPPED_VIEW;
        goto Exit;
    }

    // BaseAddress HARUS persis sama dengan StartingVpn VAD (tidak boleh unmap parsial di tengah)
    if (Vad->StartingVpn != VpnTarget)
    {
        KsReleaseSpinLock(&MmVmmLock, OldIrql);
        Status = STATUS_FREE_VM_NOT_AT_BASE;
        goto Exit;
    }

    PageCount = (Vad->EndingVpn - Vad->StartingVpn) + 1;
    ControlArea = Vad->ControlArea;

    // 3. Hapus PTE & Bebaskan Memori Fisik untuk setiap halaman
    CurrentVa = TargetVa;
    for (ULONG i = 0; i < PageCount; i++, CurrentVa += PAGE_SIZE)
    {
        PAGE_ENTRY *pde = MM_GET_PDE(CurrentVa);
        if (!pde || !pde->bits.present)
        {
            continue;
        }

        PAGE_ENTRY *pte = MM_GET_PTE(CurrentVa);
        if (pte && pte->bits.present)
        {
            ULONG PhysAddress = pte->bits.frame << PAGE_SHIFT;

            // Hapus mapping di Hardware Page Table
            MI_WRITE_INVALID_PTE(pte);

            // Flush TLB CPU
            MMI_INVALIDATE_TLB(CurrentVa);

            // Bebaskan physical RAM page ke PIP Database Free List
            MmFreePhysicalPage(PhysAddress);
        }
    }

    // 4. Cabut Node VAD dari AVL Tree
    RtlDeleteElementAvl(&MmKernelVadTree, (PRTL_BALANCED_NODE)Vad);
    MmVadRoot = (PMM_VAD)MmKernelVadTree.Root;

    KsReleaseSpinLock(&MmVmmLock, OldIrql);

    // Bebaskan struct MM_VAD ke VAD Free List
    MmpFreeVadNode(Vad);

    // 5. Update Reference Counter pada Control Area
    if (ControlArea)
    {
        InterlockedDecrement((PLONG)&ControlArea->NumberOfMappedViews);

        OldIrql = MiAcquirePfnLock();
        ControlArea->NumberOfSectionReferences--;
        
        // Panggil MiCheckControlArea untuk free ControlArea jika refcount 0
        MiCheckControlArea(ControlArea, OldIrql);
    }

    DPRINT("[MM] Section View at VA 0x%08X Unmapped Successfully (%u pages freed)\n",
           TargetVa, PageCount);

Exit:
    if (IsAttached)
    {
        PtDetachProcess();
    }

    return Status;
}

/* EOF */