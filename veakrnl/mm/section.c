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

VEASTATUS
VEAPI
MiSimpleReadFile(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER ByteOffset OPTIONAL,
    IN ULONG Length,
    OUT PVOID Buffer,
    OUT PULONG BytesRead
)
{
    PDEVICE_OBJECT DeviceObject;
    KEVENT Event;
    OIP_INFORMATION IoStatusBlock;
    POIP Oip;
    VEASTATUS Status;
    PVOID ReadBuffer = Buffer;
    ULONG AlignedLength = ALIGN_UP_BY(Length, 512);
    BOOLEAN AllocatedTemp = FALSE;

    if (!FileObject || !Buffer || Length == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    DeviceObject = FileObject->DeviceObject;
    if (!DeviceObject)
    {
        return STATUS_INVALID_FILE_FOR_SECTION;
    }

    /* Jika Length bukan kelipatan 512, alokasikan buffer sementara berukuran ter-align */
    if (Length % 512 != 0)
    {
        ReadBuffer = UlAllocatePoolWithTag(NonPagedPool, AlignedLength, 'rDmM');
        if (!ReadBuffer) return STATUS_INSUFFICIENT_RESOURCES;
        AllocatedTemp = TRUE;
    }

    KsInitializeEvent(&Event, NotificationEvent, FALSE);
    RtlZeroMemory(&IoStatusBlock, sizeof(OIP_INFORMATION));

    Oip = ObioBuildAsynchronousFsdRequest(
        OIP_MJ_READ,
        DeviceObject,
        ReadBuffer,
        AlignedLength,
        ByteOffset,
        &IoStatusBlock
    );

    if (!Oip)
    {
        if (AllocatedTemp) UlFreePool(ReadBuffer);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Oip->UserEvent = &Event;
    POIP_STACK Stack = ObioGetNextOipStackLocation(Oip);
    Stack->FileObject = FileObject;

    Status = ObioCallObject(DeviceObject, Oip);

    if (Status == STATUS_PENDING)
    {
        KsWaitForSingleObject(&Event);
        Status = IoStatusBlock.Status;
    }

    if (VEA_SUCCESS(Status))
    {
        if (AllocatedTemp)
        {
            RtlCopyMemory(Buffer, ReadBuffer, Length);
        }

        if (BytesRead)
        {
            *BytesRead = (ULONG)min(IoStatusBlock.Information, (ULONG_PTR)Length);
        }
    }

    if (AllocatedTemp)
    {
        UlFreePool(ReadBuffer);
    }

    return Status;
}


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
    PSEGMENT Segment;
    PCONTROL_AREA ControlArea;
    KIRQL OldIrql;

    if (!Section) return;

    // A. Hapus dari MmSectionBasedTree jika section ini bertipe SEC_BASED
    if (Section->Flags.Based)
    {
        RtlDeleteElementAvl(&MmSectionBasedTree, (PRTL_BALANCED_NODE)Section);
    }

    Segment = Section->Segment;
    if (!Segment) return;

    // B. Jika ini SEC_IMAGE, bebaskan metadata ELF dengan CONTAINING_RECORD
    if (Section->Flags.Image && Segment->u2.ImageInformation)
    {
        PMM_IMAGE_SECTION_OBJECT ImageSecObj = CONTAINING_RECORD(
            Segment->u2.ImageInformation, 
            MM_IMAGE_SECTION_OBJECT, 
            ImageInformation
        );

        if (ImageSecObj)
        {
            if (ImageSecObj->Segments)
            {
                UlFreePool(ImageSecObj->Segments);
            }
            UlFreePool(ImageSecObj);
        }
    }

    // C. Cleanup Hierarchy: Turunkan RefCount dan serahkan ke MiCheckControlArea
    ControlArea = Segment->ControlArea;
    if (ControlArea)
    {
        OldIrql = MiAcquirePfnLock();
        
        if (ControlArea->NumberOfSectionReferences > 0)
        {
            ControlArea->NumberOfSectionReferences--;
        }
        
        MiCheckControlArea(ControlArea, OldIrql);
    }
    else
    {
        UlFreePool(Segment);
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
    else if (File->FsContext)
    {
        PGENERIC_FCB Fcb = (PGENERIC_FCB)File->FsContext;
        FileSize = Fcb->FileSize;
    }
    else
    {
        FileSize = File->Size;
    }

    if (FileSize == 0) return STATUS_INVALID_FILE_FOR_SECTION;

    TotalPages = (ULONG)((FileSize + PAGE_SIZE - 1) >> PAGE_SHIFT);

    Segment = (PSEGMENT)UlAllocatePoolWithTag(NonPagedPool, sizeof(SEGMENT), 'SgMm');
    if (!Segment) return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(Segment, sizeof(SEGMENT));

    if (File->SectionObjectPointer)
    {
        ControlArea = File->SectionObjectPointer->DataSectionObject;
    }
    else
    {
        UlFreePool(Segment);
        return STATUS_INVALID_PARAMETER;
    }
    
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
    PSEGMENT Segment;
    BOOLEAN DeleteSection = FALSE;

    /* Guard Check */
    if (!ControlArea)
    {
        MiReleasePfnLock(OldIrql);
        return;
    }

    /* 
     * Kriteria Penghapusan ControlArea:
     * 1. Tidak ada referensi dari Handle Section (NumberOfSectionReferences == 0)
     * 2. Tidak ada view yang masih dipetakan ke memori (NumberOfUserReferences == 0)
     * 3. Tidak ada referensi dari sistem PFN (NumberOfPfnReferences == 0, opsional tergantung desain PFN lu)
     */
    if (ControlArea->NumberOfSectionReferences == 0 &&
        ControlArea->NumberOfUserReferences == 0)
    {
        /* Tandai sedang dihapus agar thread lain tidak mencoba me-reference */
        ControlArea->u.Flags.BeingDeleted = 1;
        DeleteSection = TRUE;
    }

    Segment = ControlArea->Segment;

    /* Lepaskan Lock Global PFN / VMM SEBELUM memanggil UlFreePool atau membebaskan Object 
     * untuk mencegah Deadlock.
     */
    MiReleasePfnLock(OldIrql);

    if (DeleteSection)
    {
        /* 1. Bersihkan Data File / Dereference FileObject */
        if (ControlArea->u.Flags.File && ControlArea->FilePointer)
        {
            PFILE_OBJECT FileObject = ControlArea->FilePointer;

            /* Hapus referensi dari FileObject Pointer agar tidak menunjuk ke ControlArea yang akan dihapus */
            if (FileObject->SectionObjectPointer)
            {
                if (ControlArea->u.Flags.Image)
                {
                    FileObject->SectionObjectPointer->ImageSectionObject = NULL;
                }
                else
                {
                    FileObject->SectionObjectPointer->DataSectionObject = NULL;
                }
            }

            /* Dereference FileObject yang ditahan oleh ControlArea */
            ObDereferenceObject(FileObject);
        }

        /* 2. Bersihkan Segment */
        if (Segment)
        {
            /* Jika ada Page Table internal (biasanya untuk data section besar), bersihkan */
            if (Segment->u2.FirstMappedVa == NULL && Segment->TotalNumberOfPtes > 0)
            {
               /* Tambahkan logika pembersihan PTE internal di sini jika Segment menggunakan 
                * mekanisme Page Table tersendiri (di luar VAD biasa).
                */
            }

            UlFreePool(Segment);
        }

        /* 3. Bebaskan Control Area beserta ekor Subsection-nya */
        UlFreePool(ControlArea);
    }
}

VEASTATUS
VEAPI
ElfFmtCreateImageSection(
    IN PFILE_OBJECT FileObject,
    OUT PMM_IMAGE_SECTION_OBJECT ImageSectionObject
)
{
    LARGE_INTEGER ByteOffset;
    ELF32_EHDR ElfHeader;
    VEASTATUS Status;
    ULONG ImageSize = 0;
    ULONG NrSegments = 0;
    ULONG BytesRead = 0;

    ByteOffset.QuadPart = 0;

    /* Baca ELF Header langsung dari PFILE_OBJECT */
    Status = MiSimpleReadFile(
        FileObject,
        &ByteOffset,
        sizeof(ELF32_EHDR),
        &ElfHeader,
        &BytesRead
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[ELF/FMT] MiSimpleReadFile failed to read ELF Header: 0x%08X\n", Status);
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    if (BytesRead < sizeof(ELF32_EHDR))
    {
        DPRINT("[ELF/FMT] Read incomplete ELF Header (%u / %u bytes)\n",
                BytesRead, sizeof(ELF32_EHDR));
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    if (!RtlValidateElfHeader(&ElfHeader, sizeof(ELF32_EHDR)))
    {
        MiDumpMemory((PVOID)&ElfHeader, 64);
        DPRINT("[ELF/FMT] RtlValidateElfHeader returned FALSE!\n");
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    DPRINT("[ELF/FMT] Header Validated: Type=0x%04X, Entry=0x%08X, ShNum=%u, PhNum=%u\n",
            ElfHeader.e_type, ElfHeader.e_entry, ElfHeader.e_shnum, ElfHeader.e_phnum);

    /* -------------------------------------------------------------------
     * KASUS A: Relocatable Object Driver (.ko) -> Section Headers
     * ------------------------------------------------------------------- */
    if (ElfHeader.e_type == ET_REL)
    {
        ULONG ShdrTableSize = ElfHeader.e_shnum * sizeof(ELF32_SHDR);
        PELF32_SHDR ShdrTable = (PELF32_SHDR)UlAllocatePoolWithTag(NonPagedPool, ShdrTableSize, 'fLE ');
        if (!ShdrTable) return STATUS_INSUFFICIENT_RESOURCES;

        ByteOffset.QuadPart = ElfHeader.e_shoff;
        Status = MiSimpleReadFile(FileObject, &ByteOffset, ShdrTableSize, ShdrTable, &BytesRead);

        if (!VEA_SUCCESS(Status) || BytesRead < ShdrTableSize)
        {
            DPRINT("[ELF/FMT] Failed to read Section Header Table (Status=0x%08X, Read=%u/%u)\n",
                    Status, BytesRead, ShdrTableSize);
            UlFreePool(ShdrTable);
            return STATUS_INVALID_IMAGE_FORMAT;
        }

        ULONG MaxSectionEndOffset = 0;

        for (ULONG i = 0; i < ElfHeader.e_shnum; i++)
        {
            if (ShdrTable[i].sh_flags & SHF_ALLOC) NrSegments++;

            /* ABAIKAN SHT_NULL dan SHT_NOBITS (.bss) karena tidak punya wujud di file fisik */
            if (ShdrTable[i].sh_type != SHT_NULL && ShdrTable[i].sh_type != SHT_NOBITS)
            {
                ULONG SecEnd = ShdrTable[i].sh_offset + ShdrTable[i].sh_size;
                
                /* SANITY CHECK: Filter nilai sampah (garbage). 
                 * Sebuah kernel object (.ko) normal tidak mungkin melebihi 16 MB.
                 */
                if (SecEnd > MaxSectionEndOffset && SecEnd < (16 * 1024 * 1024))
                {
                    MaxSectionEndOffset = SecEnd;
                }
            }
        }

        /* Jika Section Header Table ada di paling ujung file */
        ULONG ShdrEnd = ElfHeader.e_shoff + ShdrTableSize;
        if (ShdrEnd > MaxSectionEndOffset)
        {
            MaxSectionEndOffset = ShdrEnd;
        }

        DPRINT("[ELF/FMT] Found %u allocatable sections in ET_REL. MaxFileOffset = %u bytes\n", 
                NrSegments, MaxSectionEndOffset);

        if (NrSegments > 0)
        {
            ImageSectionObject->Segments = (PMM_SECTION_SEGMENT)UlAllocatePoolWithTag(
                NonPagedPool, sizeof(MM_SECTION_SEGMENT) * NrSegments, 'gSmM'
            );

            if (!ImageSectionObject->Segments)
            {
                UlFreePool(ShdrTable);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            RtlZeroMemory(ImageSectionObject->Segments, sizeof(MM_SECTION_SEGMENT) * NrSegments);

            ULONG SegIdx = 0;
            for (ULONG i = 0; i < ElfHeader.e_shnum; i++)
            {
                if (ShdrTable[i].sh_flags & SHF_ALLOC)
                {
                    PMM_SECTION_SEGMENT Seg = &ImageSectionObject->Segments[SegIdx++];
                    
                    /* Untuk ET_REL, tempatkan VirtualAddress sesuai sh_offset file */
                    Seg->Image.VirtualAddress = ShdrTable[i].sh_offset;
                    Seg->Image.FileOffset = ShdrTable[i].sh_offset;
                    Seg->Length.QuadPart = ALIGN_UP_BY(ShdrTable[i].sh_size, PAGE_SIZE);
                    Seg->RawLength.QuadPart = (ShdrTable[i].sh_type != SHT_NOBITS) ? ShdrTable[i].sh_size : 0;
                    Seg->Protection = RtlElfShFlagsToVeaProtection(ShdrTable[i].sh_flags);
                    Seg->FileObject = FileObject;

                    DPRINT("[ELF/FMT] Seg[%u]: VA=0x%08X, FileOff=0x%08X, Size=%u, Prot=0x%X\n",
                            SegIdx - 1, (ULONG)Seg->Image.VirtualAddress, (ULONG)Seg->Image.FileOffset,
                            (ULONG)Seg->Length.QuadPart, Seg->Protection);
                }
            }

            /* ImageSize WAJIB mencakup seluruh panjang file agar metadata ELF di ekor file ikut ter-map */
            ImageSize = MaxSectionEndOffset;
            ImageSectionObject->NrSegments = NrSegments;
        }

        UlFreePool(ShdrTable);
    }
    /* -------------------------------------------------------------------
     * KASUS B: Executable / Shared Library (ET_EXEC / ET_DYN) -> PT_LOAD
     * ------------------------------------------------------------------- */
    else if (ElfHeader.e_type == ET_EXEC || ElfHeader.e_type == ET_DYN)
    {
        ULONG PhdrTableSize = ElfHeader.e_phnum * sizeof(ELF32_PHDR);
        PELF32_PHDR PhdrTable = (PELF32_PHDR)UlAllocatePoolWithTag(NonPagedPool, PhdrTableSize, 'fLE ');
        if (!PhdrTable) return STATUS_INSUFFICIENT_RESOURCES;

        ByteOffset.QuadPart = ElfHeader.e_phoff;
        Status = MiSimpleReadFile(FileObject, &ByteOffset, PhdrTableSize, PhdrTable, &BytesRead);

        if (!VEA_SUCCESS(Status) || BytesRead < PhdrTableSize)
        {
            DPRINT("[ELF/FMT] Failed to read Program Header Table (Status=0x%08X)\n", Status);
            UlFreePool(PhdrTable);
            return STATUS_INVALID_IMAGE_FORMAT;
        }

        for (ULONG i = 0; i < ElfHeader.e_phnum; i++)
        {
            if (PhdrTable[i].p_type == PT_LOAD) NrSegments++;
        }

        if (NrSegments > 0)
        {
            ImageSectionObject->Segments = (PMM_SECTION_SEGMENT)UlAllocatePoolWithTag(
                NonPagedPool, sizeof(MM_SECTION_SEGMENT) * NrSegments, 'gSmM'
            );

            if (!ImageSectionObject->Segments)
            {
                UlFreePool(PhdrTable);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            RtlZeroMemory(ImageSectionObject->Segments, sizeof(MM_SECTION_SEGMENT) * NrSegments);

            ULONG SegIdx = 0;
            for (ULONG i = 0; i < ElfHeader.e_phnum; i++)
            {
                if (PhdrTable[i].p_type == PT_LOAD)
                {
                    PMM_SECTION_SEGMENT Seg = &ImageSectionObject->Segments[SegIdx++];
                    Seg->Image.VirtualAddress = PhdrTable[i].p_vaddr;
                    Seg->Image.FileOffset = PhdrTable[i].p_offset;
                    Seg->Length.QuadPart = ALIGN_UP_BY(PhdrTable[i].p_memsz, PAGE_SIZE);
                    Seg->RawLength.QuadPart = PhdrTable[i].p_filesz;
                    Seg->FileObject = FileObject;

                    ULONG Flags = 0;
                    if (PhdrTable[i].p_flags & PF_X) Flags |= SHF_EXECINSTR;
                    if (PhdrTable[i].p_flags & PF_W) Flags |= SHF_WRITE;
                    Seg->Protection = RtlElfShFlagsToVeaProtection(Flags);

                    ULONG EndVa = PhdrTable[i].p_vaddr + PhdrTable[i].p_memsz;
                    if (EndVa > ImageSize) ImageSize = EndVa;
                }
            }
            ImageSectionObject->NrSegments = NrSegments;
        }

        UlFreePool(PhdrTable);
    }

    if (ImageSize == 0 || NrSegments == 0)
    {
        DPRINT("[ELF/FMT] FAILED: ImageSize=%u, NrSegments=%u\n", ImageSize, NrSegments);
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    ImageSize = ALIGN_UP_BY(ImageSize, PAGE_SIZE);

    RtlZeroMemory(&ImageSectionObject->ImageInformation, sizeof(SECTION_IMAGE_INFORMATION));
    ImageSectionObject->ImageInformation.ImageFileSize = ImageSize;
    ImageSectionObject->ImageInformation.TransferAddress = (PVOID)(ULONG_PTR)ElfHeader.e_entry;
    ImageSectionObject->ImageInformation.Machine = ElfHeader.e_machine;
    ImageSectionObject->ImageInformation.ImageContainsCode = TRUE;
    ImageSectionObject->FileObject = FileObject;

    DPRINT("[ELF/FMT] SUCCESS: Total ImageSize = %u bytes (%u pages)\n", 
            ImageSize, ImageSize >> PAGE_SHIFT);

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
MmCreateImageSection(
    OUT PVOID *SectionObject,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN PLARGE_INTEGER InputMaximumSize OPTIONAL,
    IN ULONG SectionPageProtection,
    IN ULONG AllocationAttributes,
    IN PFILE_OBJECT FileObject
)
{
    PSECTION Section = NULL;
    PCONTROL_AREA ControlArea = NULL;
    PSEGMENT Segment = NULL;
    PMM_IMAGE_SECTION_OBJECT ImageSectionObject = NULL;
    VEASTATUS Status;
    ULONG TotalPages;
    SIZE_T SegmentSize;

    if (!FileObject || !SectionObject) return STATUS_INVALID_PARAMETER;

    /* 1. Alokasi MM_IMAGE_SECTION_OBJECT (Wadah internal untuk metadata ELF) */
    ImageSectionObject = (PMM_IMAGE_SECTION_OBJECT)UlAllocatePoolWithTag(
        NonPagedPool, sizeof(MM_IMAGE_SECTION_OBJECT), 'iIIC'
    );
    if (!ImageSectionObject) return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(ImageSectionObject, sizeof(MM_IMAGE_SECTION_OBJECT));

    /* 2. Parse ELF Image & Populate Segments Array */
    Status = ElfFmtCreateImageSection(FileObject, ImageSectionObject);
    if (!VEA_SUCCESS(Status))
    {
        UlFreePool(ImageSectionObject);
        return Status;
    }

    /* 3. Hitung Kebutuhan PTE dan Alokasi SEGMENT ASLI */
    TotalPages = (ImageSectionObject->ImageInformation.ImageFileSize + PAGE_SIZE - 1) >> PAGE_SHIFT;
    SegmentSize = sizeof(SEGMENT) + (TotalPages * sizeof(PAGE_ENTRY));

    Segment = (PSEGMENT)UlAllocatePoolWithTag(NonPagedPool, SegmentSize, 'SgMm');
    if (!Segment)
    {
        if (ImageSectionObject->Segments) UlFreePool(ImageSectionObject->Segments);
        UlFreePool(ImageSectionObject);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(Segment, SegmentSize);

    /* 4. Alokasi Control Area ASLI */
    ControlArea = (PCONTROL_AREA)UlAllocatePoolWithTag(
        NonPagedPool, sizeof(CONTROL_AREA) + sizeof(SUBSECTION), 'iCmM'
    );
    if (!ControlArea)
    {
        if (ImageSectionObject->Segments) UlFreePool(ImageSectionObject->Segments);
        UlFreePool(ImageSectionObject);
        UlFreePool(Segment);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(ControlArea, sizeof(CONTROL_AREA) + sizeof(SUBSECTION));

    /* 5. Tautkan Berdasarkan Hierarki Windows NT Sejati */
    ControlArea->Segment = Segment;
    ControlArea->u.Flags.Image = 1;
    ControlArea->u.Flags.File = 1;
    ControlArea->FilePointer = FileObject;
    ControlArea->NumberOfSectionReferences = 1; // Dihitung dari Section Utama

    Segment->ControlArea = ControlArea;
    Segment->SizeOfSegment = ImageSectionObject->ImageInformation.ImageFileSize;
    Segment->TotalNumberOfPtes = TotalPages;
    
    /* MAGIC NT: Tautkan Image Information TANPA cast berbahaya! */
    Segment->u2.ImageInformation = &ImageSectionObject->ImageInformation;

    /* Tambah Reference Count pada FileObject agar aman dari Use-After-Free */
    ObReferenceObject(FileObject);

    Status = ObCreateObject(
        KernelMode, MmSectionObjectType, ObjectAttributes, KernelMode,
        NULL, sizeof(SECTION), 0, 0, (PVOID*)&Section
    );

    if (!VEA_SUCCESS(Status))
    {
        if (ImageSectionObject->Segments) UlFreePool(ImageSectionObject->Segments);
        UlFreePool(ImageSectionObject);
        UlFreePool(Segment);
        UlFreePool(ControlArea);
        ObDereferenceObject(FileObject);
        return Status;
    }

    RtlZeroMemory(Section, sizeof(SECTION));
    Section->SizeOfSection.QuadPart = Segment->SizeOfSegment;
    Section->InitialPageProtection = SectionPageProtection;
    
    /* 100% AMAN: PSEGMENT ke PSEGMENT (Bukan cast paksaan lagi) */
    Section->Segment = Segment; 
    Section->Flags.Image = 1;

    *SectionObject = (PVOID)Section;
    return STATUS_SUCCESS;
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

        if (AllocationAttributes & SEC_IMAGE)
        {
            Status = MmCreateImageSection(
                SectionObject,
                DesiredAccess,
                ObjectAttributes,
                InputMaximumSize,
                SectionPageProtection,
                AllocationAttributes,
                File
            );

            if (FileHandle && File)
            {
                ObDereferenceObject(File);
            }

            return Status;
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
    if (AllocationAttributes & SEC_LARGE_PAGES)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (!FileHandle && !FileObject && (AllocationAttributes & SEC_IMAGE))
    {
        return STATUS_INVALID_FILE_FOR_SECTION;
    }

    /* Teruskan langsung ke MmCreateArm3Section */
    return MmCreateArm3Section(
        SectionObject,
        DesiredAccess,
        ObjectAttributes,
        MaximumSize,
        SectionPageProtection,
        AllocationAttributes,
        FileHandle,
        FileObject
    );
}

VEASTATUS
VEAPI
MmMapViewOfSegment(
    IN PEPROCESS Process,
    IN PSEGMENT Segment,
    IN OUT PVOID *BaseAddress,
    IN SIZE_T ViewSize,
    IN ULONG Protect,
    IN ULONGLONG ViewOffset
)
{
    PCONTROL_AREA ControlArea = Segment ? Segment->ControlArea : NULL;
    PMM_VAD Vad = NULL;
    ULONG PageCount;
    ULONG_PTR TargetVa;
    KIRQL OldIrql;
    VEASTATUS Status = STATUS_SUCCESS;

    DPRINT("[MM/SEG] MmMapViewOfSegment Entry: Process=%p, Segment=%p, ControlArea=%p, ViewSize=%u, Protect=0x%X, ViewOffset=%I64u\n",
           Process, Segment, ControlArea, ViewSize, Protect, ViewOffset);

    if (!Segment || !BaseAddress || ViewSize == 0)
    {
        DPRINT("[MM/SEG] ERROR: Invalid Parameter! Segment=%p, BaseAddress=%p, ViewSize=%u\n",
               Segment, BaseAddress, ViewSize);
        return STATUS_INVALID_PARAMETER;
    }

    ViewSize = PAGE_ROUND_UP(ViewSize);
    PageCount = (ULONG)(ViewSize >> PAGE_SHIFT);
    TargetVa = (ULONG_PTR)*BaseAddress;

    DPRINT("[MM/SEG] TargetVa Input=0x%08X, Rounded ViewSize=%u (%u pages)\n",
           TargetVa, ViewSize, PageCount);

    if (TargetVa != 0)
    {
        TargetVa = PAGE_ALIGN_DOWN(TargetVa);
    }

    if (TargetVa == 0)
    {
        TargetVa = MmAllocateVirtualRange(PageCount);
        if (TargetVa == 0)
        {
            DPRINT("[MM/SEG] ERROR: MmAllocateVirtualRange returned 0 for %u pages!\n", PageCount);
            return STATUS_NO_MEMORY;
        }

        DPRINT("[MM/SEG] Allocated Virtual Range: TargetVa=0x%08X (%u pages)\n", TargetVa, PageCount);

        /* Hubungkan VAD teralokasi ke ControlArea & Segment */
        KsAcquireSpinLock(&MmVmmLock, &OldIrql);
        Vad = MmpFindVadByVpn(MmKernelVadTree.Root, TargetVa >> PAGE_SHIFT);
        if (Vad)
        {
            Vad->ControlArea = ControlArea;
            Vad->ViewOffset  = ViewOffset;
            Vad->Protection  = Protect;
            DPRINT("[MM/SEG] VAD found and assigned: VPN=0x%X, ControlArea=%p\n",
                   Vad->StartingVpn, ControlArea);
        }
        else
        {
            DPRINT("[MM/SEG] ERROR: MmpFindVadByVpn returned NULL for VPN 0x%X!\n",
                   TargetVa >> PAGE_SHIFT);
        }
        KsReleaseSpinLock(&MmVmmLock, OldIrql);

        if (!Vad) return STATUS_NO_MEMORY;
    }
    else
    {
        DPRINT("[MM/SEG] TargetVa specified (0x%08X), allocating manual VAD node\n", TargetVa);

        Vad = (PMM_VAD)UlAllocatePoolWithTag(NonPagedPool, sizeof(MM_VAD), 'daVM');
        if (!Vad)
        {
            DPRINT("[MM/SEG] ERROR: Failed to allocate pool for MM_VAD!\n");
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(Vad, sizeof(MM_VAD));
        Vad->StartingVpn = TargetVa >> PAGE_SHIFT;
        Vad->EndingVpn = Vad->StartingVpn + PageCount - 1;
        Vad->ControlArea = ControlArea;
        Vad->ViewOffset = ViewOffset;
        Vad->Protection = Protect;

        KsAcquireSpinLock(&MmVmmLock, &OldIrql);
        RtlInsertElementAvl(&MmKernelVadTree, (PRTL_BALANCED_NODE)Vad);
        KsReleaseSpinLock(&MmVmmLock, OldIrql);

        DPRINT("[MM/SEG] Manual VAD inserted to AVL tree: StartVPN=0x%X, EndVPN=0x%X\n",
               Vad->StartingVpn, Vad->EndingVpn);
    }

    /* Update reference counter ControlArea */
    if (ControlArea)
    {
        InterlockedIncrement((PLONG)&ControlArea->NumberOfMappedViews);
        InterlockedIncrement((PLONG)&ControlArea->NumberOfSectionReferences);
        DPRINT("[MM/SEG] ControlArea reference count updated (MappedViews=%d)\n",
               ControlArea->NumberOfMappedViews);
    }

    *BaseAddress = (PVOID)TargetVa;
    DPRINT("[MM/SEG] MmMapViewOfSegment SUCCESS: Final BaseAddress=0x%08X\n", TargetVa);
    return STATUS_SUCCESS;
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
    PSEGMENT Segment;
    VEASTATUS Status = STATUS_SUCCESS;
    BOOLEAN IsAttached = FALSE;
    ULONGLONG ViewOffset = 0;
    ULONG_PTR ImageBase = 0;
    SIZE_T ImageSize = 0;

    DPRINT("[MM] MmMapViewOfSection Entry: Section=%p, Process=%p, BaseAddress=%p (*Base=%p), ViewSize=%p (*Size=%u), Protect=0x%X\n",
           Section, Process, BaseAddress, BaseAddress ? *BaseAddress : NULL, ViewSize, ViewSize ? (ULONG)*ViewSize : 0, Protect);

    if (!Section || !Process || !BaseAddress || !ViewSize)
    {
        DPRINT("[MM] ERROR: Invalid Parameter! Section=%p, Process=%p, BaseAddress=%p, ViewSize=%p\n",
               Section, Process, BaseAddress, ViewSize);
        return STATUS_INVALID_PARAMETER;
    }

    if (!Protect || MiMakeProtectionMask(Protect) == MM_INVALID_PROTECTION)
    {
        DPRINT("[MM] ERROR: Invalid Page Protection 0x%X!\n", Protect);
        return STATUS_INVALID_PAGE_PROTECTION;
    }

    if ((PEPROCESS)PtGetCurrentProcess() != Process)
    {
        PtAttachProcess((PKPROCESS)Process);
        IsAttached = TRUE;
    }

    Segment = Section->Segment;

    /* -----------------------------------------------------------------
     * JALUR A: MAPPING ELF IMAGE SECTION (SEC_IMAGE)
     * ----------------------------------------------------------------- */
    if (Section->Flags.Image && Segment)
    {
        PSECTION_IMAGE_INFORMATION ImageInfo = Segment->u2.ImageInformation;
        
        if (ImageInfo && ImageInfo->ImageFileSize > 0)
        {
            ImageSize = ImageInfo->ImageFileSize;
        }
        else
        {
            /* Fallback jika ImageInfo kosong */
            ImageSize = (SIZE_T)Section->SizeOfSection.QuadPart;
        }

        ImageBase = (ULONG_PTR)*BaseAddress;

        DPRINT("[MM] SEC_IMAGE Path: ImageSize=%u, RequestedBase=0x%08X\n",
               ImageSize, ImageBase);

        /* Map segmen image via MmMapViewOfSegment. 
         * Note: Proteksi per-segmen (text/data/bss) dihandle di dalam loader image. */
        Status = MmMapViewOfSegment(
            Process,
            Segment,
            (PVOID*)&ImageBase,
            ImageSize,
            Protect,
            0
        );

        if (VEA_SUCCESS(Status))
        {
            *BaseAddress = (PVOID)ImageBase;
            *ViewSize = ImageSize;

            DPRINT("[MM] Image Section 0x%p Mapped into Process '%s' at VA 0x%08X (Size: %u bytes)\n",
                   Section, Process->ImageFileName, ImageBase, ImageSize);
        }
        else
        {
            DPRINT("[MM] ERROR: MmMapViewOfSegment FAILED for SEC_IMAGE with Status=0x%08X\n", Status);
        }

        goto Exit;
    }

    /* -----------------------------------------------------------------
     * JALUR B: MAPPING DATA SECTION BIASA / ANONYMOUS
     * ----------------------------------------------------------------- */

    /* Validasi Hak Akses khusus Data Section */
    if ((Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) &&
        !(Section->InitialPageProtection & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)))
    {
        DPRINT("[MM] ERROR: Write Protection mismatch! Requested=0x%X, Initial=0x%X\n",
               Protect, Section->InitialPageProtection);
        Status = STATUS_SECTION_PROTECTION;
        goto Exit;
    }

    if ((Protect & MI_SECTION_CAN_READ) &&
        !(Section->InitialPageProtection & MI_SECTION_CAN_READ))
    {
        DPRINT("[MM] ERROR: Read Protection mismatch! Requested=0x%X, Initial=0x%X\n",
               Protect, Section->InitialPageProtection);
        Status = STATUS_SECTION_PROTECTION;
        goto Exit;
    }

    if ((Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) &&
        !(Section->InitialPageProtection & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
    {
        DPRINT("[MM] ERROR: Execute Protection mismatch! Requested=0x%X, Initial=0x%X\n",
               Protect, Section->InitialPageProtection);
        Status = STATUS_SECTION_PROTECTION;
        goto Exit;
    }

    if (SectionOffset)
    {
        SectionOffset->QuadPart &= ~(PAGE_SIZE - 1);
        ViewOffset = (ULONGLONG)SectionOffset->QuadPart;
    }

    /* Check Overflow */
    if ((ViewOffset + *ViewSize) < ViewOffset)
    {
        DPRINT("[MM] ERROR: ViewOffset + ViewSize Overflow! ViewOffset=%I64u, ViewSize=%u\n",
               ViewOffset, *ViewSize);
        Status = STATUS_INVALID_VIEW_SIZE;
        goto Exit;
    }

    /* Auto-calculate ViewSize jika caller meminta 0 */
    if (*ViewSize == 0)
    {
        if ((ULONGLONG)Section->SizeOfSection.QuadPart <= ViewOffset)
        {
            DPRINT("[MM] ERROR: SizeOfSection (%I64u) <= ViewOffset (%I64u)!\n",
                   Section->SizeOfSection.QuadPart, ViewOffset);
            Status = STATUS_INVALID_VIEW_SIZE;
            goto Exit;
        }

        *ViewSize = (SIZE_T)(Section->SizeOfSection.QuadPart - ViewOffset);
    }

    /* Check Boundary terhadap ukuran Section */
    if ((ViewOffset + *ViewSize) > (ULONGLONG)Section->SizeOfSection.QuadPart)
    {
        DPRINT("[MM] ERROR: ViewOffset + ViewSize (%I64u) > SizeOfSection (%I64u)!\n",
               ViewOffset + *ViewSize, Section->SizeOfSection.QuadPart);
        Status = STATUS_INVALID_VIEW_SIZE;
        goto Exit;
    }

    DPRINT("[MM] DATA_SECTION Path: ViewSize=%u, ViewOffset=%I64u, RequestedBase=%p\n",
           *ViewSize, ViewOffset, *BaseAddress);

    /* Delegasikan alokasi VAD dan mapping data segment */
    Status = MmMapViewOfSegment(
        Process,
        Segment,
        BaseAddress,
        *ViewSize,
        Protect,
        ViewOffset
    );

    if (VEA_SUCCESS(Status))
    {
        DPRINT("[MM] Data Section 0x%p Mapped into Process '%s' at VA 0x%08X (Size: %u bytes)\n",
               Section, Process->ImageFileName, *BaseAddress, *ViewSize);
    }
    else
    {
        DPRINT("[MM] ERROR: MmMapViewOfSegment FAILED for DATA_SECTION with Status=0x%08X\n", Status);
    }

Exit:
    DPRINT("[MM] MmMapViewOfSection Exit with Status=0x%08X\n", Status);

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

VEASTATUS
VEAPI
VeaCreateSection(OUT PHANDLE SectionHandle,
                IN ACCESS_MASK DesiredAccess,
                IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
                IN PLARGE_INTEGER MaximumSize OPTIONAL,
                IN ULONG SectionPageProtection OPTIONAL,
                IN ULONG AllocationAttributes,
                IN HANDLE FileHandle OPTIONAL)
{
    PVOID SectionObject = NULL;
    HANDLE LocalHandle = NULL;
    VEASTATUS Status;
    LARGE_INTEGER SafeMaximumSize;
    PLARGE_INTEGER SectionSizePtr = NULL;
    KPROCESSOR_MODE AccessMode = KeGetPreviousMode();

    /* 1. Validasi Pointer Handle Output */
    if (!SectionHandle)
    {
        return STATUS_INVALID_PARAMETER;
    }
    if (MaximumSize)
    {
        // Salin ke stack local agar aman jika dipanggil dari UserMode
        SafeMaximumSize = *MaximumSize;
        SectionSizePtr = &SafeMaximumSize;
    }

    Status = MmCreateSection(
        &SectionObject,
        DesiredAccess,
        ObjectAttributes,
        SectionSizePtr,
        SectionPageProtection,
        AllocationAttributes,
        FileHandle,
        NULL
    );

    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* 4. Tanam Objek Section ke Handle Table Proses & Dapatkan Handle */
    Status = ObInsertObject(
        SectionObject,
        NULL,
        DesiredAccess,
        0,
        NULL,
        &LocalHandle
    );

    if (VEA_SUCCESS(Status))
    {
        *SectionHandle = LocalHandle;
    }
    else
    {
        ObDereferenceObject(SectionObject);
    }

    return Status;
}

/* EOF */