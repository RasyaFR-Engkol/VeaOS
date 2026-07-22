#include <veakrnl.h>

PVOID
VEAPI
MmAllocatePoolPages(POOL_TYPE PoolType, ULONG NumberOfPages)
{
    // 1. Minta rentang Virtual Address ke AVL/VAD 
    ULONG VirtualAddress = MmAllocateVirtualRange(NumberOfPages); 
    if (VirtualAddress == 0) {
        kdp_print("MM FATAL: Failed to get Virtual Range for Pool!\n\r");
        return NULL; 
    }

    ULONG CurrentVa = VirtualAddress;

    // 2. Map Virtual Address ke Physical Pages satu per satu
    for (ULONG i = 0; i < NumberOfPages; i++) {
        
        // Minta memori fisik dari PIP Database
        ULONG PhysPage = MmAllocatePhysicalPage(); 
        if (PhysPage == 0) {
            // TODO: Bikin logic rollback untuk ngebebasin VAD dan phys page 
            // yang udah terlanjur dialokasikan kalau OOM di tengah jalan.
            kdp_print("MM FATAL: Out of Physical Memory during Pool allocation!\n\r");
            return NULL;
        }

        // Ambil Page Directory Entry (PDE)
        PAGE_ENTRY *pde = MM_GET_PDE(CurrentVa);
        
        // JIKA PAGE TABLE BELUM ADA: Bikin Page Table baru (Sama kayak di MmMapPip)
        if (!pde->bits.present) {
            ULONG NewPtPhys = MmAllocatePhysicalPage(); // Pinjam 1 page untuk PT
            if (NewPtPhys == 0) {
                return NULL; // OOM saat bikin Page Table
            }
            
            MMI_WRITE_PDE_VALID(pde, NewPtPhys >> 12);

            PVOID PtVirtualAddr = (PVOID)((ULONG)MM_GET_PTE(CurrentVa) & 0xFFFFF000);
            MMI_INVALIDATE_TLB((ULONG)PtVirtualAddr);
            RtlZeroMemory(PtVirtualAddr, 4096);
        }

        // Ambil Page Table Entry (PTE) dan validasi mapping-nya
        PAGE_ENTRY *pte = MM_GET_PTE(CurrentVa);
        MMI_WRITE_PTE_VALID(pte, PhysPage >> 12);
        
        // Beri tahu CPU ada mapping baru di TLB
        MMI_INVALIDATE_TLB(CurrentVa);

        CurrentVa += 4096;
    }

    return (PVOID)VirtualAddress;
}

PVOID
VEAPI
MmAllocatePoolPage(POOL_TYPE PoolType)
{
    // Cukup panggil versi jamaknya dengan jumlah 1 Page
    return MmAllocatePoolPages(PoolType, 1);
}

VOID
VEAPI
MmFreePoolPages(PVOID BaseAddress, ULONG NumberOfPages)
{
    if (BaseAddress == NULL || NumberOfPages == 0) return;

    ULONG CurrentVa = (ULONG)BaseAddress;

    // 1. Unmap Physical Pages satu per satu
    for (ULONG i = 0; i < NumberOfPages; i++) {
        PAGE_ENTRY *pte = MM_GET_PTE(CurrentVa);
        
        if (pte->bits.present) {
            // Dapatkan alamat fisik murni (hilangkan 12 bit flag di bawah)
            ULONG PhysPage = (*(ULONG*)pte) & 0xFFFFF000; 
            
            // Kembalikan ke PIP Database (List halaman kosong)
            MmFreePhysicalPage(PhysPage); 
            
            // Hapus isi PTE sepenuhnya biar nggak ada sisa akses
            MI_WRITE_INVALID_PTE(pte);
            
            // Wajib! Kasih tau CPU kalau mapping ini udah musnah
            MMI_INVALIDATE_TLB(CurrentVa);
        }
        CurrentVa += 4096;
    }

    // 2. Kembalikan rentang Virtual Address ke VAD/AVL lu
    MmFreeVirtualRange((ULONG)BaseAddress);
}

VOID
VEAPI
MmFreePoolPage(PVOID BaseAddress)
{
    MmFreePoolPages(BaseAddress, 1);
}