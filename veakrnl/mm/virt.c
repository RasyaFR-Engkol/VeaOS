#include <veakrnl.h>

PMM_VAD MmVadRoot = NULL;
static RTL_AVL_TREE MmKernelVadTree;
static PMM_VAD MmBootstrapPool = NULL;
static ULONG MmBootstrapIndex = 0;
static PMM_VAD MmVadFreeList = NULL;
static KSPIN_LOCK MmVmmLock;

static 
LONG 
VEAPI
MmpCompareVad(PRTL_BALANCED_NODE NodeA, PRTL_BALANCED_NODE NodeB) 
{
    PMM_VAD VadA = (PMM_VAD)NodeA;
    PMM_VAD VadB = (PMM_VAD)NodeB;

    if (VadA->StartingVpn < VadB->StartingVpn) return -1;
    if (VadA->StartingVpn > VadB->StartingVpn) return 1;
    return 0;
}

BOOLEAN 
VEAPI 
MmInitializeVmm(VOID)
{
    MmKernelVadTree.Root = NULL;
    MmKernelVadTree.CompareRoutine = MmpCompareVad;
    MmVadRoot = NULL;
    KsInitializeSpinlock(&MmVmmLock);
    return TRUE;
}

static PMM_VAD VEAPI MmpAllocateVadNode(VOID)
{
    KIRQL OldIrql;

    // 1. Ambil dari Lookaside / Free List
    KsAcquireSpinLock(&MmVmmLock, &OldIrql);
    if (MmVadFreeList) {
        PMM_VAD ReusedNode = MmVadFreeList;
        MmVadFreeList = (PMM_VAD)ReusedNode->CoreNode.Left;
        KsReleaseSpinLock(&MmVmmLock, OldIrql);
        RtlZeroMemory(ReusedNode, sizeof(MM_VAD));
        return ReusedNode;
    }
    KsReleaseSpinLock(&MmVmmLock, OldIrql);

    // 2. Ambil dari Bootstrap Pool jika masih ada slot
    if (!MmBootstrapPool) {
        ULONG PhysPage = MmAllocateContiguousPhysicalPages(16);
        if (PhysPage == 0) {
            DPRINT("VMM FATAL: Failed to allocate physical pages for Bootstrap Pool!\n\r");
            return NULL;
        }

        ULONG BootstrapVa = PhysPage + 0xC0000000;

        for (ULONG i = 0; i < 16; i++) {
            ULONG CurrentVa = BootstrapVa + (i * PAGE_SIZE);
            ULONG CurrentPhys = PhysPage + (i * PAGE_SIZE);

            // Cek apakah Page Directory Entry (PDE) sudah ada
            PAGE_ENTRY *pde = MM_GET_PDE(CurrentVa);
            if (!pde->bits.present) {
                // Alokasikan Page Table fisik baru jika belum ada PDE
                ULONG NewPtPhys = MmAllocatePhysicalPage();
                if (NewPtPhys == 0) {
                    kdp_print("VMM FATAL: Out of memory for Bootstrap Page Table!\n\r");
                    return NULL;
                }

                MMI_WRITE_PDE_VALID(pde, NewPtPhys >> 12);

                // Zeroing Page Table baru tersebut
                PVOID PtVirtualAddr = (PVOID)((ULONG)MM_GET_PTE(CurrentVa) & 0xFFFFF000);
                MMI_INVALIDATE_TLB((ULONG)PtVirtualAddr);
                RtlZeroMemory(PtVirtualAddr, PAGE_SIZE);
            }

            // Map PTE ke Alamat Fisik Bootstrap Pool
            PAGE_ENTRY *pte = MM_GET_PTE(CurrentVa);
            if (!pte->bits.present) {
                MMI_WRITE_PTE_VALID(pte, CurrentPhys >> 12);
                MMI_INVALIDATE_TLB(CurrentVa);
            }
        }

        MmBootstrapPool = (PMM_VAD)BootstrapVa;
        RtlZeroMemory(MmBootstrapPool, 16 * PAGE_SIZE);
    }

    if (MmBootstrapIndex < ((16 * 4096) / sizeof(MM_VAD))) {
        return &MmBootstrapPool[MmBootstrapIndex++];
    }

    // 3. EXPAND VAD POOL SECARA MANDIRI (Tanpa panggil UlAllocatePoolWithTag!)
    // Alokasikan 1 Page Fisik baru khusus VAD, potong-potong jadi Node, lalu masukkan ke Free List
    ULONG NewPagePhys = MmAllocatePhysicalPage();
    if (NewPagePhys == 0) {
        kdp_print("VMM FATAL: Out of physical memory for VAD nodes!\n\r");
        return NULL;
    }

    PMM_VAD NewPageVa = (PMM_VAD)(NewPagePhys + 0xC0000000); // Identity/Direct Mapping
    RtlZeroMemory(NewPageVa, 4096);

    ULONG NodesInPage = 4096 / sizeof(MM_VAD);

    KsAcquireSpinLock(&MmVmmLock, &OldIrql);
    // Masukkan node ke-2 sampai terakhir ke MmVadFreeList
    for (ULONG i = 1; i < NodesInPage; i++) {
        NewPageVa[i].CoreNode.Left = (PRTL_BALANCED_NODE)MmVadFreeList;
        MmVadFreeList = &NewPageVa[i];
    }
    KsReleaseSpinLock(&MmVmmLock, OldIrql);

    // Kembalikan node ke-0 untuk pemanggil saat ini
    return &NewPageVa[0];
}

static VOID VEAPI MmpFreeVadNode(PMM_VAD VadNode)
{
    KIRQL OldIrql;
    KsAcquireSpinLock(&MmVmmLock, &OldIrql);
    
    RtlZeroMemory(VadNode, sizeof(MM_VAD));
    VadNode->CoreNode.Left = (PRTL_BALANCED_NODE)MmVadFreeList;
    MmVadFreeList = VadNode;
    
    KsReleaseSpinLock(&MmVmmLock, OldIrql);
}

static 
PMM_VAD 
VEAPI
MmpFindVadByVpn(PRTL_BALANCED_NODE Node, ULONG Vpn)
{
    while (Node != NULL) {
        PMM_VAD Vad = (PMM_VAD)Node;
        
        // Cek apakah VPN berada di dalam rentang VAD ini
        if (Vpn >= Vad->StartingVpn && Vpn <= Vad->EndingVpn) {
            return Vad; 
        }
        
        // Telusuri ke kiri atau ke kanan
        if (Vpn < Vad->StartingVpn) {
            Node = Node->Left;
        } else {
            Node = Node->Right;
        }
    }
    
    return NULL;
}

static 
ULONG 
VEAPI
MmpFindFreeVirtualGap(ULONG PageCount, ULONG* LastVpnChecked)
{
    // Mulai dari node dengan alamat virtual terendah (Cached Leftmost)
    PRTL_BALANCED_NODE CurrentNode = MmKernelVadTree.Leftmost;
    
    while (CurrentNode != NULL) {
        PMM_VAD Vad = (PMM_VAD)CurrentNode;
        
        // Hitung celah antara batas terakhir dan awal VAD saat ini
        ULONG GapPages = Vad->StartingVpn - *LastVpnChecked;
        
        if (GapPages >= PageCount) {
            // Celah ditemukan!
            return (*LastVpnChecked) << 12; 
        }
        
        // Geser batas pengecekan ke akhir VAD ini
        *LastVpnChecked = Vad->EndingVpn + 1;
        
        // Maju ke node VAD berikutnya secara berurutan (In-Order)
        CurrentNode = RtlGetNextNodeAvl(CurrentNode);
    }
    
    // Jika loop selesai dan tidak ketemu celah, berarti taruh di ujung kanan
    return 0;
}

ULONG 
VEAPI 
MmAllocateVirtualRange(ULONG PageCount)
{
    PMM_VAD NewVad = MmpAllocateVadNode();
    if (!NewVad) {
        return 0;
    }

    KIRQL OldIrql;
    ULONG StartingKva = 0xC1000000; 
    ULONG LastVpnChecked = StartingKva >> 12;
    ULONG AllocatedVa = 0;

    if (PageCount == 0 || PageCount > (0xFFC00000 - StartingKva) / PAGE_SIZE) {
        // Jangan lupa free node kalau Bailing Out
        MmpFreeVadNode(NewVad); 
        return 0;
    }

    KsAcquireSpinLock(&MmVmmLock, &OldIrql);

    if (!MmKernelVadTree.Root) {
        AllocatedVa = StartingKva;
    } else {
        AllocatedVa = MmpFindFreeVirtualGap(PageCount, &LastVpnChecked);
        if (AllocatedVa == 0) AllocatedVa = LastVpnChecked << 12;
    }

    if (AllocatedVa + (PageCount * 4096) >= 0xFFC00000) {
        kdp_print("VMM FATAL: Kernel Virtual Space Exhausted!\n\r");
        KsReleaseSpinLock(&MmVmmLock, OldIrql);
        MmpFreeVadNode(NewVad); // Free di luar lock
        return 0;
    }

    NewVad->StartingVpn = AllocatedVa >> 12;
    NewVad->EndingVpn = NewVad->StartingVpn + PageCount - 1;

    RtlInsertElementAvl(&MmKernelVadTree, (PRTL_BALANCED_NODE)NewVad);
    MmVadRoot = (PMM_VAD)MmKernelVadTree.Root;

    KsReleaseSpinLock(&MmVmmLock, OldIrql);
    return AllocatedVa;
}

VOID 
VEAPI 
MmFreeVirtualRange(ULONG VirtualAddress)
{
    KIRQL OldIrql;
    ULONG VpnTarget = VirtualAddress >> 12;

    KsAcquireSpinLock(&MmVmmLock, &OldIrql);
    PMM_VAD VadToFree = MmpFindVadByVpn(MmKernelVadTree.Root, VpnTarget);
    if (!VadToFree) {
        kdp_print("VMM WARNING: Tried to free unallocated virtual address!\n\r");
        KsReleaseSpinLock(&MmVmmLock, OldIrql);
        return;
    }

    if (VpnTarget != VadToFree->StartingVpn) {
        kdp_print("VMM WARNING: Tried to free from middle of a VAD!\n\r");
        KsReleaseSpinLock(&MmVmmLock, OldIrql);
        return;
    }

    ULONG PageCount = (VadToFree->EndingVpn - VadToFree->StartingVpn) + 1;
    ULONG CurrentVa = VadToFree->StartingVpn << 12;

    for (ULONG i = 0; i < PageCount; i++, CurrentVa += 4096) {
        if (!MM_GET_PDE(CurrentVa)->bits.present) {
            continue;
        }

        PAGE_ENTRY *pte = MM_GET_PTE(CurrentVa);
        
        if (pte->bits.present) {
            ULONG PhysAddress = pte->bits.frame << 12;

            // Hapus pemetaan di Page Table Entry
            pte->bits.present = 0;
            pte->bits.frame = 0;

            // Beri tahu CPU bahwa cache alamat ini udah basi
            MMI_INVALIDATE_TLB(CurrentVa);

            // Recycle the frame only after the old mapping has been invalidated.
            MmFreePhysicalPage(PhysAddress);
        }
    }

    RtlDeleteElementAvl(&MmKernelVadTree, (PRTL_BALANCED_NODE)VadToFree);

    MmVadRoot = (PMM_VAD)MmKernelVadTree.Root;
    KsReleaseSpinLock(&MmVmmLock, OldIrql);

    MmpFreeVadNode(VadToFree);
}

ULONG
VEAPI
MmGetVirtualRangePageCount(ULONG VirtualAddress)
{
    KIRQL OldIrql;
    ULONG PageCount = 0;

    if (VirtualAddress & (PAGE_SIZE - 1))
    {
        return 0;
    }

    KsAcquireSpinLock(&MmVmmLock, &OldIrql);

    PMM_VAD Vad = MmpFindVadByVpn(
        MmKernelVadTree.Root,
        VirtualAddress >> 12
    );
    if (Vad && Vad->StartingVpn == (VirtualAddress >> 12))
    {
        PageCount = Vad->EndingVpn - Vad->StartingVpn + 1;
    }

    KsReleaseSpinLock(&MmVmmLock, OldIrql);
    return PageCount;
}
