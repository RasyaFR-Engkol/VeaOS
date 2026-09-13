#include <veakrnl.h>

PVOID
VEAPI
MmMapIoSpace(
    ULONG PhysicalAddress,
    ULONG NumberOfBytes,
    MEMORY_CACHING_TYPE CacheType
)
{
    // 1. Hitung berapa page yang dibutuhkan (Page Aligned)
    ULONG Offset = PhysicalAddress & 4095;
    ULONG AlignedPhys = PhysicalAddress & ~4095;
    ULONG PageCount = (NumberOfBytes + Offset + 4095) / 4096;

    // 2. Minta rentang Virtual Address kosong dari AVL Tree kernel lu
    ULONG BaseVa = MmAllocateVirtualRange(PageCount);
    if (BaseVa == 0) {
        kdp_print("MM: MmMapIoSpace failed to allocate virtual range!\n\r");
        return NULL;
    }

    ULONG CurrentVa = BaseVa;
    ULONG CurrentPhys = AlignedPhys;

    // 3. Lakukan mapping manual ke Page Table
    for (ULONG i = 0; i < PageCount; i++) {
        PAGE_ENTRY *pde = MM_GET_PDE(CurrentVa);
        
        // JIKA PAGE TABLE BELUM ADA: Kita harus bikin PT baru (Sama kayak di MmAllocatePoolPages)
        if (!pde->bits.present) {
            ULONG NewPtPhys = MmAllocatePhysicalPage(); // Ambil 1 page fisik murni untuk Page Table
            if (NewPtPhys == 0) {
                kdp_print("MM: MmMapIoSpace OOM creating Page Table!\n\r");
                // TODO: Rollback virtual range if necessary
                return NULL;
            }
            
            MMI_WRITE_PDE_VALID(pde, NewPtPhys >> 12);

            PVOID PtVirtualAddr = (PVOID)((ULONG)MM_GET_PTE(CurrentVa) & 0xFFFFF000);
            MMI_INVALIDATE_TLB((ULONG)PtVirtualAddr);
            RtlZeroMemory(PtVirtualAddr, 4096);
        }

        // Tembak langsung alamat fisik MMIO ke PTE
        PAGE_ENTRY *pte = MM_GET_PTE(CurrentVa);
        
        // Setup bit PTE: Present, Writable. 
        // Untuk MMIO (LAPIC), tambahkan bit PCD (Page Cache Disable) jika struktur PAGE_ENTRY lu mendukung.
        MMI_WRITE_PTE_VALID(pte, CurrentPhys >> 12);
        
        // Refresh TLB CPU
        MMI_INVALIDATE_TLB(CurrentVa);

        CurrentVa += 4096;
        CurrentPhys += 4096;
    }

    // Kembalikan alamat virtual yang sudah ditambah offset aslinya
    return (PVOID)(BaseVa + Offset);
}

VOID
VEAPI
MmUnmapIoSpace(
    PVOID BaseAddress,
    ULONG NumberOfBytes
)
{
    if (BaseAddress == NULL || NumberOfBytes == 0) return;

    ULONG BaseVa = (ULONG)BaseAddress & ~4095;
    ULONG Offset = (ULONG)BaseAddress & 4095;
    ULONG PageCount = (NumberOfBytes + Offset + 4095) / 4096;

    ULONG CurrentVa = BaseVa;

    // 1. Bersihkan PTE tanpa menyentuh PMM/PipDatabase
    for (ULONG i = 0; i < PageCount; i++) {
        PAGE_ENTRY *pte = MM_GET_PTE(CurrentVa);
        
        if (pte->bits.present) {
            MI_WRITE_INVALID_PTE(pte); // Hapus mapping murni
            MMI_INVALIDATE_TLB(CurrentVa);
        }
        CurrentVa += 4096;
    }

    // 2. Cari node VAD di AVL Tree untuk dicabut kodenya
    // Karena kita tidak mau memicu pembebasan page fisik ganda, kita langsung hapus nodenya saja.
    // Triknya: Panggil MmFreeVirtualRange setelah PTE-nya sudah di-wipe (karena bits.present sudah 0, dia tidak akan memanggil MmFreePhysicalPage)
    MmFreeVirtualRange(BaseVa);
}

PVOID
VEAPI
MiGetSystemPageDirectoryTableBase(VOID)
{
    ULONG Cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(Cr3));

    return (PVOID)Cr3;
}   