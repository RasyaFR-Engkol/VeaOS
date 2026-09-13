#include <veakrnl.h>

PMM_VAD MmVadRoot = NULL;
static RTL_AVL_TREE MmKernelVadTree;
static PMM_VAD MmBootstrapPool = NULL;
static ULONG MmBootstrapIndex = 0;
static PMM_VAD MmVadFreeList = NULL;

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
    return TRUE;
}

static
PMM_VAD
VEAPI
MmpAllocateVadNode(VOID)
{
    if (MmVadFreeList) {
        PMM_VAD ReusedNode = MmVadFreeList;
        // Akali pakai pointer Left untuk nyimpen next alamat free list sementara
        MmVadFreeList = (PMM_VAD)ReusedNode->CoreNode.Left; 
        return ReusedNode;
    }

    if (!MmBootstrapPool) {
        ULONG PhysPage = MmAllocatePhysicalPage();
        MmBootstrapPool = (PMM_VAD)(PhysPage + 0xC0000000); 
        RtlZeroMemory(MmBootstrapPool, 4096);
    }

    if (MmBootstrapIndex < (4096 / sizeof(MM_VAD))) {
        return &MmBootstrapPool[MmBootstrapIndex++];
    }

    kdp_print("VMM FATAL: Bootstrap VAD pool exhausted!\n\r");
    return NULL;
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
    ULONG StartingKva = 0xC1000000; 
    ULONG LastVpnChecked = StartingKva >> 12;
    ULONG AllocatedVa = 0;

    if (!MmKernelVadTree.Root) {
        // Jika pohon kosong, langsung ambil di batas bawah KVA
        AllocatedVa = StartingKva;
    } else {
        // Cari celah di antara node
        AllocatedVa = MmpFindFreeVirtualGap(PageCount, &LastVpnChecked);
        
        // Jika tidak ada celah di tengah, taruh di ujung paling kanan
        if (AllocatedVa == 0) {
            AllocatedVa = LastVpnChecked << 12;
        }
    }

    // Cegah tabrakan dengan Recursive Mapping di 0xFFC00000
    if (AllocatedVa + (PageCount * 4096) >= 0xFFC00000) {
        kdp_print("VMM FATAL: Kernel Virtual Space Exhausted!\n\r");
        return 0;
    }

    // Buat VAD baru
    PMM_VAD NewVad = MmpAllocateVadNode();
    if (!NewVad) return 0;

    NewVad->StartingVpn = AllocatedVa >> 12;
    NewVad->EndingVpn = NewVad->StartingVpn + PageCount - 1;

    // Setor Node ke mesin AVL RTL
    RtlInsertElementAvl(&MmKernelVadTree, (PRTL_BALANCED_NODE)NewVad);
    
    // Sinkronkan global pointer (opsional, buat mempermudah akses eksternal)
    MmVadRoot = (PMM_VAD)MmKernelVadTree.Root;

    return AllocatedVa;
}

VOID 
VEAPI 
MmFreeVirtualRange(ULONG VirtualAddress)
{
    ULONG VpnTarget = VirtualAddress >> 12;

    // 1. Cari apakah blok virtual ini benar-benar terdaftar di AVL Tree
    PMM_VAD VadToFree = MmpFindVadByVpn(MmKernelVadTree.Root, VpnTarget);
    if (!VadToFree) {
        kdp_print("VMM WARNING: Tried to free unallocated virtual address!\n\r");
        return;
    }

    // Pastikan user ngasih alamat base yang pas (nggak di tengah-tengah blok)
    if (VpnTarget != VadToFree->StartingVpn) {
        kdp_print("VMM WARNING: Tried to free from middle of a VAD!\n\r");
        return;
    }

    ULONG PageCount = (VadToFree->EndingVpn - VadToFree->StartingVpn) + 1;
    ULONG CurrentVa = VadToFree->StartingVpn << 12;

    // 2. Copot PTE dan bebaskan Physical Pages-nya ke MmPipDatabase
    for (ULONG i = 0; i < PageCount; i++, CurrentVa += 4096) {
        PAGE_ENTRY *pte = MM_GET_PTE(CurrentVa);
        
        if (pte->bits.present) {
            ULONG PhysAddress = pte->bits.frame << 12;
            
            // Kembalikan tanah fisik ke list PMM
            MmFreePhysicalPage(PhysAddress);
            
            // Hapus pemetaan di Page Table Entry
            pte->bits.present = 0;
            pte->bits.frame = 0;
            
            // Beri tahu CPU bahwa cache alamat ini udah basi
            MMI_INVALIDATE_TLB(CurrentVa);
        }
    }

    // 3. Hapus struktur pencatatan (Node VAD) dari AVL Tree
    RtlDeleteElementAvl(&MmKernelVadTree, (PRTL_BALANCED_NODE)VadToFree);
    
    // 4. Masukkan VAD yang sudah dicabut ke tong sampah daur ulang (Free List)
    RtlZeroMemory(VadToFree, sizeof(MM_VAD));
    VadToFree->CoreNode.Left = (PRTL_BALANCED_NODE)MmVadFreeList;
    MmVadFreeList = VadToFree;

    // Sinkronkan global root pointer
    MmVadRoot = (PMM_VAD)MmKernelVadTree.Root;
}
