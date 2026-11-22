/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : heap.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : RTL Heap
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <rtl.h>
#include <vz_user.h>
#include "heap.h"

/* Revision History ------------------------------------------------------
 * DATE       : 08-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file heap.c
 * --------------------------------------------------------------------- */

/* STATIC *********************************************************************/

static
SIZE_T
RtlpHeapSelectSize(IN SIZE_T Requested, IN SIZE_T FromParameters, IN SIZE_T Default)
{
    if (Requested != 0)
    {
        return Requested;
    }
    if (FromParameters != 0)
    {
        return FromParameters;
    }
    return Default;
}

static
VOID
RtlpHeapLinkFreeBlock(IN OUT PHEAP Heap, IN OUT PHEAP_FREE_ENTRY Free)
{
    ULONG Index = Free->Entry.Size;
    PLIST_ENTRY Head;
    PLIST_ENTRY Cursor;

    if (Index >= HEAP_FREELIST_COUNT)
    {
        Index = 0;
    }
    Head = &Heap->FreeLists[Index];

    if (Index == 0)
    {
        /* Large blocks: keep list 0 sorted, smallest first */
        Cursor = Head->Flink;
        while (Cursor != Head &&
               CONTAINING_RECORD(Cursor, HEAP_FREE_ENTRY, FreeList)->Entry.Size < Free->Entry.Size)
        {
            Cursor = Cursor->Flink;
        }
        Head = Cursor;                  /* insert before Cursor */
    }

    InsertTailList(Head, &Free->FreeList);
    Heap->FreeListBitmap[Index >> 5] |= (1UL << (Index & 31));
    Heap->TotalFreeSize += Free->Entry.Size;
}

static
PHEAP_ENTRY
RtlpHeapInsertFreeBlock(IN OUT PHEAP Heap, IN OUT PHEAP_ENTRY Block,
                        IN SIZE_T Units, IN ULONG PreviousUnits)
{
    PHEAP_ENTRY Last = Block;
    SIZE_T Take;

    while (Units > 0)
    {
        Take = Units;
        if (Take > HEAP_MAX_BLOCK_UNITS)
        {
            Take = HEAP_MAX_BLOCK_UNITS;
            if (Units - Take < HEAP_MIN_BLOCK_UNITS)
            {
                Take -= HEAP_MIN_BLOCK_UNITS;
            }
        }

        Block->Size = (USHORT)Take;
        Block->PreviousSize = (USHORT)PreviousUnits;
        Block->SegmentIndex = 0;
        Block->UnusedBytes = 0;
        Block->Flags = (Take == Units) ? HEAP_ENTRY_LAST : 0;
        HEAP_ENTRY_UPDATE_CHECKSUM(Block);
        RtlpHeapLinkFreeBlock(Heap, (PHEAP_FREE_ENTRY)Block);

        Last = Block;
        PreviousUnits = (ULONG)Take;
        Block += Take;
        Units -= Take;
    }
    return Last;
}


static
VOID
RtlpHeapInitializeSegment(IN OUT PHEAP Heap, OUT PHEAP_SEGMENT Segment,
                          IN PVOID Base, IN PVOID FirstEntry,
                          IN PVOID CommitEnd, IN PVOID ReserveEnd)
{
    Segment->Signature = HEAP_SEGMENT_SIGNATURE;
    Segment->Heap = Heap;
    Segment->BaseAddress = Base;
    Segment->FirstEntry = FirstEntry;
    Segment->CommitEnd = CommitEnd;
    Segment->ReserveEnd = ReserveEnd;
    InsertTailList(&Heap->SegmentList, &Segment->SegmentListEntry);

    /* Whole committed area after the headers becomes one free block */
    Segment->LastEntry = RtlpHeapInsertFreeBlock(Heap, (PHEAP_ENTRY)FirstEntry,
                            ((PUCHAR)CommitEnd - (PUCHAR)FirstEntry) >> HEAP_GRANULARITY_SHIFT, 0);
}

static
VOID
RtlpHeapLock(IN PHEAP Heap, IN ULONG Flags)
{
    if (!(Flags & HEAP_NO_SERIALIZE) && Heap->Lock != NULL)
    {
        RtlEnterCriticalSection(Heap->Lock);
    }
}

static
VOID
RtlpHeapUnlock(IN PHEAP Heap, IN ULONG Flags)
{
    if (!(Flags & HEAP_NO_SERIALIZE) && Heap->Lock != NULL)
    {
        RtlLeaveCriticalSection(Heap->Lock);
    }
}

static
ULONG
RtlpHeapProtect(IN PHEAP Heap)
{
    return (Heap->Flags & HEAP_CREATE_ENABLE_EXECUTE) ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
}

static
VOID
RtlpHeapSetEntry(OUT PHEAP_ENTRY Entry, IN SIZE_T Units, IN ULONG PreviousUnits,
                 IN UCHAR Flags, IN UCHAR UnusedBytes)
{
    Entry->Size = (USHORT)Units;
    Entry->PreviousSize = (USHORT)PreviousUnits;
    Entry->SegmentIndex = 0;
    Entry->Flags = Flags;
    Entry->UnusedBytes = UnusedBytes;
    HEAP_ENTRY_UPDATE_CHECKSUM(Entry);
}

static
BOOLEAN
RtlpHeapEntryIsValid(IN PHEAP_ENTRY Entry)
{
    PUCHAR Bytes = (PUCHAR)Entry;
    UCHAR Sum = 0;
    ULONG Index;

    for (Index = 0; Index < 7; Index++)
    {
        Sum ^= Bytes[Index];
    }
    return (BOOLEAN)(Sum == Entry->Cookie);
}

static
PHEAP_SEGMENT
RtlpHeapFindSegment(IN PHEAP Heap, IN PVOID Address)
{
    PLIST_ENTRY Link;
    PHEAP_SEGMENT Segment;

    for (Link = Heap->SegmentList.Flink; Link != &Heap->SegmentList; Link = Link->Flink)
    {
        Segment = CONTAINING_RECORD(Link, HEAP_SEGMENT, SegmentListEntry);
        if ((PUCHAR)Address >= (PUCHAR)Segment->FirstEntry &&
            (PUCHAR)Address < (PUCHAR)Segment->CommitEnd)
        {
            return Segment;
        }
    }
    return NULL;
}

static
VOID
RtlpHeapUnlinkFreeBlock(IN OUT PHEAP Heap, IN OUT PHEAP_FREE_ENTRY Free)
{
    ULONG Index = Free->Entry.Size;

    if (Index >= HEAP_FREELIST_COUNT)
    {
        Index = 0;
    }

    RemoveEntryList(&Free->FreeList);
    if (IsListEmpty(&Heap->FreeLists[Index]))
    {
        Heap->FreeListBitmap[Index >> 5] &= ~(1UL << (Index & 31));
    }
    Heap->TotalFreeSize -= Free->Entry.Size;
}

static
ULONG
RtlpHeapLowestSetBit(IN ULONG Value)        /* Value != 0 */
{
    ULONG Bit = 0;

    while ((Value & 1UL) == 0)
    {
        Value >>= 1;
        Bit++;
    }
    return Bit;
}

static
PHEAP_FREE_ENTRY
RtlpHeapFindFreeBlock(IN PHEAP Heap, IN SIZE_T Units)
{
    PLIST_ENTRY Head;
    PLIST_ENTRY Link;
    PHEAP_FREE_ENTRY Free;
    ULONG Word;
    ULONG Bits;
    ULONG Index;

    if (Units < HEAP_FREELIST_COUNT)
    {
        for (Word = (ULONG)(Units >> 5); Word < HEAP_FREELIST_COUNT / 32; Word++)
        {
            Bits = Heap->FreeListBitmap[Word];
            if (Word == (ULONG)(Units >> 5))
            {
                Bits &= ~0UL << (Units & 31);
            }
            if (Bits != 0)
            {
                Index = (Word << 5) + RtlpHeapLowestSetBit(Bits);
                return CONTAINING_RECORD(Heap->FreeLists[Index].Flink, HEAP_FREE_ENTRY, FreeList);
            }
        }
    }

    Head = &Heap->FreeLists[0];
    for (Link = Head->Flink; Link != Head; Link = Link->Flink)
    {
        Free = CONTAINING_RECORD(Link, HEAP_FREE_ENTRY, FreeList);
        if (Free->Entry.Size >= Units)
        {
            return Free;
        }
    }
    return NULL;
}

static
PHEAP_ENTRY
RtlpHeapCarve(IN OUT PHEAP Heap, IN OUT PHEAP_FREE_ENTRY Free, IN SIZE_T Units)
{
    PHEAP_ENTRY Block = &Free->Entry;
    PHEAP_ENTRY Tail;
    PHEAP_ENTRY After;
    PHEAP_SEGMENT Segment;
    SIZE_T Remain = Block->Size - Units;
    UCHAR WasLast = (UCHAR)(Block->Flags & HEAP_ENTRY_LAST);

    RtlpHeapUnlinkFreeBlock(Heap, Free);

    if (Remain < HEAP_MIN_BLOCK_UNITS)
    {
        /* Sisa terlalu kecil untuk berdiri sendiri: caller dapat seluruh block */
        Block->Flags = (UCHAR)(HEAP_ENTRY_BUSY | WasLast);
        HEAP_ENTRY_UPDATE_CHECKSUM(Block);
        return Block;
    }

    Tail = Block + Units;
    RtlpHeapSetEntry(Tail, Remain, Units, WasLast, 0);
    RtlpHeapSetEntry(Block, Units, Block->PreviousSize, HEAP_ENTRY_BUSY, 0);

    if (WasLast)
    {
        Segment = RtlpHeapFindSegment(Heap, Block);
        if (Segment != NULL)
        {
            Segment->LastEntry = Tail;
        }
    }
    else
    {
        After = Tail + Remain;
        After->PreviousSize = (USHORT)Remain;
        HEAP_ENTRY_UPDATE_CHECKSUM(After);
    }

    RtlpHeapLinkFreeBlock(Heap, (PHEAP_FREE_ENTRY)Tail);
    return Block;
}

static
BOOLEAN
RtlpHeapExtendSegment(IN OUT PHEAP Heap, IN OUT PHEAP_SEGMENT Segment, IN SIZE_T Units)
{
    PHEAP_ENTRY Last = Segment->LastEntry;
    PUCHAR OldEnd = (PUCHAR)Segment->CommitEnd;
    PVOID CommitBase = OldEnd;
    SIZE_T Available = (PUCHAR)Segment->ReserveEnd - OldEnd;
    SIZE_T Need = Units << HEAP_GRANULARITY_SHIFT;
    SIZE_T LastBytes = 0;
    SIZE_T Bytes;
    SIZE_T Total;
    SIZE_T Previous;
    PHEAP_ENTRY Start;
    VEASTATUS Status;
    BOOLEAN TrailingFree = !(Last->Flags & HEAP_ENTRY_BUSY);

    if (TrailingFree)
    {
        LastBytes = (SIZE_T)Last->Size << HEAP_GRANULARITY_SHIFT;
    }
    Need = (Need > LastBytes) ? (Need - LastBytes) : 0;

    Bytes = HEAP_ALIGN_UP((Need > Heap->SegmentCommit) ? Need : Heap->SegmentCommit, PAGE_SIZE);
    if (Bytes > Available)
    {
        Bytes = HEAP_ALIGN_UP(Need, PAGE_SIZE);     /* coba minimum yang dibutuhkan */
    }
    if (Bytes == 0 || Bytes > Available)
    {
        return FALSE;
    }

    if (Heap->CommitRoutine != NULL)
    {
        Status = Heap->CommitRoutine((PVOID)Heap, &CommitBase, &Bytes);
    }
    else
    {
        Status = VzAllocateVirtualMemory(NtCurrentProcess(), &CommitBase, 0, &Bytes,
                                         MEM_COMMIT, RtlpHeapProtect(Heap));
    }
    if (!VEA_SUCCESS(Status))
    {
        return FALSE;
    }
    Segment->CommitEnd = OldEnd + Bytes;

    if (TrailingFree)
    {
        Start = Last;
        Previous = Last->PreviousSize;
        Total = Last->Size + (Bytes >> HEAP_GRANULARITY_SHIFT);
        RtlpHeapUnlinkFreeBlock(Heap, (PHEAP_FREE_ENTRY)Last);
    }
    else
    {
        Start = (PHEAP_ENTRY)OldEnd;
        Previous = Last->Size;
        Total = Bytes >> HEAP_GRANULARITY_SHIFT;
        RtlpHeapSetEntry(Last, Last->Size, Last->PreviousSize,
                         (UCHAR)(Last->Flags & ~HEAP_ENTRY_LAST), Last->UnusedBytes);
    }

    Segment->LastEntry = RtlpHeapInsertFreeBlock(Heap, Start, Total, (ULONG)Previous);
    return TRUE;
}

static
BOOLEAN
RtlpHeapCreateSegment(IN OUT PHEAP Heap, IN SIZE_T Units)
{
    PVOID Base = NULL;
    PVOID CommitBase;
    SIZE_T Zero = 0;
    SIZE_T HeaderSize = HEAP_ALIGN_UP(sizeof(HEAP_SEGMENT), HEAP_GRANULARITY);
    SIZE_T Need = HeaderSize + (Units << HEAP_GRANULARITY_SHIFT);
    SIZE_T Reserve = HEAP_ALIGN_UP((Need > Heap->SegmentReserve) ? Need : Heap->SegmentReserve, PAGE_SIZE);
    SIZE_T Commit = HEAP_ALIGN_UP((Need > Heap->SegmentCommit) ? Need : Heap->SegmentCommit, PAGE_SIZE);
    ULONG Protect = RtlpHeapProtect(Heap);
    VEASTATUS Status;

    if (Commit > Reserve)
    {
        Reserve = Commit;
    }

    Status = VzAllocateVirtualMemory(NtCurrentProcess(), &Base, 0, &Reserve, MEM_RESERVE, Protect);
    if (!VEA_SUCCESS(Status))
    {
        return FALSE;
    }

    CommitBase = Base;
    Status = VzAllocateVirtualMemory(NtCurrentProcess(), &CommitBase, 0, &Commit, MEM_COMMIT, Protect);
    if (!VEA_SUCCESS(Status))
    {
        VzFreeVirtualMemory(NtCurrentProcess(), &Base, &Zero, MEM_RELEASE);
        return FALSE;
    }

    RtlZeroMemory(Base, HeaderSize);
    RtlpHeapInitializeSegment(Heap, (PHEAP_SEGMENT)Base, Base, (PUCHAR)Base + HeaderSize,
                              (PUCHAR)Base + Commit, (PUCHAR)Base + Reserve);
    return TRUE;
}

static
BOOLEAN
RtlpHeapExtend(IN OUT PHEAP Heap, IN SIZE_T Units)
{
    PLIST_ENTRY Link;
    PHEAP_SEGMENT Segment;

    /* 1. Tumbuhkan segment yang ada (in place) */
    for (Link = Heap->SegmentList.Flink; Link != &Heap->SegmentList; Link = Link->Flink)
    {
        Segment = CONTAINING_RECORD(Link, HEAP_SEGMENT, SegmentListEntry);
        if (RtlpHeapExtendSegment(Heap, Segment, Units))
        {
            return TRUE;
        }
    }

    /* 2. Segment baru, hanya kalau heap growable */
    if (!(Heap->Flags & HEAP_GROWABLE))
    {
        return FALSE;
    }
    return RtlpHeapCreateSegment(Heap, Units);
}

static
PVOID
RtlpHeapAllocateBlock(IN OUT PHEAP Heap, IN SIZE_T Units, IN SIZE_T Size)
{
    PHEAP_FREE_ENTRY Free;
    PHEAP_ENTRY Block;

    Free = RtlpHeapFindFreeBlock(Heap, Units);
    if (Free == NULL)
    {
        if (!RtlpHeapExtend(Heap, Units))
        {
            return NULL;
        }
        Free = RtlpHeapFindFreeBlock(Heap, Units);
        if (Free == NULL)
        {
            return NULL;
        }
    }

    Block = RtlpHeapCarve(Heap, Free, Units);
    Block->UnusedBytes = (UCHAR)(((SIZE_T)Block->Size << HEAP_GRANULARITY_SHIFT) -
                                 sizeof(HEAP_ENTRY) - Size);
    HEAP_ENTRY_UPDATE_CHECKSUM(Block);
    return Block + 1;
}

static
PVOID
RtlpHeapAllocateVirtual(IN OUT PHEAP Heap, IN SIZE_T Size)
{
    PHEAP_VIRTUAL_ALLOC_ENTRY Virtual = NULL;
    SIZE_T Bytes = HEAP_ALIGN_UP(Size + sizeof(HEAP_VIRTUAL_ALLOC_ENTRY), PAGE_SIZE);
    VEASTATUS Status;

    Status = VzAllocateVirtualMemory(NtCurrentProcess(), (PVOID *)&Virtual, 0, &Bytes,
                                     MEM_RESERVE | MEM_COMMIT, RtlpHeapProtect(Heap));
    if (!VEA_SUCCESS(Status))
    {
        return NULL;
    }

    Virtual->CommitSize = Bytes;
    Virtual->ReserveSize = Bytes;
    RtlpHeapSetEntry(&Virtual->BusyBlock, 0, 0, HEAP_ENTRY_BUSY | HEAP_ENTRY_VIRTUAL, 0);
    InsertTailList(&Heap->VirtualAllocList, &Virtual->Entry);
    return &Virtual->BusyBlock + 1;             /* halaman baru sudah nol */
}

static
BOOLEAN
RtlpHeapValidateBlock(IN PHEAP_SEGMENT Segment, IN PHEAP_ENTRY Block)
{
    PHEAP_ENTRY Neighbor;

    if (!RtlpHeapEntryIsValid(Block) ||
        !(Block->Flags & HEAP_ENTRY_BUSY) ||
        (Block->Flags & HEAP_ENTRY_VIRTUAL) ||
        Block->Size < HEAP_MIN_BLOCK_UNITS ||
        (PUCHAR)(Block + Block->Size) > (PUCHAR)Segment->CommitEnd)
    {
        return FALSE;
    }

    if (Block->PreviousSize != 0)
    {
        Neighbor = Block - Block->PreviousSize;
        if ((PUCHAR)Neighbor < (PUCHAR)Segment->FirstEntry ||
            !RtlpHeapEntryIsValid(Neighbor) ||
            Neighbor->Size != Block->PreviousSize)
        {
            return FALSE;
        }
    }
    else if ((PVOID)Block != Segment->FirstEntry)
    {
        return FALSE;
    }

    if (!(Block->Flags & HEAP_ENTRY_LAST))
    {
        Neighbor = Block + Block->Size;
        if ((PUCHAR)Neighbor >= (PUCHAR)Segment->CommitEnd ||
            !RtlpHeapEntryIsValid(Neighbor) ||
            Neighbor->PreviousSize != Block->Size)
        {
            return FALSE;
        }
    }
    return TRUE;
}

static
BOOLEAN
RtlpHeapFreeBlock(IN OUT PHEAP Heap, IN OUT PHEAP_SEGMENT Segment, IN OUT PHEAP_ENTRY Block)
{
    PHEAP_ENTRY Next;
    PHEAP_ENTRY Prev;
    PHEAP_ENTRY After;
    SIZE_T Units;
    BOOLEAN IsLast;
    BOOLEAN Coalesce = !(Heap->ForceFlags & HEAP_DISABLE_COALESCE_ON_FREE);

    if (!RtlpHeapValidateBlock(Segment, Block))
    {
        return FALSE;
    }

    Units = Block->Size;
    IsLast = (BOOLEAN)((Block->Flags & HEAP_ENTRY_LAST) != 0);

    /* Gabung ke depan (hasil gabungan tidak boleh > HEAP_MAX_BLOCK_UNITS) */
    if (Coalesce && !IsLast)
    {
        Next = Block + Units;
        if (!(Next->Flags & HEAP_ENTRY_BUSY) && Units + Next->Size <= HEAP_MAX_BLOCK_UNITS)
        {
            IsLast = (BOOLEAN)((Next->Flags & HEAP_ENTRY_LAST) != 0);
            RtlpHeapUnlinkFreeBlock(Heap, (PHEAP_FREE_ENTRY)Next);
            Units += Next->Size;
        }
    }

    /* Gabung ke belakang */
    if (Coalesce && Block->PreviousSize != 0)
    {
        Prev = Block - Block->PreviousSize;
        if (!(Prev->Flags & HEAP_ENTRY_BUSY) && Units + Prev->Size <= HEAP_MAX_BLOCK_UNITS)
        {
            RtlpHeapUnlinkFreeBlock(Heap, (PHEAP_FREE_ENTRY)Prev);
            Units += Prev->Size;
            Block = Prev;
        }
    }

    RtlpHeapSetEntry(Block, Units, Block->PreviousSize, IsLast ? HEAP_ENTRY_LAST : 0, 0);

    if (IsLast)
    {
        Segment->LastEntry = Block;
    }
    else
    {
        After = Block + Units;
        After->PreviousSize = (USHORT)Units;
        HEAP_ENTRY_UPDATE_CHECKSUM(After);
    }

    RtlpHeapLinkFreeBlock(Heap, (PHEAP_FREE_ENTRY)Block);
    return TRUE;
}

static
BOOLEAN
RtlpHeapUnlinkVirtual(IN OUT PHEAP Heap, IN PVOID BaseAddress, OUT PVOID *Region)
{
    PLIST_ENTRY Link;
    PHEAP_VIRTUAL_ALLOC_ENTRY Virtual;

    for (Link = Heap->VirtualAllocList.Flink; Link != &Heap->VirtualAllocList; Link = Link->Flink)
    {
        Virtual = CONTAINING_RECORD(Link, HEAP_VIRTUAL_ALLOC_ENTRY, Entry);
        if ((PVOID)(&Virtual->BusyBlock + 1) == BaseAddress)
        {
            RemoveEntryList(&Virtual->Entry);
            *Region = Virtual;
            return TRUE;
        }
    }
    return FALSE;
}

/* PUBLIC *****************************************************************************/

PVOID
VEAPI
RtlCreateHeap(IN ULONG Flags,
              IN PVOID HeapBase OPTIONAL,
              IN SIZE_T ReserveSize OPTIONAL,
              IN SIZE_T CommitSize OPTIONAL,
              IN PVOID Lock OPTIONAL,
              IN PRTL_HEAP_PARAMETERS Parameters OPTIONAL)
{
    VEASTATUS Status;
    RTL_HEAP_PARAMETERS Params;
    PHEAP Heap;
    PVOID Base;
    PVOID CommitBase;
    PUCHAR FirstEntry;
    SIZE_T Reserve;
    SIZE_T Commit;
    SIZE_T HeaderSize;
    SIZE_T MinSize;
    SIZE_T Zero = 0;
    ULONG Protect;
    ULONG Index;
    BOOLEAN OwnsMemory = (HeapBase == NULL);

    Flags &= HEAP_CREATE_VALID_MASK;

    /* A caller-supplied lock only makes sense for a serialized heap */
    if ((Flags & HEAP_NO_SERIALIZE) && Lock != NULL)
    {
        return NULL;
    }

    RtlZeroMemory(&Params, sizeof(Params));
    if (Parameters != NULL)
    {
        if (Parameters->Length < sizeof(RTL_HEAP_PARAMETERS))
        {
            return NULL;
        }
        Params = *Parameters;
    }

    /* HEAP + first HEAP_SEGMENT, then at least one free block */
    HeaderSize = HEAP_ALIGN_UP(sizeof(HEAP) + sizeof(HEAP_SEGMENT), HEAP_GRANULARITY);
    MinSize = HeaderSize + (HEAP_MIN_BLOCK_UNITS << HEAP_GRANULARITY_SHIFT);

    if (HeapBase != NULL)
    {
        /* Caller-owned memory: assumed fully committed, 8-byte aligned */
        if (((ULONG_PTR)HeapBase & (HEAP_GRANULARITY - 1)) != 0 || ReserveSize < MinSize)
        {
            return NULL;
        }
        Base = HeapBase;
        Reserve = ReserveSize & ~(HEAP_GRANULARITY - 1);
        Commit = Reserve;
    }
    else
    {
        Reserve = RtlpHeapSelectSize(ReserveSize, Params.SegmentReserve, HEAP_DEFAULT_RESERVE);
        Commit = RtlpHeapSelectSize(CommitSize, Params.SegmentCommit, HEAP_DEFAULT_COMMIT);

        Commit = HEAP_ALIGN_UP(Commit, PAGE_SIZE);
        if (Commit < HEAP_ALIGN_UP(MinSize, PAGE_SIZE))
        {
            Commit = HEAP_ALIGN_UP(MinSize, PAGE_SIZE);
        }
        Reserve = HEAP_ALIGN_UP(Reserve, PAGE_SIZE);
        if (Reserve < Commit)
        {
            Reserve = Commit;
        }

        Protect = (Flags & HEAP_CREATE_ENABLE_EXECUTE) ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;

        /* Reserve the whole region first... */
        Base = NULL;
        Status = VzAllocateVirtualMemory(NtCurrentProcess(), &Base, 0, &Reserve,
                                         MEM_RESERVE, Protect);
        if (!VEA_SUCCESS(Status))
        {
            return NULL;
        }

        /* ...then commit only the initial part (this is the watermark) */
        CommitBase = Base;
        Status = VzAllocateVirtualMemory(NtCurrentProcess(), &CommitBase, 0, &Commit,
                                         MEM_COMMIT, Protect);
        if (!VEA_SUCCESS(Status))
        {
            VzFreeVirtualMemory(NtCurrentProcess(), &Base, &Zero, MEM_RELEASE);
            return NULL;
        }
    }

    /* Heap handle == address of the HEAP struct at the start of the region */
    Heap = (PHEAP)Base;
    RtlZeroMemory(Heap, HeaderSize);

    Heap->Signature = HEAP_SIGNATURE;
    Heap->Flags = Flags;
    Heap->ForceFlags = Flags & HEAP_FORCE_MASK;
    Heap->InternalFlags = OwnsMemory ? 0 : HEAP_INT_USER_MEMORY;
    Heap->SegmentReserve = RtlpHeapSelectSize(0, Params.SegmentReserve, HEAP_DEFAULT_RESERVE);
    Heap->SegmentCommit = RtlpHeapSelectSize(0, Params.SegmentCommit, HEAP_DEFAULT_COMMIT);
    Heap->DeCommitFreeBlockThreshold = RtlpHeapSelectSize(0, Params.DeCommitFreeBlockThreshold, HEAP_DEFAULT_DECOMMIT_FREE);
    Heap->DeCommitTotalFreeThreshold = RtlpHeapSelectSize(0, Params.DeCommitTotalFreeThreshold, HEAP_DEFAULT_DECOMMIT_TOTAL);
    Heap->MaximumAllocationSize = RtlpHeapSelectSize(0, Params.MaximumAllocationSize, HEAP_DEFAULT_MAX_ALLOC);
    Heap->VirtualMemoryThreshold = RtlpHeapSelectSize(0, Params.VirtualMemoryThreshold,
                                                      HEAP_MAX_BLOCK_UNITS << HEAP_GRANULARITY_SHIFT);
    if (Heap->VirtualMemoryThreshold > (HEAP_MAX_BLOCK_UNITS << HEAP_GRANULARITY_SHIFT))
    {
        Heap->VirtualMemoryThreshold = HEAP_MAX_BLOCK_UNITS << HEAP_GRANULARITY_SHIFT;
    }
    Heap->CommitRoutine = Params.CommitRoutine;

    if (!(Flags & HEAP_NO_SERIALIZE))
    {
        if (Lock != NULL)
        {
            Heap->Lock = (PRTL_CRITICAL_SECTION)Lock;
            Heap->InternalFlags |= HEAP_INT_USER_LOCK;
        }
        else
        {
            Status = RtlInitializeCriticalSection(&Heap->LockObject);
            if (!VEA_SUCCESS(Status))
            {
                goto Fail;
            }
            Heap->Lock = &Heap->LockObject;
        }
    }

    for (Index = 0; Index < HEAP_FREELIST_COUNT; Index++)
    {
        InitializeListHead(&Heap->FreeLists[Index]);
    }
    InitializeListHead(&Heap->SegmentList);
    InitializeListHead(&Heap->VirtualAllocList);

    FirstEntry = (PUCHAR)Base + HeaderSize;
    RtlpHeapInitializeSegment(Heap, (PHEAP_SEGMENT)(Heap + 1), Base, FirstEntry,
                              (PUCHAR)Base + Commit, (PUCHAR)Base + Reserve);

    /* TODO: register Heap in PEB->ProcessHeaps */
    return Heap;

Fail:
    if (OwnsMemory)
    {
        VzFreeVirtualMemory(NtCurrentProcess(), &Base, &Zero, MEM_RELEASE);
    }
    return NULL;
}

PVOID
VEAPI
RtlAllocateHeap(PVOID HeapHandle, ULONG Flags, SIZE_T Size)
{
    PHEAP Heap = (PHEAP)HeapHandle;
    PVOID Result = NULL;
    SIZE_T Units;
    BOOLEAN UseVirtual;

    if (Heap == NULL || Heap->Signature != HEAP_SIGNATURE)
    {
        return NULL;
    }

    Flags = (Flags | Heap->ForceFlags) & HEAP_ALLOC_VALID_MASK;

    if (Size <= Heap->MaximumAllocationSize && Size <= HEAP_HARD_MAX_ALLOC)
    {
        Units = (Size + sizeof(HEAP_ENTRY) + HEAP_GRANULARITY - 1) >> HEAP_GRANULARITY_SHIFT;
        if (Units < HEAP_MIN_BLOCK_UNITS)
        {
            Units = HEAP_MIN_BLOCK_UNITS;
        }

        UseVirtual = (BOOLEAN)(Units > HEAP_MAX_BLOCK_UNITS - HEAP_MIN_BLOCK_UNITS ||
                               Units > (Heap->VirtualMemoryThreshold >> HEAP_GRANULARITY_SHIFT));

        RtlpHeapLock(Heap, Flags);
        if (UseVirtual)
        {
            Result = RtlpHeapAllocateVirtual(Heap, Size);
        }
        else
        {
            Result = RtlpHeapAllocateBlock(Heap, Units, Size);
        }
        RtlpHeapUnlock(Heap, Flags);

        if (Result != NULL && !UseVirtual && (Flags & HEAP_ZERO_MEMORY))
        {
            RtlZeroMemory(Result, Size);
        }
    }

    /* Raise SETELAH lock dilepas */
    if (Result == NULL && (Flags & HEAP_GENERATE_EXCEPTIONS))
    {
        RtlRaiseStatus(STATUS_NO_MEMORY);
    }
    return Result;
}

BOOLEAN
VEAPI
RtlFreeHeap(PVOID HeapHandle, ULONG Flags, PVOID BaseAddress)
{
    PHEAP Heap = (PHEAP)HeapHandle;
    PHEAP_ENTRY Block;
    PHEAP_SEGMENT Segment;
    PVOID Region = NULL;
    SIZE_T Zero = 0;
    BOOLEAN Result;

    if (BaseAddress == NULL)
    {
        return TRUE;                            /* sama seperti NT */
    }
    if (Heap == NULL || Heap->Signature != HEAP_SIGNATURE ||
        ((ULONG_PTR)BaseAddress & (HEAP_GRANULARITY - 1)) != 0)
    {
        return FALSE;
    }

    Flags = (Flags | Heap->ForceFlags) & HEAP_NO_SERIALIZE;
    Block = (PHEAP_ENTRY)BaseAddress - 1;

    RtlpHeapLock(Heap, Flags);
    Segment = RtlpHeapFindSegment(Heap, Block);
    if (Segment != NULL)
    {
        Result = RtlpHeapFreeBlock(Heap, Segment, Block);
    }
    else
    {
        Result = RtlpHeapUnlinkVirtual(Heap, BaseAddress, &Region);
    }
    RtlpHeapUnlock(Heap, Flags);

    if (Region != NULL)
    {
        VzFreeVirtualMemory(NtCurrentProcess(), &Region, &Zero, MEM_RELEASE);
    }
    return Result;
}


/* EOF */