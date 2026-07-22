#pragma once

#include "mmtype.h"
#include "ldrtypes.h"

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
BOOLEAN VEAPI MmInitialize(PBLOCK_BOOT_1 BlockBoot);
BOOLEAN VEAPI MmInitializePip(PBLOCK_BOOT_1 BlockBoot);
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