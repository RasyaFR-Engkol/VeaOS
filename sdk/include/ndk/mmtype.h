#pragma once

#include "procbind.h"
#include "rtl.h"

/* PIP Flags */
#define PIP_STATE_FREE      0  // Halaman kosong, siap dialokasikan
#define PIP_STATE_ACTIVE    1  // Halaman sedang dipakai (oleh kernel/user)
#define PIP_STATE_RESERVED  2  // Halaman dipakai BIOS/Hardware (nggak boleh disentuh)
#define PIP_STATE_ZEROED    3  // Halaman kosong dan isinya udah di-nol-kan (aman untuk keamanan)

/* PIP (Page Index Pointer) */
typedef struct _PIP {
    union { ULONG NextPip; } Flink;
    union { ULONG PrevPip; } Blink;
    PVOID PteAddress;
    USHORT ShareCount;
    USHORT ReferenceCount;
    UCHAR State;
    UCHAR Flags;
    ULONG WsIndex;
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
        ULONG COW           : 1; // Bit 9: Copy-on-Write untuk OS
        ULONG available     : 2; // Bit 10-11: Kosong, OS bebas pake buat apa aja!
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

typedef struct _WORKING_SET {
    ULONG PageFaultCount;       // Berapa kali proses ini kena Page Fault
    ULONG PeakWorkingSetSize;   // Memori fisik maksimal yang pernah dipakai (dalam bytes)
    ULONG WorkingSetSize;       // Memori fisik yang DIPAKAI SAAT INI (dalam bytes)
    LIST_ENTRY WorkingSetList;  // Daftar halaman memori (VAD/PTE) yang saat ini aktif di RAM
} WORKING_SET, *PWORKING_SET;

extern ULONG MmScratchSrcVa;
extern ULONG MmScratchDstVa;

typedef LARGE_INTEGER PHYSICAL_ADDRESS, *PPHYSICAL_ADDRESS;

typedef struct _MDL
{
    struct _MDL *Next;
    USHORT       Size;
    USHORT       MdlFlags;
    struct _EPROCESS *Process;
    PVOID        MappedSystemVa;
    PVOID        StartVa;
    ULONG        ByteCount;
    ULONG        ByteOffset;
    ULONG PfnArray[];
} MDL, *PMDL;

#define MDL_MAPPED_TO_SYSTEM_VA     0x0001
#define MDL_PAGES_LOCKED            0x0002
#define MDL_SOURCE_IS_NONPAGED_POOL 0x0004
#define MDL_ALLOCATED_FIXED_SIZE    0x0008
#define MDL_PARTIAL                 0x0010
#define MDL_PARTIAL_HAS_BEEN_MAPPED 0x0020
#define MDL_IO_PAGE_READ            0x0040
#define MDL_WRITE_OPERATION         0x0080
#define MDL_PARENT_MAPPED_SYSTEM_VA 0x0100
#define MDL_FREE_EXTRA_PTES         0x0200
#define MDL_DESCRIBES_AWE           0x0400
#define MDL_IO_SPACE                0x0800
#define MDL_NETWORK_HEADER          0x1000
#define MDL_MAPPING_CAN_FAIL        0x2000
#define MDL_ALLOCATED_MUST_SUCCEED  0x4000
#define MDL_INTERNAL                0x8000

#define MDL_MAPPING_FLAGS (MDL_MAPPED_TO_SYSTEM_VA     | \
                           MDL_PAGES_LOCKED            | \
                           MDL_SOURCE_IS_NONPAGED_POOL | \
                           MDL_PARTIAL_HAS_BEEN_MAPPED | \
                           MDL_PARENT_MAPPED_SYSTEM_VA | \
                           MDL_IO_SPACE)

typedef ULONG_PTR PFN_NUMBER, *PPFN_NUMBER;

typedef enum _MM_SYSTEMSIZE {
    MmSmallSystem,
    MmMediumSystem,
    MmLargeSystem
} MM_SYSTEMSIZE;

extern ULONG MmNumberOfPhysicalPages;
extern MM_SYSTEMSIZE MmSystemSize;

#define KERNEL_PHYS_BASE 0x1000000              // 16MB (Tempat kernel ditaruh)
#define KERNEL_RESERVE_SIZE (50 * 1024 * 1024)  // 50MB (Area aman untuk kernel)
#define MM_PAGE_FRAME_NUMBER_SIZE         20

#define _1KB (1024u)
#define _1MB (1024 * _1KB)
#define _1GB (1024 * _1MB)
#define _64K (64 * _1KB)

typedef struct _MMWSLENTRY
{
    ULONG_PTR Valid:1;
    ULONG_PTR LockedInWs:1;
    ULONG_PTR LockedInMemory:1;
    ULONG_PTR Protection:5;
    ULONG_PTR Hashed:1;
    ULONG_PTR Direct:1;
    ULONG_PTR Age:2;
    ULONG_PTR VirtualPageNumber: MM_PAGE_FRAME_NUMBER_SIZE;
} MMWSLENTRY, *PMMWSLENTRY;

typedef struct _MMWSLE_FREE_ENTRY
{
    ULONG MustBeZero:1;
#ifdef _WIN64
    ULONG PreviousFree: 31;
    LONG NextFree;
#define MMWSLE_PREVIOUS_FREE_MASK 0x7FFFFFFF
#else
    ULONG PreviousFree: 11;
#define MMWSLE_PREVIOUS_FREE_MASK 0x7FF
#define MMWSLE_PREVIOUS_FREE_JUMP 0x800
    LONG NextFree: 20;
#endif
} MMWSLE_FREE_ENTRY, *PMMWSLE_FREE_ENTRY;

typedef struct _MMWSLE
{
    union
    {
        PVOID VirtualAddress;
        ULONG_PTR Long;
        MMWSLENTRY e1;
        MMWSLE_FREE_ENTRY Free;
    } u1;
} MMWSLE, *PMMWSLE;

typedef struct _MMSUPPORT {
    ULONG PageFaultCount;         // Berapa kali kena Page Fault
    ULONG WorkingSetSize;         // Jumlah halaman fisik aktif saat ini (Pages)
    ULONG PeakWorkingSetSize;     // Peak halaman fisik terbanyak (Pages)
    ULONG MinimumWorkingSetSize;  // Quota Min (misal: 50 Pages)
    ULONG MaximumWorkingSetSize;  // Quota Max (misal: 345 Pages)
    
    PMMWSLE Wsle;                 // Array entri Working Set (dinamis)
    ULONG LastUnusedWsle;         // Hint slot WSLE kosong buat alokasi cepat
    ULONG FirstDynamic;           // Indeks awal halaman dinamis (setelah locked pages)
    ULONG LastEntry;              // Indeks entri terakhir yang terisi
} MMSUPPORT, *PMMSUPPORT;

#define MM_VADS_PER_PAGE (4096 / sizeof(MM_VAD))

/* BSOD BugCode Parameter */
#define MM_BUGCHECK_PTE_CORRUPTED 0x41284

typedef enum _LOCK_OPERATION {
    IoReadAccess,
    IoWriteAccess,
    IoModifyAccess
} LOCK_OPERATION;

extern BOOLEAN UlPoolInitialized;
extern RTL_BITMAP MiPipBitmap;

// Section Allocation Att

#define SEC_NO_CHANGE                                       0x400000
#define SEC_FILE                                            0x800000
#define SEC_IMAGE                                           0x1000000
#define SEC_PROTECTED_IMAGE                                 0x2000000
#define SEC_RESERVE                                         0x4000000
#define SEC_COMMIT                                          0x8000000
#define SEC_NOCACHE                                         0x10000000
#define SEC_WRITECOMBINE                                    0x40000000
#define SEC_LARGE_PAGES                                     0x80000000

typedef enum _SECTION_INHERIT {
    ViewShare = 1,
    ViewUnmap = 2
} SECTION_INHERIT;

typedef struct _SEGMENT SEGMENT, *PSEGMENT;
typedef struct _FILE_OBJECT FILE_OBJECT, *PFILE_OBJECT;

typedef struct _CONTROL_AREA {
    PSEGMENT Segment;
    LIST_ENTRY DereferenceList;
    ULONG NumberOfSectionReferences;
    ULONG NumberOfUserReferences;
    ULONG NumberOfMappedViews;
    ULONG NumberOfSystemCacheViews;
    ULONG NumberOfPfnReferences;
    ULONG WritableUserReferences;
    ULONG QuadwordPad;
    
    // Flags
    union {
        ULONG LongFlags;
        struct {
            ULONG BeingCreated : 1;
            ULONG BeingDeleted : 1;
            ULONG BeingPurged : 1;
            ULONG NoModifiedWriting : 1;
            ULONG FailIfNeedsUnclean : 1;
            ULONG Image : 1;
            ULONG Based : 1;
            ULONG File : 1;
            ULONG Networked : 1;
            ULONG NoCache : 1;
            ULONG PhysicalMemory : 1;
            ULONG CopyOnWrite : 1;
            ULONG Reserve : 1;
            ULONG Commit : 1;
            ULONG FloppyMedia : 1;
            ULONG WasPurged : 1;
            ULONG UserWritable : 1;
            ULONG ValidDataLength : 1;
            ULONG SystemViews : 1;
            ULONG PackageFile : 1;
            ULONG FilePointerNull : 1;
            ULONG DebugSymbolsLoaded : 1;
            ULONG SetMappedFileInformation : 1;
            ULONG GlobalMemory : 1;
            ULONG FastDataSection : 1;
            ULONG GlobalOnlyPerSession : 1;
            ULONG Rom : 1;
            ULONG Deleting : 1;
        } Flags;
    } u;

    PFILE_OBJECT FilePointer;
    struct _EVENT_COUNTER *WaitingForDeletion;
    USHORT ModifiedWriteCount;
    USHORT FlushInProgressCount;
} CONTROL_AREA, *PCONTROL_AREA;

typedef struct _SEGMENT {
    PCONTROL_AREA ControlArea;
    ULONG TotalNumberOfPtes;
    ULONG NonExtendedPtes;
    ULONG_PTR SizeOfSegment;
    ULONG_PTR BasedAddress;
    MMWSLE Wsle;
    ULONG u1;
    ULONG u2;
    PVOID PrototypePte;
} SEGMENT, *PSEGMENT;

typedef struct _SUBSECTION {
    PCONTROL_AREA ControlArea;
    union {
        ULONG LongFlags;
        struct {
            ULONG ReadOnly : 1;
            ULONG ReadWrite : 1;
            ULONG SubSectionBase : 1;
            ULONG WhichArea : 1;
            ULONG Protection : 5;
            ULONG LargePages : 1;
            ULONG StartingSector : 22;
        } Flags;
    } u;

    ULONG StartingSector;
    ULONG NumberOfFullSectors;
    PVOID SubsectionBase;
    ULONG UnusedPtes;
    ULONG PtesInSubSection;
    struct _SUBSECTION *NextSubsection;
} SUBSECTION, *PSUBSECTION;

typedef struct _MSUBSECTION {
    SUBSECTION Core;
    // Porsi khusus untuk Mapped Subsections
} MSUBSECTION, *PMSUBSECTION;

typedef struct _MMADDRESS_NODE
{
    union
    {
        LONG_PTR Balance:2;
        struct _MMADDRESS_NODE *Parent;
    } u1;
    struct _MMADDRESS_NODE *LeftChild;
    struct _MMADDRESS_NODE *RightChild;
    ULONG_PTR StartingVpn;
    ULONG_PTR EndingVpn;
} MMADDRESS_NODE, *PMMADDRESS_NODE;

typedef struct _MMSECTION_FLAGS
{
    ULONG BeingDeleted:1;
    ULONG BeingCreated:1;
    ULONG BeingPurged:1;
    ULONG NoModifiedWriting:1;
    ULONG FailAllIo:1;
    ULONG Image:1;
    ULONG Based:1;
    ULONG File:1;
    ULONG Networked:1;
    ULONG NoCache:1;
    ULONG PhysicalMemory:1;
    ULONG CopyOnWrite:1;
    ULONG Reserve:1;
    ULONG Commit:1;
    ULONG FloppyMedia:1;
    ULONG WasPurged:1;
    ULONG UserReference:1;
    ULONG GlobalMemory:1;
    ULONG DeleteOnClose:1;
    ULONG FilePointerNull:1;
    ULONG DebugSymbolsLoaded:1;
    ULONG SetMappedFileIoComplete:1;
    ULONG CollidedFlush:1;
    ULONG NoChange:1;
    ULONG filler0:1;
    ULONG ImageMappedInSystemSpace:1;
    ULONG UserWritable:1;
    ULONG Accessed:1;
    ULONG GlobalOnlyPerSession:1;
    ULONG Rom:1;
    ULONG WriteCombined:1;
    ULONG filler:1;
} MMSECTION_FLAGS, *PMMSECTION_FLAGS;

typedef struct _SECTION
{
    MMADDRESS_NODE Address;
    PSEGMENT Segment;
    LARGE_INTEGER SizeOfSection;
    union
    {
        ULONG LongFlags;
        MMSECTION_FLAGS Flags;
    };
    ULONG InitialPageProtection;
} SECTION, *PSECTION;

#define SECTION_QUERY                0x0001
#define SECTION_MAP_WRITE            0x0002
#define SECTION_MAP_READ             0x0004
#define SECTION_MAP_EXECUTE          0x0008
#define SECTION_EXTEND_SIZE          0x0010
#define SECTION_MAP_EXECUTE_EXPLICIT 0x0020

#define SECTION_ALL_ACCESS (VEA_STANDARD_RIGHTS_REQUIRED|SECTION_QUERY| \
                            SECTION_MAP_WRITE |                     \
                            SECTION_MAP_READ |                      \
                            SECTION_MAP_EXECUTE |                   \
                            SECTION_EXTEND_SIZE)

extern PFN_NUMBER MmHighestPhysicalPage;

typedef struct _MM_SECTION_SEGMENT
{
    LONG64 RefCount;
    PFILE_OBJECT FileObject;

    // FAST_MUTEX Lock;
    LARGE_INTEGER RawLength;		/* length of the segment which is part of the mapped file */
    LARGE_INTEGER Length;			/* absolute length of the segment */
    PLONG64 ReferenceCount;
	ULONG SectionCount;
    ULONG Protection;
    PULONG Flags;
    BOOLEAN WriteCopy;
	BOOLEAN Locked;

	struct
	{
		ULONGLONG FileOffset;		/* start offset into the file for image sections */
		ULONG_PTR VirtualAddress;	/* start offset into the address range for image sections */
		ULONG Characteristics;
	} Image;

	ULONG SegFlags;

    ULONGLONG LastPage;

	RTL_GENERIC_TABLE PageTable;
} MM_SECTION_SEGMENT, *PMM_SECTION_SEGMENT;

// Page Attribute
#define PAGE_NOACCESS          0x01
#define PAGE_READONLY          0x02
#define PAGE_READWRITE         0x04
#define PAGE_WRITECOPY         0x08
#define PAGE_EXECUTE           0x10
#define PAGE_EXECUTE_READ      0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_WRITECOPY 0x80
#define PAGE_GUARD            0x100
#define PAGE_NOCACHE          0x200
#define PAGE_WRITECOMBINE     0x400

// Segment Attribute
#define MM_PHYSICALMEMORY_SEGMENT           (0x1)
#define MM_DATAFILE_SEGMENT                 (0x2)
#define MM_SEGMENT_INDELETE                 (0x4)
#define MM_SEGMENT_INCREATE                 (0x8)
#define MM_IMAGE_SECTION_FLUSH_DELETE       (0x10)

/* EOF */