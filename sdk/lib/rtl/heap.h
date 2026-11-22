// RTL HEAP HEADER
//
//

#pragma once

#include <procbind.h>
#include <veastatus.h>
#include "rtl2.h"

#define HEAP_HARD_MAX_ALLOC     0x7FFFFFF0UL
#define HEAP_ALLOC_VALID_MASK   (HEAP_NO_SERIALIZE | HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY)

#define HEAP_SIGNATURE              0x50414548UL    /* 'HEAP' */
#define HEAP_SEGMENT_SIGNATURE      0x47455348UL    /* 'HSEG' */

#define HEAP_GRANULARITY            8UL
#define HEAP_GRANULARITY_SHIFT      3UL
#define HEAP_MIN_BLOCK_UNITS        2UL             /* HEAP_FREE_ENTRY = 16 byte */
#define HEAP_MAX_BLOCK_UNITS        0xFE00UL        /* 0x7F000 byte */
#define HEAP_FREELIST_COUNT         128UL

#define HEAP_DEFAULT_RESERVE        0x100000UL
#define HEAP_DEFAULT_COMMIT         PAGE_SIZE
#define HEAP_DEFAULT_DECOMMIT_FREE  PAGE_SIZE
#define HEAP_DEFAULT_DECOMMIT_TOTAL 0x10000UL
#define HEAP_DEFAULT_MAX_ALLOC      0x7FFFFFF0UL

#define HEAP_ENTRY_BUSY             0x01
#define HEAP_ENTRY_LAST             0x02
#define HEAP_ENTRY_VIRTUAL          0x04

#define HEAP_INT_USER_MEMORY        0x00000001UL    /* HeapBase dari caller */
#define HEAP_INT_USER_LOCK          0x00000002UL    /* Lock dari caller */

#define HEAP_NO_SERIALIZE               0x00000001UL
#define HEAP_GROWABLE                   0x00000002UL
#define HEAP_GENERATE_EXCEPTIONS        0x00000004UL
#define HEAP_ZERO_MEMORY                0x00000008UL
#define HEAP_REALLOC_IN_PLACE_ONLY      0x00000010UL
#define HEAP_TAIL_CHECKING_ENABLED      0x00000020UL
#define HEAP_FREE_CHECKING_ENABLED      0x00000040UL
#define HEAP_DISABLE_COALESCE_ON_FREE   0x00000080UL

#define HEAP_CREATE_ALIGN_16            0x00010000UL
#define HEAP_CREATE_ENABLE_TRACING      0x00020000UL
#define HEAP_CREATE_ENABLE_EXECUTE      0x00040000UL

#define HEAP_FORCE_MASK \
    (HEAP_NO_SERIALIZE | HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY | \
     HEAP_REALLOC_IN_PLACE_ONLY | HEAP_TAIL_CHECKING_ENABLED | \
     HEAP_FREE_CHECKING_ENABLED | HEAP_DISABLE_COALESCE_ON_FREE)

#define HEAP_CREATE_VALID_MASK \
    (HEAP_FORCE_MASK | HEAP_GROWABLE | HEAP_CREATE_ALIGN_16 | \
     HEAP_CREATE_ENABLE_TRACING | HEAP_CREATE_ENABLE_EXECUTE)

#define HEAP_ALIGN_UP(Value, Align) \
    (((SIZE_T)(Value) + (Align) - 1) & ~((SIZE_T)(Align) - 1))

#pragma pack (push, 8)

typedef struct _HEAP_ENTRY              /* WAJIB 8 byte = 1 unit */
{
    USHORT Size;                        /* satuan 8 byte, termasuk header */
    USHORT PreviousSize;
    UCHAR  SegmentIndex;
    UCHAR  Flags;
    UCHAR  UnusedBytes;
    UCHAR  Cookie;                      /* XOR byte 0..6 */
} HEAP_ENTRY, *PHEAP_ENTRY;

#define HEAP_ENTRY_UPDATE_CHECKSUM(E) \
    ((E)->Cookie = (UCHAR)(((PUCHAR)(E))[0] ^ ((PUCHAR)(E))[1] ^ \
                           ((PUCHAR)(E))[2] ^ ((PUCHAR)(E))[3] ^ \
                           ((PUCHAR)(E))[4] ^ ((PUCHAR)(E))[5] ^ \
                           ((PUCHAR)(E))[6]))

typedef
VEASTATUS
(VEAPI *PRTL_HEAP_COMMIT_ROUTINE)(
    IN PVOID Base,
    IN OUT PVOID *CommitAddress,
    IN OUT PSIZE_T CommitSize
);

typedef struct _HEAP_FREE_ENTRY
{
    HEAP_ENTRY Entry;
    LIST_ENTRY FreeList;
} HEAP_FREE_ENTRY, *PHEAP_FREE_ENTRY;

typedef struct _HEAP_SEGMENT
{
    ULONG        Signature;
    struct _HEAP *Heap;
    PVOID        BaseAddress;
    PHEAP_ENTRY  LastEntry; 
    PVOID        FirstEntry;
    PVOID        CommitEnd;             /* watermark: batas yang sudah di-commit */
    PVOID        ReserveEnd;
    LIST_ENTRY   SegmentListEntry;
} HEAP_SEGMENT, *PHEAP_SEGMENT;

typedef struct _HEAP_VIRTUAL_ALLOC_ENTRY 
{
    LIST_ENTRY Entry;                   /* di Heap->VirtualAllocList */
    SIZE_T     CommitSize;
    SIZE_T     ReserveSize;
    HEAP_ENTRY BusyBlock;               /* user pointer = &BusyBlock + 1 */
} HEAP_VIRTUAL_ALLOC_ENTRY, *PHEAP_VIRTUAL_ALLOC_ENTRY;

typedef struct _HEAP
{
    ULONG                    Signature;
    ULONG                    Flags;
    ULONG                    ForceFlags;
    ULONG                    InternalFlags;
    SIZE_T                   SegmentReserve;
    SIZE_T                   SegmentCommit;
    SIZE_T                   DeCommitFreeBlockThreshold;
    SIZE_T                   DeCommitTotalFreeThreshold;
    SIZE_T                   VirtualMemoryThreshold;    /* byte */
    SIZE_T                   MaximumAllocationSize;
    SIZE_T                   TotalFreeSize;             /* unit 8 byte */
    PRTL_HEAP_COMMIT_ROUTINE CommitRoutine;
    PRTL_CRITICAL_SECTION    Lock;
    RTL_CRITICAL_SECTION     LockObject;
    LIST_ENTRY               SegmentList;
    LIST_ENTRY               VirtualAllocList;
    ULONG                    FreeListBitmap[HEAP_FREELIST_COUNT / 32];
    LIST_ENTRY               FreeLists[HEAP_FREELIST_COUNT];
} HEAP, *PHEAP;

#pragma pack (pop)

typedef VEASTATUS (VEAPI *PRTL_HEAP_COMMIT_ROUTINE)(
    IN PVOID Base,
    IN OUT PVOID *CommitAddress,
    IN OUT PSIZE_T CommitSize
);

typedef struct _RTL_HEAP_PARAMETERS
{
    ULONG Length;                         // Harus diisi sizeof(RTL_HEAP_PARAMETERS) untuk validasi
    SIZE_T SegmentReserve;                // Ukuran Virtual Reserve default untuk segmen baru
    SIZE_T SegmentCommit;                 // Ukuran Virtual Commit default untuk segmen baru
    SIZE_T DeCommitFreeBlockThreshold;    // Batas minimal ukuran blok bebas sebelum di-decommit ke kernel
    SIZE_T DeCommitTotalFreeThreshold;    // Batas akumulasi total memori bebas sebelum trimming
    SIZE_T MaximumAllocationSize;         // Batas maksimum alokasi dari heap (di atas ini return failure)
    SIZE_T VirtualMemoryThreshold;        // Ambang batas alokasi langsung via VzAllocateVirtualMemory
    SIZE_T InitialCommit;                 // Ukuran memori fisik (Commit) awal saat heap dibuat
    SIZE_T InitialReserve;                // Ukuran ruang alamat virtual (Reserve) awal saat heap dibuat
    PRTL_HEAP_COMMIT_ROUTINE CommitRoutine; // Routine kustom untuk commit memori (NULL = default VzAllocate)
    SIZE_T SizeOfHeapReferences;          // Ukuran struktur/array referensi heap internal
} RTL_HEAP_PARAMETERS, *PRTL_HEAP_PARAMETERS;

PVOID VEAPI RtlCreateHeap(IN ULONG Flags, IN PVOID HeapBase OPTIONAL, IN SIZE_T ReserveSize OPTIONAL, IN SIZE_T CommitSize OPTIONAL, IN PVOID Lock OPTIONAL, IN PRTL_HEAP_PARAMETERS Parameters OPTIONAL);
BOOLEAN VEAPI RtlDestroyHeap(IN PVOID HeapHandle);
PVOID VEAPI RtlAllocateHeap(IN PVOID HeapHandle, IN ULONG Flags, IN SIZE_T Size);
BOOLEAN VEAPI RtlFreeHeap(IN PVOID HeapHandle, IN ULONG Flags, IN PVOID BaseAddress);
#define RtlGetProcessHeap() (NtCurrentPeb()->ProcessHeap)