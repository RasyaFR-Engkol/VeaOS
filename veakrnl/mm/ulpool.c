#include <veakrnl.h>

POOL_DESCRIPTOR UlPoolVector[MaxPoolType];
POOL_TRACKER_BIG_PAGES UlBigPageTable[POOL_MAX_BIG_PAGES];
POOL_TRACKER_TABLE UlPoolTrackTable[POOL_TRACK_TABLE_SIZE];
ULONG UlpBigPageTableCount = 0;

/* PRIVATE */
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
            return;
        }

        CurrentIndex = (CurrentIndex + 1) & POOL_TRACK_TABLE_MASK;
    } while (CurrentIndex != HashIndex);
    
    // Kalau tabel pool tag penuh (lebih dari 256 tag unik)
    KdPrintf("ULPOOL: Pool Tracker Table is Full! Cannot track Tag\n\r");
}

VOID VEAPI UlpCheckLinks(PLIST_ENTRY ListHead)
{
    if ((UlpDecode(UlpDecode(ListHead->Flink)->Blink) != ListHead) ||
        (UlpDecode(UlpDecode(ListHead->Blink)->Flink) != ListHead))
    {
        // BugCheck kalau pointer corrupt / kena buffer overflow
        KsBugCheck(BAD_POOL_HEADER, 3, (ULONG_PTR)ListHead,
                     (ULONG_PTR)UlpDecode(UlpDecode(ListHead->Flink)->Blink),
                     (ULONG_PTR)UlpDecode(UlpDecode(ListHead->Blink)->Flink));
    }
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
    Flink = UlpDecode(Entry->Flink);
    Blink = UlpDecode(Entry->Blink);
    Flink->Blink = UlpEncode(Blink);
    Blink->Flink = UlpEncode(Flink);
}

PLIST_ENTRY VEAPI UlpRemoveHead(PLIST_ENTRY ListHead)
{
    PLIST_ENTRY Entry, Flink;
    Entry = UlpDecode(ListHead->Flink);
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
    // Inisialisasi NonPagedPool (Biasanya ditaruh di memori fisik yang nggak pernah di-swap)
    UlInitializePoolDescriptor(
        &UlPoolVector[NonPagedPool], 
        NonPagedPool, 
        0, 
        0, 
        NULL // Todo: Kasih KSPIN_LOCK kalau udah masuk bab Multiprocessing
    );

    // Inisialisasi PagedPool (Nanti kalau Virtual Memory / Paging lu udah support Swap)
    UlInitializePoolDescriptor(
        &UlPoolVector[PagedPool], 
        PagedPool, 
        0, 
        0, 
        NULL
    );
}

BOOLEAN
VEAPI
UlExpandPool(POOL_TYPE PoolType)
{
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
    UlpInsertTail(&Descriptor->ListHeads[Header->BlockSize], &FreeBlock->ListEntry);

    Descriptor->TotalPages++;
    Descriptor->TotalBytesMapped += POOL_PAGE_SIZE;

    return TRUE;    
}

PVOID 
VEAPI 
UlAllocatePoolWithTag(POOL_TYPE PoolType, ULONG NumberOfBytes, ULONG Tag) 
{
    ULONG BlockSize;
    PPOOL_HEADER Header, NewHeader;
    PPOOL_FREE_BLOCK FreeBlock;
    
    // 1. Ambil Descriptor yang sesuai dengan tipe memori yang diminta
    PPOOL_DESCRIPTOR Descriptor = &UlPoolVector[PoolType];

    ULONG TotalBytes = NumberOfBytes + sizeof(POOL_HEADER);
    TotalBytes = (TotalBytes + 7) & ~7;
    BlockSize = TotalBytes >> POOL_BLOCK_SHIFT; 

    if (BlockSize >= POOL_MAX_BLOCKS) {
        if (UlpBigPageTableCount >= POOL_MAX_BIG_PAGES) {
            KdPrintf("ULPOOL: Big Page Table is 100%% Full!\n\r");
            return NULL;
        }

        ULONG NumberOfPages = (NumberOfBytes + 4095) / 4096;

        PVOID Va = MmAllocatePoolPages(PoolType, NumberOfPages);
        if (Va == NULL) return NULL; // Out of Memory

        BOOLEAN Tracked = FALSE;
        ULONG HashIndex = UlpComputeHashForVa(Va);
        ULONG CurrentIndex = HashIndex;

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

        if (!Tracked) {
            MmFreePoolPages(Va, NumberOfPages);
            KdPrintf("ULPOOL: Big Page Table is Full!\n\r");
            return NULL; 
        }

        UlpBigPageTableCount++;
        UlpUpdateTracker(Tag, PoolType, NumberOfBytes, TRUE);

        Descriptor->TotalBigPages += NumberOfPages;
        Descriptor->TotalBytesMapped += (NumberOfPages * 4096);
        Descriptor->TotalBytesAllocated += NumberOfBytes;

        return Va;
    }

RetryAllocation:
    ULONG CurrentIndex = BlockSize;
    while (CurrentIndex < POOL_MAX_BLOCKS) {
        
        // Pake fungsi cek yang aman dari eksploitasi
        if (!UlpIsListEmpty(&Descriptor->ListHeads[CurrentIndex])) {
            
            // Cabut pakai Safe Unlinking
            PLIST_ENTRY Entry = UlpRemoveHead(&Descriptor->ListHeads[CurrentIndex]);
            FreeBlock = CONTAINING_RECORD(Entry, POOL_FREE_BLOCK, ListEntry);
            Header = &FreeBlock->Header;

            // Validasi header sebelum dipakai (Jangan-jangan corrupt pas nganggur)
            UlpCheckHeader(Header);

            // Logika Splitting (Membelah)
            if (Header->BlockSize > BlockSize + 1) {
                ULONG RemainingBlocks = Header->BlockSize - BlockSize;
                
                Header->BlockSize = BlockSize;

                NewHeader = Header + BlockSize;
                NewHeader->BlockSize = RemainingBlocks;
                NewHeader->PreviousSize = BlockSize;
                NewHeader->PoolType = 0; // 0 = Kosong
                
                PPOOL_HEADER NextHeader = NewHeader + RemainingBlocks;
                if (PAGE_ALIGN(NextHeader) != NextHeader) { // Kalau belum ujung page
                    NextHeader->PreviousSize = RemainingBlocks; 
                }

                // Masukkan sisa blok ke list dengan aman
                PPOOL_FREE_BLOCK NewFreeBlock = (PPOOL_FREE_BLOCK)NewHeader;
                UlpInsertTail(&Descriptor->ListHeads[RemainingBlocks], &NewFreeBlock->ListEntry);
            }

            // Siapkan blok untuk user
            Header->PoolType = PoolType + 1; // +1 menandakan blok ini aktif
            Header->PoolTag = Tag;

            // Update statistik (Accounting)
            Descriptor->RunningAllocs++;
            Descriptor->TotalBytesAllocated += (Header->BlockSize << POOL_BLOCK_SHIFT);

            UlpUpdateTracker(Tag, PoolType, (Header->BlockSize << POOL_BLOCK_SHIFT), TRUE);

            return (PVOID)(Header + 1);
        }
        CurrentIndex++;
    }

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

    // CEK BIG PAGE: Apakah alamat P sejajar dengan batas 4KB (Page-Aligned)?
    if (((ULONG_PTR)P & 4095) == 0) {
        
        BOOLEAN Found = FALSE;
        ULONG HashIndex = UlpComputeHashForVa(P);
        ULONG CurrentIndex = HashIndex;

        // Linear Probing Lookup
        do {
            if (UlBigPageTable[CurrentIndex].Va == P) {
                
                if (UlBigPageTable[CurrentIndex].Key != Tag) {
                    KsBugCheckEx(BAD_POOL_CALLER, 3, (ULONG_PTR)P, Tag, UlBigPageTable[CurrentIndex].Key);
                }

                ULONG PagesToFree = UlBigPageTable[CurrentIndex].NumberOfPages;
                ULONG BytesToFree = UlBigPageTable[CurrentIndex].NumberOfBytes;
                PPOOL_DESCRIPTOR BigDescriptor = &UlPoolVector[UlBigPageTable[CurrentIndex].PoolType];

                UlpUpdateTracker(Tag, UlBigPageTable[CurrentIndex].PoolType, BytesToFree, FALSE);

                // Bersihkan slot
                UlBigPageTable[CurrentIndex].Va = BIG_PAGE_TOMBSTONE;
                UlBigPageTable[CurrentIndex].Key = 0;
                UlBigPageTable[CurrentIndex].NumberOfPages = 0;
                UlBigPageTable[CurrentIndex].NumberOfBytes = 0;
                UlBigPageTable[CurrentIndex].PoolType = 0; 

                MmFreePoolPages(P, PagesToFree);
                Found = TRUE;

                UlpBigPageTableCount--;

                BigDescriptor->TotalBigPages -= PagesToFree;
                BigDescriptor->TotalBytesMapped -= (PagesToFree * 4096);
                BigDescriptor->TotalBytesAllocated -= BytesToFree;

                break;
            }
            
            CurrentIndex = (CurrentIndex + 1) & POOL_BIG_PAGE_HASH_MASK;
            
            // Early exit jika menemukan slot kosong berurutan (berarti data tidak ada)
            if (UlBigPageTable[CurrentIndex].Va == NULL) {
                break;
            }

        } while (CurrentIndex != HashIndex);

        if (!Found) {
            KsBugCheckEx(BAD_POOL_CALLER, 0xA, (ULONG_PTR)P, 0, 0);
        }
        return;
    }

    if ((ULONG_PTR)P & 7) {
        // BAD_POOL_CALLER, Subcode 1: Bad Alignment
        KsBugCheckEx(BAD_POOL_CALLER, 1, (ULONG_PTR)P, 0, 0); 
    }

    PPOOL_HEADER Header = (PPOOL_HEADER)P - 1;

    if (Header->PoolType == 0 || Header->PoolTag == 'eerF') {
        // BAD_POOL_CALLER, Subcode 2: Double Free
        KsBugCheckEx(BAD_POOL_CALLER, 2, (ULONG_PTR)Header, Tag, Header->PoolTag); 
    }

    // 1. Cek keamanan tingkat dewa sebelum di-free!
    if (Header->PoolTag != Tag) {
        // BAD_POOL_CALLER, Subcode 3: Tag Mismatch
        KsBugCheckEx(BAD_POOL_CALLER, 3, (ULONG_PTR)Header, Tag, Header->PoolTag); 
    }

    if (Header->PoolType > (MaxPoolType + 1)) {
        KsBugCheckEx(BAD_POOL_CALLER, 4, (ULONG_PTR)Header, Header->PoolType, MaxPoolType);
    }

    UlpCheckHeader(Header); // Validasi integrity tetangganya

    // 2. Dapatkan Descriptor-nya (Ingat, tipe aktif itu aslinya tipe pool + 1)
    POOL_TYPE PoolType = Header->PoolType - 1;
    PPOOL_DESCRIPTOR Descriptor = &UlPoolVector[PoolType];

    ULONG CurrentSize = Header->BlockSize;

    UlpUpdateTracker(Tag, PoolType, (CurrentSize << POOL_BLOCK_SHIFT), FALSE);

    // 3. Coalesce Tetangga Kiri
    if (Header->PreviousSize != 0) {
        PPOOL_HEADER PrevHeader = POOL_PREV_BLOCK(Header);
        
        if (PrevHeader->PoolType == 0) { // Kosong
            PPOOL_FREE_BLOCK PrevFree = (PPOOL_FREE_BLOCK)PrevHeader;
            
            // Safe Unlinking!
            UlpRemoveEntry(&PrevFree->ListEntry);

            PrevHeader->BlockSize += CurrentSize;
            Header = PrevHeader;
            CurrentSize = Header->BlockSize;
        }
    }

    // 4. Coalesce Tetangga Kanan
    PPOOL_HEADER NextHeader = POOL_NEXT_BLOCK(Header);
    
    if (PAGE_ALIGN(NextHeader) != NextHeader) { // Jangan nabrak batas Page
        if (NextHeader->PoolType == 0) { // Kosong
            PPOOL_FREE_BLOCK NextFree = (PPOOL_FREE_BLOCK)NextHeader;
            
            // Safe Unlinking!
            UlpRemoveEntry(&NextFree->ListEntry);

            Header->BlockSize += NextHeader->BlockSize;
            CurrentSize = Header->BlockSize;
        }
    }

    // 5. Update PreviousSize untuk tetangga di kanan blok gabungan ini
    PPOOL_HEADER NewNextHeader = POOL_NEXT_BLOCK(Header);
    if (PAGE_ALIGN(NewNextHeader) != NewNextHeader) {
        NewNextHeader->PreviousSize = CurrentSize;
    }

    // 6. Rapikan dan kembalikan ke List yang aman
    Header->PoolType = 0;
    Header->PoolTag = 'eerF'; // Tag khusus memori bebas

    if (CurrentSize == (POOL_MAX_BLOCKS - 1)) {
        // HALAMAN INI 100% KOSONG!
        // Jangan dimasukin ke list lagi, kembalikan ke VMM biar RAM OS lega.
        MmFreePoolPage((PVOID)Header);
        
        // Update statistik VMM
        Descriptor->TotalPages--;
        Descriptor->TotalBytesMapped -= POOL_PAGE_SIZE;
        
    } else {
        // Belum sepenuhnya kosong, masih ada tetangga yang dipakai.
        // Masukkan kembali ke List yang sesuai ukurannya.
        PPOOL_FREE_BLOCK FreeBlock = (PPOOL_FREE_BLOCK)Header;
        UlpInsertHead(&Descriptor->ListHeads[CurrentSize], &FreeBlock->ListEntry);
    }

    // 7. Update statistik
    Descriptor->RunningDeAllocs++;
    Descriptor->TotalBytesAllocated -= (CurrentSize << POOL_BLOCK_SHIFT);
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