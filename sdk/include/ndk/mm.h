#pragma once

#include "mmtype.h"
#include "ldrtypes.h"
#include "pttypes.h"

/* Easier access to PD and PT */
#define MM_PT_VIRTUAL_BASE  0xFFC00000
#define MM_PD_VIRTUAL_BASE  0xFFFFF000

#define MI_PTE_BASE   ((PMMPTE)MM_PT_VIRTUAL_BASE)
#define MI_PDE_BASE   ((PMMPDE)MM_PD_VIRTUAL_BASE)

#define MiAddressToPte(Va)   (MI_PTE_BASE + ((ULONG_PTR)(Va) >> 12))
#define MiAddressToPde(Va)   (MI_PDE_BASE + ((ULONG_PTR)(Va) >> 22))
#define MiPteToAddress(Pte)  ((PVOID)((ULONG_PTR)(Pte) << 10))
#define MiPteToPde(Pte)      ((PMMPDE)MiAddressToPte(Pte))   /* PD self-mapped */
#define MiIsPteOnPdeBoundary(Pte) \
    ((((ULONG_PTR)(Pte)) & (PAGE_SIZE - 1)) == 0)
    
#define MM_GET_PD()       MI_PDE_BASE
#define MM_GET_PDE(Va)    MiAddressToPde(Va)
#define MM_GET_PTE(Va)    MiAddressToPte(Va)

#define MI_WRITE_VALID_PTE(Ptr, Tmpl) \
    (((volatile MMPTE *)(Ptr))->Long = (Tmpl).Long)

#define MI_WRITE_INVALID_PTE(Ptr, Tmpl) \
    (((volatile MMPTE *)(Ptr))->Long = (Tmpl).Long)

#define MI_ERASE_PTE(Ptr) \
    (((volatile MMPTE *)(Ptr))->Long = 0)

#define MI_MAKE_VALID_PTE(Tmpl, Pfn, _Owner, Writable) do { \
    (Tmpl).Long = 0;                                     \
    (Tmpl).Hard.Valid = 1;                               \
    (Tmpl).Hard.Write = (Writable);                      \
    (Tmpl).Hard.Owner = (_Owner);                         \
    (Tmpl).Hard.PageFrameNumber = (Pfn);                 \
} while (0)

#define MMI_WRITE_PTE_VALID(Ptr, Pfn) \
    (*(volatile ULONG *)(Ptr) = ((ULONG)(Pfn) << 12) | 0x3)

#define MI_PDE_FLAGS_FOR(Va) \
    (((ULONG_PTR)(Va) <= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS) ? 0x7 : 0x3)
#define MI_WRITE_PDE_VALID_VA(Pde, Pfn, Va) \
    (*(volatile ULONG *)(Pde) = ((ULONG)(Pfn) << 12) | MI_PDE_FLAGS_FOR(Va))
#define MI_WRITE_PDE_VALID(Pde, Pfn)  MMI_WRITE_PTE_VALID(Pde, Pfn)

#define MMI_INVALIDATE_TLB(Va) \
    __asm__ volatile("invlpg (%0)" :: "r"((ULONG_PTR)(Va)) : "memory")

#define MI_IS_USER_ADDRESS(Va) \
    ((ULONG_PTR)(Va) <= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)

#define MM_HIGHEST_USER_ADDRESS  ((PVOID)0xBFFEFFFF)   /* sesuaikan layout */
#define MM_HIGHEST_VAD_ADDRESS   ((PVOID)((ULONG_PTR)MM_HIGHEST_USER_ADDRESS - _64K))
#define MM_VIRTMEM_GRANULARITY   _64K
#define MI_MAX_ZERO_BITS         21
#define ROUND_TO_PAGES(x)        PAGE_ROUND_UP(x)
#define MM_LOWEST_USER_VPN     (0x00010000 >> PAGE_SHIFT)
#define MM_HIGHEST_USER_VPN    (((ULONG)(ULONG_PTR)MM_HIGHEST_USER_ADDRESS) >> PAGE_SHIFT)
#define MM_VPN_IS_USER(Vpn)    ((ULONG)(Vpn) <= MM_HIGHEST_USER_VPN)
#define MM_VAD_ALIGN_64K       (0x10000 >> PAGE_SHIFT)

#define MM_IS_SYSTEM_PROCESS(Process) \
    ((Process) == NULL || (Process) == (PEPROCESS)PtSystemProcess)


/* FUNCTION */
BOOLEAN VEAPI MmInitialize(PBLOCK_BOOT_2 BlockBoot);
BOOLEAN VEAPI MmInitializePip(PBLOCK_BOOT_2 BlockBoot);
BOOLEAN VEAPI MmMapPip(VOID);
BOOLEAN 
VEAPI 
MmInitializeVmm(VOID);

BOOLEAN
VEAPI
MmInitializeKernelPageDirectory(VOID);

BOOLEAN
VEAPI
MmpSynchronizeKernelPageTable(ULONG VirtualAddress);

BOOLEAN
VEAPI
MmpCopyKernelPageDirectory(PULONG Destination);

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

VOID
VEAPI
MmpInitializePageFaultReadAhead(VOID);

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

#define PAGE_ALIGN_DOWN(va) ((ULONG_PTR)(va) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(va)   (((ULONG_PTR)(va) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

VOID
VEAPI
MiInsertBasedSection(IN PSECTION Section);

VEASTATUS
VEAPI
MiFindEmptyAddressRangeDownBasedTree(
    IN SIZE_T Size,
    IN ULONG_PTR HighestAddress,
    IN ULONG Alignment,
    IN PRTL_AVL_TREE Tree,
    OUT ULONG_PTR *BaseAddress
);

LONG VEAPI MmpCompareBasedNode(PRTL_BALANCED_NODE NodeA, PRTL_BALANCED_NODE NodeB);

VOID
VEAPI
MmDereferenceSegmentWithLock(
    IN PMM_SECTION_SEGMENT Segment,
    IN KIRQL OldIrql
);

/* THIS IS HACK */
#define MiAcquirePfnLock() KsAcquireDispatcherLock()
#define MiReleasePfnLock(_Irql) KsReleaseDispatcherLock(_Irql)

#define PAGE_ROUND_DOWN(x)                                  \
    (((ULONG_PTR)(x))&(~(PAGE_SIZE-1)))
#define PAGE_ROUND_UP(x)                                    \
    ( (((ULONG_PTR)(x)) + PAGE_SIZE-1)  & (~(PAGE_SIZE-1)) )

BOOLEAN
VEAPI
MmpEnsurePageTable(ULONG VirtualAddress);

PMM_VAD 
VEAPI
MmpFindVadByVpn(PRTL_BALANCED_NODE Node, ULONG Vpn);

extern PMM_VAD MmVadRoot;

VOID VEAPI MmpFreeVadNode(PMM_VAD VadNode);

VEASTATUS
VEAPI
MmUnmapViewOfSection(
    IN PEPROCESS Process,
    IN PVOID BaseAddress
);

VEASTATUS
VEAPI
VeaCreateSection(OUT PHANDLE SectionHandle,
                IN ACCESS_MASK DesiredAccess,
                IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
                IN PLARGE_INTEGER MaximumSize OPTIONAL,
                IN ULONG SectionPageProtection OPTIONAL,
                IN ULONG AllocationAttributes,
                IN HANDLE FileHandle OPTIONAL);

VEASTATUS
VEAPI
MmLoadSystemImage(
    IN PANSI_STRING FileName,
    IN PANSI_STRING NamePrefix OPTIONAL,
    IN PANSI_STRING LoadedName OPTIONAL,
    IN ULONG Flags,
    OUT PVOID *ModuleObject,
    OUT PVOID *ImageBaseAddress
);

#define MI_SECTION_CAN_READ \
    (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | \
     PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)

VOID
MiDumpMemory(
    IN PVOID BaseAddress,
    IN ULONG Length
);

VOID
VEAPI
MiCheckControlArea(
    IN PCONTROL_AREA ControlArea,
    IN KIRQL OldIrql
);

VEASTATUS
VEAPI
MmCheckSystemImage(IN HANDLE FileHandle);

extern POBJECT_TYPE MmSectionObjectType;

VOID
VEAPI
MmProbeAndLockPages(
    PMDL Mdl,
    KPROCESSOR_MODE AccessMode,
    LOCK_OPERATION Operation
);

VOID
VEAPI
MmDeleteTeb(
    IN PEPROCESS Process,
    IN PTEB TebBase
);

VEASTATUS
VEAPI
MmCreateTeb(
    IN PEPROCESS Process,
    IN PCLIENT_ID ClientId,
    IN PINITIAL_TEB InitialTeb OPTIONAL,
    OUT PTEB *TebBase
);

VOID
VEAPI
MmDeletePeb(
    IN PEPROCESS Process,
    IN PPEB PebBase
);

VEASTATUS
VEAPI
MmCreatePeb(
    IN PEPROCESS Process,
    IN PINITIAL_PEB InitialPeb OPTIONAL,
    OUT PPEB *PebBase
);

BOOLEAN
VEAPI
MmCreateProcessAddressSpace(
    IN PEPROCESS Process,
    OUT PULONG_PTR DirectoryTableBase
);

VEASTATUS
VEAPI
MiInitializeHandBuiltProcess(
    IN PEPROCESS Process,
    OUT PULONG_PTR DirectoryTableBase
);

VEASTATUS
VEAPI
MmInitializeProcessAddressSpace(
    IN PEPROCESS Process,
    IN PVOID SectionObject OPTIONAL,
    OUT PCHAR ImageFileName OPTIONAL
);

VOID
VEAPI
MmpInitializeProcessVadTree(IN PEPROCESS Process);

PRTL_AVL_TREE
VEAPI
MmpGetVadTree(IN PEPROCESS Process, IN ULONG Vpn, OUT PKSPIN_LOCK *Lock);

PMM_VAD
VEAPI
MmpFindOverlappingVad(IN PRTL_AVL_TREE Tree, IN ULONG StartVpn, IN ULONG EndVpn);

VEASTATUS
VEAPI
MmpInsertVadAnywhere(IN PRTL_AVL_TREE Tree, IN PKSPIN_LOCK Lock,
                     IN PMM_VAD Vad, IN ULONG PageCount,
                     IN ULONG LowVpn, IN ULONG HighVpnExclusive,
                     IN ULONG AlignPages);


VEASTATUS
VEAPI
MmpInsertVadAt(IN PRTL_AVL_TREE Tree, IN PKSPIN_LOCK Lock, IN PMM_VAD Vad);

PMM_VAD VEAPI MmpAllocateVadNode(VOID);

ULONG
VEAPI
MiMakeProtectionMask(IN ULONG Protect);

VEASTATUS
VEAPI
VeaAllocateVirtualMemory(
    IN HANDLE ProcessHandle,
    IN OUT PVOID *UBaseAddress,
    IN ULONG_PTR ZeroBits,
    IN OUT PSIZE_T URegionSize,
    IN ULONG AllocationType,
    IN ULONG Protect
);

VEASTATUS
VEAPI
VeaFreeVirtualMemory(
    IN HANDLE ProcessHandle,
    IN OUT PVOID *UBaseAddress,
    IN OUT PSIZE_T URegionSize,
    IN ULONG FreeType
);

VEASTATUS
VEAPI
VeaReadVirtualMemory(
    IN HANDLE ProcessHandle,
    IN PVOID BaseAddress,
    OUT PVOID Buffer,
    IN SIZE_T NumberOfBytesToRead,
    OUT PSIZE_T NumberOfBytesRead OPTIONAL
);

VEASTATUS
VEAPI
VeaWriteVirtualMemory(
    IN HANDLE ProcessHandle,
    IN PVOID BaseAddress,
    IN PVOID Buffer,
    IN SIZE_T NumberOfBytesToWrite,
    OUT PSIZE_T NumberOfBytesWritten OPTIONAL
);

VEASTATUS
VEAPI
VeaQuerySection(
    IN HANDLE SectionHandle,
    IN SECTION_INFORMATION_CLASS SectionInformationClass,
    OUT PVOID SectionInformation,
    IN SIZE_T SectionInformationLength,
    OUT PSIZE_T ResultLength OPTIONAL
);

VEASTATUS
VEAPI
VeaMapViewOfSection(
    IN HANDLE SectionHandle,
    IN HANDLE ProcessHandle,
    IN OUT PVOID *BaseAddress,
    IN ULONG_PTR ZeroBits,
    IN SIZE_T CommitSize,
    IN OUT PLARGE_INTEGER SectionOffset OPTIONAL,
    IN OUT PSIZE_T ViewSize,
    IN SECTION_INHERIT InheritDisposition,
    IN ULONG AllocationType,
    IN ULONG Win32Protect
);

VEASTATUS
VEAPI
VeaUnmapViewOfSection(
    IN HANDLE ProcessHandle,
    IN PVOID BaseAddress
);

VOID
VEAPI
MmUnlockPages(
    PMDL Mdl
);

PVOID
VEAPI
MmCreateKernelStack(
    IN BOOLEAN GuiStack,
    IN UCHAR Node
);

VOID
VEAPI
MmDeleteKernelStack(
    IN PVOID StackBase,
    IN BOOLEAN GuiStack
);

VOID
VEAPI
MmDeleteProcessAddressSpace(
    IN PEPROCESS Process
);

/* EOF */