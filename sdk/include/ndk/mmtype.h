#pragma once

#include "procbind.h"
#include <rtl.h>

/* PIP Flags */
#define PIP_STATE_FREE      0  // Halaman kosong, siap dialokasikan
#define PIP_STATE_ACTIVE    1  // Halaman sedang dipakai (oleh kernel/user)
#define PIP_STATE_RESERVED  2  // Halaman dipakai BIOS/Hardware (nggak boleh disentuh)
#define PIP_STATE_ZEROED    3  // Halaman kosong dan isinya udah di-nol-kan (aman untuk keamanan)

/* PIP */
typedef struct _PIP
{
    union
    {
        ULONG NextPip;
        ULONG PteAddress;
    } Flink;

    USHORT ReferenceCount;
    UCHAR State;
    UCHAR Flags;
} __attribute__((packed)) PIP, *PPIP;

/* PAGE_ENTRY */
#pragma pack(push, 1)
typedef union _PAGE_ENTRY {
    ULONG raw; // Buat ngisi nol atau math kasar
    struct {
        ULONG present       : 1; // Bit 0: 1 = Halaman ada di RAM
        ULONG rw            : 1; // Bit 1: 0 = Read Only, 1 = Read/Write
        ULONG user          : 1; // Bit 2: 0 = Kernel (Ring 0), 1 = User (Ring 3)
        ULONG write_through : 1; // Bit 3: Caching write-through
        ULONG cache_disable : 1; // Bit 4: Caching disable
        ULONG accessed      : 1; // Bit 5: Diset CPU kalau memori dibaca
        ULONG dirty         : 1; // Bit 6: Diset CPU kalau memori ditulis
        ULONG ps_or_pat     : 1; // Bit 7: Page Size (PDE) / PAT (PTE)
        ULONG global        : 1; // Bit 8: Global page (nggak di-flush dari TLB)
        ULONG available     : 3; // Bit 9-11: Kosong, OS bebas pake buat apa aja!
        ULONG frame         : 20;// Bit 12-31: Physical Address >> 12
    } bits;
} PAGE_ENTRY;
#pragma pack(pop)

typedef struct _MM_VAD {
    RTL_BALANCED_NODE CoreNode; // WAJIB ditaruh di paling atas
    ULONG StartingVpn;
    ULONG EndingVpn;
} MM_VAD, *PMM_VAD;

/* FOR ULPOOL */
#define POOL_BLOCK_SHIFT 3  // 2^3 = 8 bytes (Alignment standar x86)
#define POOL_PAGE_SIZE   4096
#define POOL_MAX_BLOCKS  (POOL_PAGE_SIZE >> POOL_BLOCK_SHIFT) // 512

typedef enum _POOL_TYPE {
    NonPagedPool,
    PagedPool,
    MaxPoolType
} POOL_TYPE;

typedef struct _POOL_HEADER {
    union {
        struct {
            USHORT PreviousSize : 9; // Ukuran blok sebelumnya (buat coalescing)
            USHORT PoolIndex : 7;    // Index untuk halaman pool
            USHORT BlockSize : 9;    // Ukuran blok ini (Max 512 = 1 Page)
            USHORT PoolType : 7;     // NonPaged atau Paged
        };
        ULONG Ulong1;
    };
    ULONG PoolTag; // Tag 4 karakter (misal: 'Thrd', 'File')
} POOL_HEADER, *PPOOL_HEADER;

typedef struct _POOL_FREE_BLOCK {
    POOL_HEADER Header;
    LIST_ENTRY ListEntry;
} POOL_FREE_BLOCK, *PPOOL_FREE_BLOCK;

typedef struct _POOL_DESCRIPTOR {
    POOL_TYPE PoolType;
    ULONG PoolIndex;
    ULONG Threshold;
    PVOID LockAddress; // Nanti diisi pointer ke Spinlock
    
    // Accounting & Stats
    ULONG RunningAllocs;
    ULONG RunningDeAllocs;
    ULONG TotalPages;
    ULONG TotalBytesMapped;    // Total RAM (fisik) yang bener-bener dipesan dari VMM
    ULONG TotalBytesAllocated; // Total RAM yang lagi dipinjam/dipakai sama user/driver
    ULONG TotalBigPages;
    
    // Array List Head Pindah Kesini!
    LIST_ENTRY ListHeads[POOL_MAX_BLOCKS];
} POOL_DESCRIPTOR, *PPOOL_DESCRIPTOR;

/* ULbigpage */
#define POOL_MAX_BIG_PAGES 1024
#define POOL_BIG_PAGE_HASH_MASK (POOL_MAX_BIG_PAGES - 1)
#define BIG_PAGE_TOMBSTONE ((PVOID)(ULONG_PTR)-1)

typedef struct _POOL_TRACKER_BIG_PAGES {
    PVOID Va;              // Alamat virtual memori
    ULONG Key;             // Pool Tag
    ULONG NumberOfPages;   // Jumlah halaman fisik yang dipesan
    ULONG NumberOfBytes;   // Ukuran asli yang diminta (buat statistik)
    POOL_TYPE PoolType;    // Tipe pool
} POOL_TRACKER_BIG_PAGES, *PPOOL_TRACKER_BIG_PAGES;

#define POOL_TRACK_TABLE_SIZE   256   // power of 2, sesuaikan sama estimasi jumlah tag unik
#define POOL_TRACK_TABLE_MASK   (POOL_TRACK_TABLE_SIZE - 1)
#define POOL_TRACK_TOMBSTONE    0xFFFFFFFF  // Key gak mungkin ini (kecuali tag lu literally itu)

typedef struct _POOL_TRACKER_TABLE {
    ULONG Key;              // Pool Tag (0 = slot belum pernah dipake)
    LONG  NonPagedAllocs;
    LONG  NonPagedFrees;
    LONG  NonPagedBytes;
    LONG  PagedAllocs;
    LONG  PagedFrees;
    LONG  PagedBytes;
} POOL_TRACKER_TABLE, *PPOOL_TRACKER_TABLE;

typedef enum _MEMORY_CACHING_TYPE_ORIG {
  MmFrameBufferCached = 2
} MEMORY_CACHING_TYPE_ORIG;

typedef enum _MEMORY_CACHING_TYPE {
  MmNonCached = FALSE,
  MmCached = TRUE,
  MmWriteCombined = MmFrameBufferCached,
  MmHardwareCoherentCached,
  MmNonCachedUnordered,
  MmUSWCCached,
  MmMaximumCacheType
} MEMORY_CACHING_TYPE;