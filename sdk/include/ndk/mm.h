#pragma once

#include "mmtype.h"
#include "ldrtypes.h"
#include "pttypes.h"

/* Easier access to PD and PT */
#define MM_PT_VIRTUAL_BASE  0xFFC00000
#define MM_PD_VIRTUAL_BASE  0xFFFFF000

#define MM_GET_PD() \
    ((PAGE_ENTRY *)MM_PD_VIRTUAL_BASE)

#define MM_GET_PDE(VirtualAddress) \
    (&MM_GET_PD()[(ULONG)(VirtualAddress) >> 22])

#define MM_GET_PTE(VirtualAddress) \
    (&((PAGE_ENTRY *)MM_PT_VIRTUAL_BASE)[(ULONG)(VirtualAddress) >> 12])

#define MMI_WRITE_PTE_VALID(PtePtr, Pfn) do { \
    (PtePtr)->bits.frame   = (Pfn);           \
    (PtePtr)->bits.present = 1;               \
    (PtePtr)->bits.rw      = 1;               \
    (PtePtr)->bits.user    = 0;               \
} while(0)

#define MMI_WRITE_PDE_VALID(PdePtr, Pfn) \
    MMI_WRITE_PTE_VALID(PdePtr, Pfn)

#define MMI_INVALIDATE_TLB(VA) \
    asm volatile("invlpg (%0)" ::"r"(VA) : "memory")
    
#define MI_WRITE_INVALID_PTE(PointerPte) (*(ULONG*)(PointerPte) = 0)

/* FUNCTION */
BOOLEAN VEAPI MmInitialize(PBLOCK_BOOT_2 BlockBoot);
BOOLEAN VEAPI MmInitializePip(PBLOCK_BOOT_2 BlockBoot);
BOOLEAN VEAPI MmMapPip(VOID);
BOOLEAN 
VEAPI 
MmInitializeVmm(VOID);

/* EXTERNATION */
extern PPIP MmPipDatabase;
extern ULONG MmTotalPages;
extern ULONG MmFirstFreePip;

/* PIP Function for allocation */
ULONG
VEAPI
MmAllocateContiguousPhysicalPages(ULONG PageCount);

VOID 
VEAPI 
MmFreeContiguousPhysicalPages(ULONG BasePhysicalAddress, ULONG PageCount);

VOID 
VEAPI 
MmFreePhysicalPage(ULONG PhysicalAddress);

ULONG 
VEAPI
MmAllocatePhysicalPage(VOID);

/* Allocatin berbasis AVL TREE untuk Virtual Allocation */
ULONG 
VEAPI 
MmAllocateVirtualRange(ULONG PageCount);

VOID 
VEAPI 
MmFreeVirtualRange(ULONG VirtualAddress);

ULONG
VEAPI
MmGetVirtualRangePageCount(ULONG VirtualAddress);

/* MmAllocationPool */
PVOID
VEAPI
MmAllocatePoolPages(POOL_TYPE PoolType, ULONG NumberOfPages);

PVOID
VEAPI
MmAllocatePoolPage(POOL_TYPE PoolType);

VOID
VEAPI
MmFreePoolPages(PVOID BaseAddress, ULONG NumberOfPages);

VOID
VEAPI
MmFreePoolPage(PVOID BaseAddress);

PVOID
VEAPI
MmAllocateContiguousMemory(
    IN SIZE_T NumberOfBytes,
    IN PHYSICAL_ADDRESS HighestAcceptableAddress
);

VOID
VEAPI
MmFreeContiguousMemory(
    IN PVOID BaseAddress
);

PHYSICAL_ADDRESS
VEAPI
MmGetPhysicalAddress(
    IN PVOID BaseAddress
);

/* UlPool */
VOID 
VEAPI 
UlInitializePools(VOID);

PVOID 
VEAPI 
UlAllocatePoolWithTag(POOL_TYPE PoolType, ULONG NumberOfBytes, ULONG Tag) ;

VOID 
VEAPI 
UlFreePoolWithTag(PVOID P, ULONG Tag);

VOID
VEAPI
UlFreePool(PVOID P);

PVOID 
VEAPI 
UlAllocatePoolZero(POOL_TYPE PoolType, ULONG NumberOfBytes, ULONG Tag);

/* Mapping IO */
VOID
VEAPI
MmUnmapIoSpace(
    PVOID BaseAddress,
    ULONG NumberOfBytes
);

PVOID
VEAPI
MmMapIoSpace(
    ULONG PhysicalAddress,
    ULONG NumberOfBytes,
    MEMORY_CACHING_TYPE CacheType
);

// macro
#define POOL_PREV_BLOCK(Entry) ((PPOOL_HEADER)(Entry) - (Entry)->PreviousSize)
#define POOL_NEXT_BLOCK(Entry) ((PPOOL_HEADER)(Entry) + (Entry)->BlockSize)
#define PAGE_ALIGN(Va)         ((PVOID)((ULONG_PTR)(Va) & ~(POOL_PAGE_SIZE - 1)))

#define PAGE_SIZE 0x1000
#define PAGE_SHIFT 12L

PVOID
VEAPI
MiGetSystemPageDirectoryTableBase(VOID);

VOID
VEAPI
MmpInitializeCowScratch(VOID);

BOOLEAN 
VEAPI
MmAccessFault(
    ULONG FaultingAddress, 
    BOOLEAN WriteFault, 
    BOOLEAN WasPresent
);

#define ADDRESS_AND_SIZE_TO_SPAN_PAGES(_Va, _Size) \
  ((ULONG) ((((ULONG_PTR) (_Va) & (PAGE_SIZE - 1)) \
    + (_Size) + (PAGE_SIZE - 1)) >> PAGE_SHIFT))

#define COMPUTE_PAGES_SPANNED(Va, Size) \
    ADDRESS_AND_SIZE_TO_SPAN_PAGES(Va,Size)

MM_SYSTEMSIZE VEAPI MmQuerySystemSize(VOID);

BOOLEAN
VEAPI
MmpEnsurePageTable(ULONG Va);

ULONG
VEAPI
MmGetPageShareCount(ULONG PhysicalAddress);

VOID
VEAPI
MmDereferencePhysicalPage(ULONG PhysicalAddress);

VOID
VEAPI
MmReferencePhysicalPage(ULONG PhysicalAddress);

VOID
VEAPI
MmInitializeZeroPageThreadEvent(VOID);

VEASTATUS
VEAPI
MmInitializeProcessDirectoryBase(
    IN PEPROCESS Process
);

BOOLEAN
VEAPI
MiInitializePipBitmap(VOID);

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
);

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
);

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
);

VEASTATUS
VEAPI
MmUnmapViewOfSection(
    IN PEPROCESS Process,
    IN PVOID BaseAddress
);

VOID
VEAPI
MmInitSectionImplementation(VOID);
