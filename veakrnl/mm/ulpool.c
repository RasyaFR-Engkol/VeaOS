#include <veakrnl.h>

POOL_DESCRIPTOR UlPoolVector[MaxPoolType];
POOL_TRACKER_BIG_PAGES UlBigPageTable[POOL_MAX_BIG_PAGES];
POOL_TRACKER_TABLE UlPoolTrackTable[POOL_TRACK_TABLE_SIZE];
ULONG UlpBigPageTableCount = 0;
static KSPIN_LOCK UlPoolLocks[MaxPoolType];
static KSPIN_LOCK UlPoolTrackerLock;
static KSPIN_LOCK UlBigPageTableLock;
BOOLEAN UlPoolInitialized;

/* PRIVATE */
static
BOOLEAN
UlpIsValidListPointer(
    IN PLIST_ENTRY Link
)
{
    ULONG_PTR Address = (ULONG_PTR)Link;
    ULONG i;

    if ((Address & (sizeof(PVOID) - 1)) != 0)
    {
        return FALSE;
    }

    for (i = 0; i < MaxPoolType; i++)
    {
        ULONG_PTR HeadStart = (ULONG_PTR)&UlPoolVector[i].ListHeads[0];
        ULONG_PTR HeadEnd = (ULONG_PTR)&UlPoolVector[i].ListHeads[POOL_MAX_BLOCKS];
        if (Address >= HeadStart && Address < HeadEnd &&
            ((Address - HeadStart) % sizeof(LIST_ENTRY)) == 0)
        {
            return TRUE;
        }
    }

    if (Address < 0xC1000000 || Address >= 0xFFC00000 ||
        (Address & (POOL_PAGE_SIZE - 1)) >
            POOL_PAGE_SIZE - sizeof(LIST_ENTRY) ||
        !MM_GET_PDE(Address)->bits.present ||
        !MM_GET_PTE(Address)->bits.present)
    {
        return FALSE;
    }

    return TRUE;
}

static
BOOLEAN
UlpIsValidPoolBlockHeader(
    IN PPOOL_HEADER Header
)
{
    ULONG_PTR Address = (ULONG_PTR)Header;
    ULONG PageOffset = Address & (POOL_PAGE_SIZE - 1);
    ULONG BlockIndex;

    if (Address < 0xC1000000 || Address >= 0xFFC00000 ||
        (Address & ((1UL << POOL_BLOCK_SHIFT) - 1)) != 0 ||
        !MM_GET_PDE(Address)->bits.present ||
        !MM_GET_PTE(Address)->bits.present)
    {
        return FALSE;
    }

    BlockIndex = PageOffset >> POOL_BLOCK_SHIFT;
    return Header->BlockSize != 0 &&
           Header->BlockSize < POOL_MAX_BLOCKS &&
           BlockIndex + Header->BlockSize <= POOL_MAX_BLOCKS - 1;
}

PLIST_ENTRY VEAPI UlpDecode(PLIST_ENTRY Link)
{
    return (PLIST_ENTRY)((ULONG_PTR)Link & ~1);
}

PLIST_ENTRY VEAPI UlpEncode(PLIST_ENTRY Link)
{
    return (PLIST_ENTRY)((ULONG_PTR)Link | 1);
}

static
ULONG 
UlpComputeHashForTag(ULONG Tag)
{
    // Hashing bitwise standar, disesuaikan dengan MASK array lu
    return (Tag ^ (Tag >> 16) ^ (Tag >> 8)) & POOL_TRACK_TABLE_MASK;
}

VOID 
VEAPI 
UlpUpdateTracker(ULONG Tag, POOL_TYPE PoolType, SIZE_T Bytes, BOOLEAN IsAllocation)
{
    ULONG HashIndex = UlpComputeHashForTag(Tag);
    ULONG CurrentIndex = HashIndex;
    KIRQL OldIrql;

    KsAcquireSpinLock(&UlPoolTrackerLock, &OldIrql);
    do {
        // 1. Ketemu Tag yang udah ada
        if (UlPoolTrackTable[CurrentIndex].Key == Tag) {
            if (IsAllocation) {
                if (PoolType == NonPagedPool) {
                    UlPoolTrackTable[CurrentIndex].NonPagedAllocs++;
                    UlPoolTrackTable[CurrentIndex].NonPagedBytes += (LONG)Bytes;
                } else {
                    UlPoolTrackTable[CurrentIndex].PagedAllocs++;
                    UlPoolTrackTable[CurrentIndex].PagedBytes += (LONG)Bytes;
                }
            } else {
                if (PoolType == NonPagedPool) {
                    UlPoolTrackTable[CurrentIndex].NonPagedFrees++;
                    UlPoolTrackTable[CurrentIndex].NonPagedBytes -= (LONG)Bytes;
                } else {
                    UlPoolTrackTable[CurrentIndex].PagedFrees++;
                    UlPoolTrackTable[CurrentIndex].PagedBytes -= (LONG)Bytes;
                }
            }
            KsReleaseSpinLock(&UlPoolTrackerLock, OldIrql);
            return;
        }

        // 2. Ketemu slot kosong atau nisan (Tombstone)
        if (UlPoolTrackTable[CurrentIndex].Key == 0 || 
            UlPoolTrackTable[CurrentIndex].Key == POOL_TRACK_TOMBSTONE) {
            
            if (IsAllocation) {
                UlPoolTrackTable[CurrentIndex].Key = Tag;
                if (PoolType == NonPagedPool) {
                    UlPoolTrackTable[CurrentIndex].NonPagedAllocs = 1;
                    UlPoolTrackTable[CurrentIndex].NonPagedBytes = (LONG)Bytes;
                    UlPoolTrackTable[CurrentIndex].NonPagedFrees = 0;
                } else {
                    UlPoolTrackTable[CurrentIndex].PagedAllocs = 1;
                    UlPoolTrackTable[CurrentIndex].PagedBytes = (LONG)Bytes;
                    UlPoolTrackTable[CurrentIndex].PagedFrees = 0;
                }
            }
            KsReleaseSpinLock(&UlPoolTrackerLock, OldIrql);
            return;
        }

        CurrentIndex = (CurrentIndex + 1) & POOL_TRACK_TABLE_MASK;
    } while (CurrentIndex != HashIndex);
    
    // Kalau tabel pool tag penuh (lebih dari 256 tag unik)
    KdPrintf("ULPOOL: Pool Tracker Table is Full! Cannot track Tag\n\r");
    KsReleaseSpinLock(&UlPoolTrackerLock, OldIrql);
}

static
VOID
UlpCheckLinksWithCaller(
    IN PLIST_ENTRY ListHead,
    IN ULONG_PTR Caller
)
{
    PLIST_ENTRY Flink = UlpDecode(ListHead->Flink);
    PLIST_ENTRY Blink = UlpDecode(ListHead->Blink);

    /* An empty list intentionally points both links back at its sentinel. */
    if (Flink == ListHead && Blink == ListHead)
    {
        return;
    }

    if (!UlpIsValidListPointer(Flink) ||
        !UlpIsValidListPointer(Blink))
    {
        DPRINT("ULPOOL: invalid list links head=%08X rawF=%08X rawB=%08X caller=%08X\n",
               (ULONG_PTR)ListHead,
               (ULONG_PTR)ListHead->Flink,
               (ULONG_PTR)ListHead->Blink,
               Caller);
        KsBugCheckEx(BAD_POOL_HEADER, 17, (ULONG_PTR)ListHead,
                     (ULONG_PTR)Flink, (ULONG_PTR)Blink);
        return;
    }

    if (UlpDecode(Flink->Blink) != ListHead ||
        UlpDecode(Blink->Flink) != ListHead)
    {
        DPRINT("ULPOOL: inconsistent list head=%08X rawF=%08X rawB=%08X "
               "fBlink=%08X bFlink=%08X caller=%08X\n",
               (ULONG_PTR)ListHead,
               (ULONG_PTR)ListHead->Flink,
               (ULONG_PTR)ListHead->Blink,
               (ULONG_PTR)Flink->Blink,
               (ULONG_PTR)Blink->Flink,
               Caller);
        KsBugCheckEx(BAD_POOL_HEADER, 18, (ULONG_PTR)ListHead,
                     (ULONG_PTR)UlpDecode(Flink->Blink),
                     (ULONG_PTR)UlpDecode(Blink->Flink));
        return;
    }
}

VOID VEAPI UlpCheckLinks(PLIST_ENTRY ListHead)
{
    UlpCheckLinksWithCaller(
        ListHead,
        (ULONG_PTR)__builtin_return_address(0));
}

VOID VEAPI UlpInitList(PLIST_ENTRY ListHead)
{
    ListHead->Flink = ListHead->Blink = UlpEncode(ListHead);
}

BOOLEAN VEAPI UlpIsListEmpty(PLIST_ENTRY ListHead)
{
    return (UlpDecode(ListHead->Flink) == ListHead);
}

VOID VEAPI UlpRemoveEntry(PLIST_ENTRY Entry)
{
    PLIST_ENTRY Blink, Flink;
    if (!UlpIsValidListPointer(Entry))
    {
        KsBugCheckEx(BAD_POOL_HEADER, 10, (ULONG_PTR)Entry, 0, 0);
        return;
    }

    Flink = UlpDecode(Entry->Flink);
    Blink = UlpDecode(Entry->Blink);

    if (!UlpIsValidListPointer(Flink) ||
        !UlpIsValidListPointer(Blink) ||
        UlpDecode(Flink->Blink) != Entry ||
        UlpDecode(Blink->Flink) != Entry)
    {
        KsBugCheckEx(BAD_POOL_HEADER, 10, (ULONG_PTR)Entry,
                     (ULONG_PTR)Flink, (ULONG_PTR)Blink);
        return;
    }

    Flink->Blink = UlpEncode(Blink);
    Blink->Flink = UlpEncode(Flink);
}

PLIST_ENTRY VEAPI UlpRemoveHead(PLIST_ENTRY ListHead)
{
    PLIST_ENTRY Entry, Flink;
    UlpCheckLinksWithCaller(
        ListHead,
        (ULONG_PTR)__builtin_return_address(0));
    Entry = UlpDecode(ListHead->Flink);
    if (Entry == ListHead)
    {
        KsBugCheckEx(BAD_POOL_HEADER, 11, (ULONG_PTR)ListHead, 0, 0);
        return NULL;
    }
    Flink = UlpDecode(Entry->Flink);
    ListHead->Flink = UlpEncode(Flink);
    Flink->Blink = UlpEncode(ListHead);
    return Entry;
}

PLIST_ENTRY VEAPI UlpRemoveTail(PLIST_ENTRY ListHead)
{
    PLIST_ENTRY Entry, Blink;
    Entry = UlpDecode(ListHead->Blink);
    Blink = UlpDecode(Entry->Blink);
    ListHead->Blink = UlpEncode(Blink);
    Blink->Flink = UlpEncode(ListHead);
    return Entry;
}

VOID VEAPI UlpInsertTail(PLIST_ENTRY ListHead, PLIST_ENTRY Entry)
{
    PLIST_ENTRY Blink;
    UlpCheckLinks(ListHead);
    
    Blink = UlpDecode(ListHead->Blink);
    Entry->Flink = UlpEncode(ListHead);
    Entry->Blink = UlpEncode(Blink);
    Blink->Flink = UlpEncode(Entry);
    ListHead->Blink = UlpEncode(Entry);
    
    UlpCheckLinks(ListHead);
}

VOID VEAPI UlpInsertHead(PLIST_ENTRY ListHead, PLIST_ENTRY Entry)
{
    PLIST_ENTRY Flink;
    UlpCheckLinks(ListHead);
    
    Flink = UlpDecode(ListHead->Flink);
    Entry->Flink = UlpEncode(Flink);
    Entry->Blink = UlpEncode(ListHead);
    Flink->Blink = UlpEncode(Entry);
    ListHead->Flink = UlpEncode(Entry);
    
    UlpCheckLinks(ListHead);
}

VOID VEAPI UlpCheckHeader(PPOOL_HEADER Entry)
{
    PPOOL_HEADER PreviousEntry, NextEntry;

    /* Apakah ada blok sebelum ini? */
    if (Entry->PreviousSize)
    {
        PreviousEntry = POOL_PREV_BLOCK(Entry);

        /* Blok ini dan tetangga kirinya wajib ada di Page yang sama (4KB) */
        if (PAGE_ALIGN(Entry) != PAGE_ALIGN(PreviousEntry))
        {
            KsBugCheckEx(BAD_POOL_HEADER, 6, (ULONG_PTR)PreviousEntry, __LINE__, (ULONG_PTR)Entry);
        }

        /* Validasi cross-check ukuran */
        if (PreviousEntry->BlockSize != Entry->PreviousSize)
        {
            KsBugCheckEx(BAD_POOL_HEADER, 5, (ULONG_PTR)PreviousEntry, __LINE__, (ULONG_PTR)Entry);
        }
    }
    else if (PAGE_ALIGN(Entry) != Entry)
    {
        /* Kalau ini blok pertama (gak ada tetangga kiri), dia wajib nempel di awal Page */
        KsBugCheckEx(BAD_POOL_HEADER, 7, 0, __LINE__, (ULONG_PTR)Entry);
    }

    /* Ukuran gak boleh 0 (Korup) */
    if (!Entry->BlockSize)
    {
        KsBugCheckEx(BAD_POOL_HEADER, 8, 0, __LINE__, (ULONG_PTR)Entry);
    }

    /* Cek tetangga kanan */
    NextEntry = POOL_NEXT_BLOCK(Entry);

    /* Kalau tetangga kanan masih di page yang sama, periksa dia juga */
    if (PAGE_ALIGN(NextEntry) != NextEntry)
    {
        if (PAGE_ALIGN(Entry) != PAGE_ALIGN(NextEntry))
        {
            KsBugCheckEx(BAD_POOL_HEADER, 9, (ULONG_PTR)NextEntry, __LINE__, (ULONG_PTR)Entry);
        }

        if (NextEntry->PreviousSize != Entry->BlockSize)
        {
            KsBugCheckEx(BAD_POOL_HEADER, 5, (ULONG_PTR)NextEntry, __LINE__, (ULONG_PTR)Entry);
        }
    }
}

static
ULONG 
UlpComputeHashForVa(PVOID Va)
{
    // Geser 12 bit (Page-Aligned) & mask sesuai batas array
    return ((ULONG_PTR)Va >> 12) & POOL_BIG_PAGE_HASH_MASK;
}

/* PUBLIC */

VOID 
VEAPI 
UlInitializePoolDescriptor(
    PPOOL_DESCRIPTOR PoolDescriptor,
    POOL_TYPE PoolType,
    ULONG PoolIndex,
    ULONG Threshold,
    PVOID PoolLock
)
{
    // 1. Setup metadata dasar
    PoolDescriptor->PoolType = PoolType;
    PoolDescriptor->PoolIndex = PoolIndex;
    PoolDescriptor->Threshold = Threshold;
    PoolDescriptor->LockAddress = PoolLock;

    // 2. Reset semua statistik akuntansi
    PoolDescriptor->RunningAllocs = 0;
    PoolDescriptor->RunningDeAllocs = 0;
    PoolDescriptor->TotalPages = 0;
    PoolDescriptor->TotalBytesAllocated = 0;
    PoolDescriptor->TotalBytesMapped = 0;
    PoolDescriptor->TotalBigPages = 0;

    // 3. Inisialisasi 512 ListHead (Looping pakai pointer math kayak NT)
    PLIST_ENTRY NextEntry = PoolDescriptor->ListHeads;
    PLIST_ENTRY LastEntry = NextEntry + POOL_MAX_BLOCKS;
    
    while (NextEntry < LastEntry) {
        InitializeListHead(NextEntry);
        NextEntry++;
    }
}

VOID 
VEAPI 
UlInitializePools(VOID)
{
    KsInitializeSpinlock(&UlPoolLocks[NonPagedPool]);
    KsInitializeSpinlock(&UlPoolLocks[PagedPool]);
    KsInitializeSpinlock(&UlPoolTrackerLock);
    KsInitializeSpinlock(&UlBigPageTableLock);

    // Inisialisasi NonPagedPool (Biasanya ditaruh di memori fisik yang nggak pernah di-swap)
    UlInitializePoolDescriptor(
        &UlPoolVector[NonPagedPool], 
        NonPagedPool, 
        0, 
        0, 
        &UlPoolLocks[NonPagedPool]
    );

    // Inisialisasi PagedPool (Nanti kalau Virtual Memory / Paging lu udah support Swap)
    UlInitializePoolDescriptor(
        &UlPoolVector[PagedPool], 
        PagedPool, 
        0, 
        0, 
        &UlPoolLocks[PagedPool]
    );

    UlPoolInitialized = TRUE;
}

BOOLEAN
VEAPI
UlExpandPool(POOL_TYPE PoolType)
{
    KIRQL OldIrql;

    if (PoolType >= MaxPoolType)
    {
        return FALSE;
    }

    PPOOL_DESCRIPTOR Descriptor = &UlPoolVector[PoolType];

    PVOID NewPage = MmAllocatePoolPage(PoolType);
    if (NewPage == NULL) {
        return FALSE; // Bener-bener kehabisan memori fisik/virtual
    }

    PPOOL_HEADER Header = (PPOOL_HEADER)NewPage;
    Header->PreviousSize = 0; // Tidak ada tetangga kiri (Awal halaman)
    Header->BlockSize = POOL_MAX_BLOCKS - 1; // 511 blok
    Header->PoolType = 0; // 0 = Kosong
    Header->PoolTag = 'eerF'; // 'Free'

    PPOOL_HEADER EndMarker = Header + (POOL_MAX_BLOCKS - 1);
    EndMarker->PreviousSize = POOL_MAX_BLOCKS - 1; // Tetangga kirinya 511 blok
    EndMarker->BlockSize = 1;
    // Trik NT: Tandai sebagai tipe Aktif biar fitur Coalescing gak nyaplok batas page!
    EndMarker->PoolType = PoolType + 1; 
    EndMarker->PoolTag = ' dnE'; // 'End ' (Marker)

    PPOOL_FREE_BLOCK FreeBlock = (PPOOL_FREE_BLOCK)Header;
    KsAcquireSpinLock(&UlPoolLocks[PoolType], &OldIrql);
    UlpInsertTail(&Descriptor->ListHeads[Header->BlockSize], &FreeBlock->ListEntry);

    Descriptor->TotalPages++;
    Descriptor->TotalBytesMapped += POOL_PAGE_SIZE;
    KsReleaseSpinLock(&UlPoolLocks[PoolType], OldIrql);

    return TRUE;    
}

PVOID 
VEAPI 
UlAllocatePoolWithTag(POOL_TYPE PoolType, ULONG NumberOfBytes, ULONG Tag) 
{
    ULONG BlockSize;
    PPOOL_HEADER Header, NewHeader;
    PPOOL_FREE_BLOCK FreeBlock;
    ULONG TotalBytes;
    PPOOL_DESCRIPTOR Descriptor;
    KIRQL OldIrql;

    if (PoolType >= MaxPoolType ||
        NumberOfBytes > (ULONG)-1 - sizeof(POOL_HEADER) - 7)
    {
        return NULL;
    }

    Descriptor = &UlPoolVector[PoolType];
    TotalBytes = NumberOfBytes + sizeof(POOL_HEADER);
    TotalBytes = (TotalBytes + 7) & ~7;
    BlockSize = TotalBytes >> POOL_BLOCK_SHIFT;

    if (BlockSize >= POOL_MAX_BLOCKS) {
        ULONG NumberOfPages = (NumberOfBytes - 1) / POOL_PAGE_SIZE + 1;

        PVOID Va = MmAllocatePoolPages(PoolType, NumberOfPages);
        if (Va == NULL) return NULL; // Out of Memory

        BOOLEAN Tracked = FALSE;
        ULONG HashIndex = UlpComputeHashForVa(Va);
        ULONG CurrentIndex = HashIndex;
        KsAcquireSpinLock(&UlBigPageTableLock, &OldIrql);

        do {
            if (UlBigPageTable[CurrentIndex].Va == NULL ||
                UlBigPageTable[CurrentIndex].Va == BIG_PAGE_TOMBSTONE) {
                UlBigPageTable[CurrentIndex].Va = Va;
                UlBigPageTable[CurrentIndex].Key = Tag;
                UlBigPageTable[CurrentIndex].NumberOfPages = NumberOfPages;
                UlBigPageTable[CurrentIndex].NumberOfBytes = NumberOfBytes;
                UlBigPageTable[CurrentIndex].PoolType = PoolType;
                Tracked = TRUE;
                break;
            }
            CurrentIndex = (CurrentIndex + 1) & POOL_BIG_PAGE_HASH_MASK;
        } while (CurrentIndex != HashIndex);

        if (Tracked)
        {
            UlpBigPageTableCount++;
        }
        KsReleaseSpinLock(&UlBigPageTableLock, OldIrql);

        if (!Tracked) {
            MmFreePoolPages(Va, NumberOfPages);
            KdPrintf("ULPOOL: Big Page Table is Full!\n\r");
            return NULL; 
        }

        UlpUpdateTracker(Tag, PoolType, NumberOfBytes, TRUE);

        KsAcquireSpinLock(&UlPoolLocks[PoolType], &OldIrql);
        Descriptor->TotalBigPages += NumberOfPages;
        Descriptor->TotalBytesMapped += (NumberOfPages * 4096);
        Descriptor->TotalBytesAllocated += NumberOfBytes;
        KsReleaseSpinLock(&UlPoolLocks[PoolType], OldIrql);

        return Va;
    }

RetryAllocation:
    KsAcquireSpinLock(&UlPoolLocks[PoolType], &OldIrql);
    ULONG CurrentIndex = BlockSize;
    while (CurrentIndex < POOL_MAX_BLOCKS) {
        if (!UlpIsListEmpty(&Descriptor->ListHeads[CurrentIndex])) {
            PLIST_ENTRY Entry = UlpRemoveHead(&Descriptor->ListHeads[CurrentIndex]);
            FreeBlock = CONTAINING_RECORD(Entry, POOL_FREE_BLOCK, ListEntry);
            Header = &FreeBlock->Header;

            if (!UlpIsValidPoolBlockHeader(Header))
            {
                KsBugCheckEx(BAD_POOL_HEADER, 12, (ULONG_PTR)Header,
                             Header->BlockSize, Header->PreviousSize);
                KsReleaseSpinLock(&UlPoolLocks[PoolType], OldIrql);
                return NULL;
            }

            UlpCheckHeader(Header);

            if (Header->BlockSize > BlockSize + 1) {
                ULONG RemainingBlocks = Header->BlockSize - BlockSize;
                Header->BlockSize = BlockSize;

                NewHeader = Header + BlockSize;
                NewHeader->BlockSize = RemainingBlocks;
                NewHeader->PreviousSize = BlockSize;
                NewHeader->PoolType = 0; // 0 = Kosong
                
                PPOOL_HEADER NextHeader = NewHeader + RemainingBlocks;
                if (PAGE_ALIGN(NextHeader) != NextHeader) {
                    NextHeader->PreviousSize = RemainingBlocks; 
                }

                PPOOL_FREE_BLOCK NewFreeBlock = (PPOOL_FREE_BLOCK)NewHeader;
                UlpInsertTail(&Descriptor->ListHeads[RemainingBlocks], &NewFreeBlock->ListEntry);
            }

            Header->PoolType = PoolType + 1;
            Header->PoolTag = Tag;

            Descriptor->RunningAllocs++;
            Descriptor->TotalBytesAllocated += (Header->BlockSize << POOL_BLOCK_SHIFT);

            UlpUpdateTracker(Tag, PoolType, (Header->BlockSize << POOL_BLOCK_SHIFT), TRUE);
            KsReleaseSpinLock(&UlPoolLocks[PoolType], OldIrql);
            return (PVOID)(Header + 1);
        }
        CurrentIndex++;
    }

    KsReleaseSpinLock(&UlPoolLocks[PoolType], OldIrql);
    if (UlExpandPool(PoolType)) {
        goto RetryAllocation;
    }
    
    // TODO: Kalau semua list kosong, minta Page baru (UlExpandPool)
    return NULL; 
}

VOID 
VEAPI 
UlFreePoolWithTag(PVOID P, ULONG Tag) 
{
    if (P == NULL) return;

    if (((ULONG_PTR)P & (POOL_PAGE_SIZE - 1)) == 0) {
        BOOLEAN Found = FALSE;
        ULONG ActualTag = Tag;
        ULONG PagesToFree = 0;
        ULONG BytesToFree = 0;
        POOL_TYPE PoolType = MaxPoolType;
        KIRQL OldIrql;
        ULONG HashIndex = UlpComputeHashForVa(P);
        ULONG CurrentIndex = HashIndex;

        KsAcquireSpinLock(&UlBigPageTableLock, &OldIrql);
        do {
            if (UlBigPageTable[CurrentIndex].Va == P) {
                if (Tag != 0 && UlBigPageTable[CurrentIndex].Key != Tag) {
                    KsBugCheckEx(BAD_POOL_CALLER, 3, (ULONG_PTR)P, Tag, UlBigPageTable[CurrentIndex].Key);
                    KsReleaseSpinLock(&UlBigPageTableLock, OldIrql);
                    return;
                }

                ActualTag = UlBigPageTable[CurrentIndex].Key;
                PagesToFree = UlBigPageTable[CurrentIndex].NumberOfPages;
                BytesToFree = UlBigPageTable[CurrentIndex].NumberOfBytes;
                PoolType = UlBigPageTable[CurrentIndex].PoolType;
                if (PoolType >= MaxPoolType || PagesToFree == 0)
                {
                    KsBugCheckEx(BAD_POOL_HEADER, 13, (ULONG_PTR)P,
                                 PoolType, PagesToFree);
                    KsReleaseSpinLock(&UlBigPageTableLock, OldIrql);
                    return;
                }
                UlBigPageTable[CurrentIndex].Va = BIG_PAGE_TOMBSTONE;
                UlBigPageTable[CurrentIndex].Key = 0;
                UlBigPageTable[CurrentIndex].NumberOfPages = 0;
                UlBigPageTable[CurrentIndex].NumberOfBytes = 0;
                UlBigPageTable[CurrentIndex].PoolType = 0;
                Found = TRUE;
                UlpBigPageTableCount--;

                break;
            }
            
            CurrentIndex = (CurrentIndex + 1) & POOL_BIG_PAGE_HASH_MASK;
            
            // Early exit jika menemukan slot kosong berurutan (berarti data tidak ada)
            if (UlBigPageTable[CurrentIndex].Va == NULL) {
                break;
            }

        } while (CurrentIndex != HashIndex);

        KsReleaseSpinLock(&UlBigPageTableLock, OldIrql);

        if (!Found) {
            KsBugCheckEx(BAD_POOL_CALLER, 0xA, (ULONG_PTR)P, 0, 0);
            return;
        }

        UlpUpdateTracker(ActualTag, PoolType, BytesToFree, FALSE);
        KsAcquireSpinLock(&UlPoolLocks[PoolType], &OldIrql);
        UlPoolVector[PoolType].TotalBigPages -= PagesToFree;
        UlPoolVector[PoolType].TotalBytesMapped -= PagesToFree * POOL_PAGE_SIZE;
        UlPoolVector[PoolType].TotalBytesAllocated -= BytesToFree;
        KsReleaseSpinLock(&UlPoolLocks[PoolType], OldIrql);

        MmFreePoolPages(P, PagesToFree);
        return;
    }

    if ((ULONG_PTR)P & 7) {
        KsBugCheckEx(BAD_POOL_CALLER, 1, (ULONG_PTR)P, 0, 0); 
        return;
    }

    PPOOL_HEADER Header = (PPOOL_HEADER)P - 1;
    UCHAR PoolIndex = Header->PoolType;
    if (PoolIndex == 0 || PoolIndex > MaxPoolType) {
        KsBugCheckEx(BAD_POOL_CALLER, 4, (ULONG_PTR)Header,
                     PoolIndex, MaxPoolType);
        return;
    }
    POOL_TYPE PoolType = (POOL_TYPE)(PoolIndex - 1);
    PPOOL_DESCRIPTOR Descriptor = &UlPoolVector[PoolType];
    KIRQL OldIrql;
    BOOLEAN FreeWholePage = FALSE;
    ULONG ActualTag;
    ULONG AllocationSize;
    ULONG CurrentSize;

    KsAcquireSpinLock(&UlPoolLocks[PoolType], &OldIrql);
    if (Header->PoolType != PoolIndex ||
        Header->PoolType == 0 || Header->PoolTag == 'eerF') {
        KsBugCheckEx(BAD_POOL_CALLER, 2, (ULONG_PTR)Header, Tag, Header->PoolTag);
        KsReleaseSpinLock(&UlPoolLocks[PoolType], OldIrql);
        return;
    }

    if (Tag != 0 && Header->PoolTag != Tag) {
        KsBugCheckEx(BAD_POOL_CALLER, 3, (ULONG_PTR)Header, Tag, Header->PoolTag);
        KsReleaseSpinLock(&UlPoolLocks[PoolType], OldIrql);
        return;
    }

    ActualTag = Header->PoolTag; 

    if (!UlpIsValidPoolBlockHeader(Header)) {
        KsBugCheckEx(BAD_POOL_HEADER, 12, (ULONG_PTR)Header,
                     Header->BlockSize, Header->PreviousSize);
        KsReleaseSpinLock(&UlPoolLocks[PoolType], OldIrql);
        return;
    }

    UlpCheckHeader(Header);
    AllocationSize = Header->BlockSize;
    CurrentSize = AllocationSize;
    UlpUpdateTracker(ActualTag, PoolType,
                     AllocationSize << POOL_BLOCK_SHIFT, FALSE);

    if (Header->PreviousSize != 0) {
        PPOOL_HEADER PrevHeader = POOL_PREV_BLOCK(Header);
        if (PrevHeader->PoolType == 0) {
            if (!UlpIsValidPoolBlockHeader(PrevHeader))
            {
                KsBugCheckEx(BAD_POOL_HEADER, 12, (ULONG_PTR)PrevHeader,
                             PrevHeader->BlockSize, PrevHeader->PreviousSize);
                KsReleaseSpinLock(&UlPoolLocks[PoolType], OldIrql);
                return;
            }
            PPOOL_FREE_BLOCK PrevFree = (PPOOL_FREE_BLOCK)PrevHeader;
            UlpRemoveEntry(&PrevFree->ListEntry);

            PrevHeader->BlockSize += CurrentSize;
            Header = PrevHeader;
            CurrentSize = Header->BlockSize;
        }
    }

    PPOOL_HEADER NextHeader = POOL_NEXT_BLOCK(Header);
    if (PAGE_ALIGN(NextHeader) != NextHeader) {
        if (NextHeader->PoolType == 0) {
            if (!UlpIsValidPoolBlockHeader(NextHeader))
            {
                KsBugCheckEx(BAD_POOL_HEADER, 12, (ULONG_PTR)NextHeader,
                             NextHeader->BlockSize, NextHeader->PreviousSize);
                KsReleaseSpinLock(&UlPoolLocks[PoolType], OldIrql);
                return;
            }
            PPOOL_FREE_BLOCK NextFree = (PPOOL_FREE_BLOCK)NextHeader;
            UlpRemoveEntry(&NextFree->ListEntry);

            Header->BlockSize += NextHeader->BlockSize;
            CurrentSize = Header->BlockSize;
        }
    }

    PPOOL_HEADER NewNextHeader = POOL_NEXT_BLOCK(Header);
    if (PAGE_ALIGN(NewNextHeader) != NewNextHeader) {
        NewNextHeader->PreviousSize = CurrentSize;
    }

    Header->PoolType = 0;
    Header->PoolTag = 'eerF';

    if (CurrentSize == (POOL_MAX_BLOCKS - 1)) {
        FreeWholePage = TRUE;
        Descriptor->TotalPages--;
        Descriptor->TotalBytesMapped -= POOL_PAGE_SIZE;
    } else {
        PPOOL_FREE_BLOCK FreeBlock = (PPOOL_FREE_BLOCK)Header;
        UlpInsertHead(&Descriptor->ListHeads[CurrentSize], &FreeBlock->ListEntry);
    }

    Descriptor->RunningDeAllocs++;
    Descriptor->TotalBytesAllocated -= (AllocationSize << POOL_BLOCK_SHIFT);
    KsReleaseSpinLock(&UlPoolLocks[PoolType], OldIrql);

    if (FreeWholePage)
    {
        MmFreePoolPage((PVOID)Header);
    }
}

VOID
VEAPI
UlFreePool(PVOID P)
{
    UlFreePoolWithTag(P, 0);
}

PVOID 
VEAPI 
UlAllocatePoolZero(POOL_TYPE PoolType, ULONG NumberOfBytes, ULONG Tag)
{
    PVOID P = UlAllocatePoolWithTag(PoolType, NumberOfBytes, Tag);
    if (P) RtlZeroMemory(P, NumberOfBytes);
    return P;
}

VOID 
VEAPI 
UlDumpPoolTracker(VOID)
{
    KdPrintf("\n\r=== VeaOS Pool Tracker Dump ===\n\r");
    KdPrintf(" TAG  | NP Allocs | NP Frees | NP Bytes | P Allocs | P Frees | P Bytes \n\r");
    KdPrintf("-----------------------------------------------------------------------\n\r");

    // Loop ke seluruh ukuran tabel tracker
    for (ULONG i = 0; i < POOL_TRACK_TABLE_SIZE; i++) {
        ULONG Tag = UlPoolTrackTable[i].Key;

        // Abaikan slot yang kosong (0) atau tombstone (POOL_TRACK_TOMBSTONE)
        if (Tag == 0 || Tag == POOL_TRACK_TOMBSTONE) {
            continue;
        }

        // Ekstrak 4 byte dari ULONG menjadi karakter string yang bisa dibaca
        CHAR TagStr[5];
        TagStr[0] = (CHAR)(Tag & 0xFF);
        TagStr[1] = (CHAR)((Tag >> 8) & 0xFF);
        TagStr[2] = (CHAR)((Tag >> 16) & 0xFF);
        TagStr[3] = (CHAR)((Tag >> 24) & 0xFF);
        TagStr[4] = '\0';

        // Filter karakter non-printable (jaga-jaga kalau tag-nya berupa angka biner acak)
        for (int j = 0; j < 4; j++) {
            if (TagStr[j] < 32 || TagStr[j] > 126) {
                TagStr[j] = '.';
            }
        }

        // Ambil metrik dari NonPagedPool[cite: 10]
        LONG NPAllocs = UlPoolTrackTable[i].NonPagedAllocs;
        LONG NPFrees  = UlPoolTrackTable[i].NonPagedFrees;
        LONG NPBytes  = UlPoolTrackTable[i].NonPagedBytes;
        
        // Ambil metrik dari PagedPool[cite: 10]
        LONG PAllocs = UlPoolTrackTable[i].PagedAllocs;
        LONG PFrees  = UlPoolTrackTable[i].PagedFrees;
        LONG PBytes  = UlPoolTrackTable[i].PagedBytes;

        // Print format rata kanan-kiri yang rapi
        KdPrintf(" %4s | %9d | %8d | %8d | %8d | %7d | %7d \n\r",
                  TagStr, NPAllocs, NPFrees, NPBytes, PAllocs, PFrees, PBytes);
    }
    KdPrintf("=======================================================================\n\r");
}