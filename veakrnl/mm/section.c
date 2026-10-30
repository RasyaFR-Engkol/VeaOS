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
MmInitSectionImplementation(VOID)
{
    OBJECT_TYPE_INITIALIZER ObjectTypeInitializer;

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
MmCreateSection(
    OUT PVOID *Section,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjAttr OPTIONAL,
    IN PLARGE_INTEGER MaximumSize,
    IN ULONG StageProtection,
    IN ULONG AllocationAttributes,
    IN HANDLE FileHandle OPTIONAL,
    IN PFILE_OBJECT FileObject OPTIONAL
)
{
    VEASTATUS Status;
    ULONG Protection;

    UNIMPLEMENTED_DBGBREAK;

    return STATUS_NOT_IMPLEMENTED;
}