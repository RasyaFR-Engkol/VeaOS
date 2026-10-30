#include <veakrnl.h>

PHYSICAL_ADDRESS
VEAPI
MmGetPhysicalAddress(
    IN PVOID BaseAddress
)
{
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG VirtualAddress;
    PAGE_ENTRY *Pde;
    PAGE_ENTRY *Pte;

    PhysicalAddress.QuadPart = 0;
    VirtualAddress = (ULONG)BaseAddress;
    Pde = MM_GET_PDE(VirtualAddress);

    if (!Pde->bits.present)
    {
        return PhysicalAddress;
    }

    if (Pde->bits.ps_or_pat)
    {
        PhysicalAddress.QuadPart =
            ((ULONGLONG)(*(ULONG *)Pde & 0xFFC00000)) |
            (VirtualAddress & 0x003FFFFF);
        return PhysicalAddress;
    }

    Pte = MM_GET_PTE(VirtualAddress);
    if (!Pte->bits.present)
    {
        return PhysicalAddress;
    }

    PhysicalAddress.QuadPart =
        ((ULONGLONG)(*(ULONG *)Pte & 0xFFFFF000)) |
        (VirtualAddress & 0x00000FFF);
    return PhysicalAddress;
}

PVOID
VEAPI
MmMapIoSpace(
    ULONG PhysicalAddress,
    ULONG NumberOfBytes,
    MEMORY_CACHING_TYPE CacheType
)
{
    ULONG Offset = PhysicalAddress & 4095;
    ULONG AlignedPhys = PhysicalAddress & ~4095;
    ULONG PageCount = (NumberOfBytes + Offset + 4095) / 4096;

    ULONG BaseVa = MmAllocateVirtualRange(PageCount);
    if (BaseVa == 0) {
        kdp_print("MM: MmMapIoSpace failed to allocate virtual range!\n\r");
        return NULL;
    }

    ULONG CurrentVa = BaseVa;
    ULONG CurrentPhys = AlignedPhys;

    for (ULONG i = 0; i < PageCount; i++) {
        
        PAGE_ENTRY *pde = MM_GET_PDE(CurrentVa);
        
        if (!pde->bits.present) {
            ULONG NewPtPhys = MmAllocatePhysicalPage(); 
            if (NewPtPhys == 0) {
                kdp_print("MM: MmMapIoSpace OOM creating Page Table!\n\r");
                MmUnmapIoSpace((PVOID)BaseVa, i * 4096);
                return NULL;
            }
            
            MMI_WRITE_PDE_VALID(pde, NewPtPhys >> 12);

            PVOID PtVirtualAddr = (PVOID)((ULONG)MM_GET_PTE(CurrentVa) & 0xFFFFF000);
            MMI_INVALIDATE_TLB((ULONG)PtVirtualAddr);
            RtlZeroMemory(PtVirtualAddr, 4096);
        }

        PAGE_ENTRY *pte = MM_GET_PTE(CurrentVa);
        if (pte->bits.present)
        {
            KsBugCheckEx(
                MEMORY_MANAGEMENT_ERROR,
                MM_BUGCHECK_PTE_CORRUPTED,
                CurrentVa,
                pte->raw,
                CurrentPhys
            );
        }

        MMI_WRITE_PTE_VALID(pte, CurrentPhys >> 12);

        switch (CacheType)
        {
            case MmNonCached:
            case MmNonCachedUnordered:
                pte->bits.cache_disable = 1;
                pte->bits.write_through = 1;
                break;

            case MmWriteCombined:
                // TODO: PAT need to be programmed. Fallback to unchached.
                pte->bits.cache_disable = 1;
                pte->bits.write_through = 1;
                break;

            default:
                break;
        }
        
        // Refresh TLB CPU
        MMI_INVALIDATE_TLB(CurrentVa);

        CurrentVa += 4096;
        CurrentPhys += 4096;
    }

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

VOID
VEAPI
MmProbeAndLockPages(
    PMDL Mdl,
    KPROCESSOR_MODE AccessMode,
    LOCK_OPERATION Operation
)
{
    ULONG PageCount;
    ULONG CurrentVa;
    ULONG i;
    PAGE_ENTRY *Pte;

    /* Can't lock MDL again */
    if (Mdl->MdlFlags & MDL_PAGES_LOCKED)
    {
        KsBugCheckEx(MEMORY_MANAGEMENT_ERROR, 0x1111, (ULONG_PTR)Mdl, 0, 0);
    }

    PageCount = (Mdl->ByteCount + Mdl->ByteOffset + 4095) / 4096;
    CurrentVa = (ULONG)Mdl->StartVa;

    for (i = 0; i < PageCount; i++)
    {
        volatile UCHAR *TouchVa = (volatile UCHAR *)CurrentVa;

RetryFault:
        /* Cek apakah Page Directory atau Page Table belum ada */
        if (!MM_GET_PDE(CurrentVa)->bits.present || 
            !MM_GET_PTE(CurrentVa)->bits.present)
        {
            if (Operation != IoReadAccess) {
                // Paksa Write Fault (Membaca lalu menulis ulang hal yang sama)
                *TouchVa = *TouchVa;
            } else {
                // Paksa Read Fault (Cukup baca 1 byte)
                (void)*TouchVa;
            }
            
            // Setelah hardware #PF tertangani oleh ISR kamu, cek PTE lagi!
            goto RetryFault;
        }

        Pte = MM_GET_PTE(CurrentVa);

        /* Cek apakah butuh Write tapi page masih Read-Only (CoW scenario) */
        if (Operation != IoReadAccess && !Pte->bits.rw) {
            
            // Paksa Write Fault untuk memicu Copy-On-Write
            *TouchVa = *TouchVa;
            
            goto RetryFault;
        }

        /* Kalau sudah lolos semua, berarti halaman fisik SIAP dan VALID */
        ULONG PhysAddress = Pte->bits.frame << 12;

        MmReferencePhysicalPage(PhysAddress);

        Mdl->PfnArray[i] = Pte->bits.frame;

        CurrentVa += PAGE_SIZE;
    }

    Mdl->MdlFlags |= MDL_PAGES_LOCKED;
}

VOID
VEAPI
MmUnlockPages(
    IN PMDL Mdl
)
{
    ULONG PageCount;
    ULONG i;

    if ((Mdl->MdlFlags & MDL_PAGES_LOCKED) == 0) {
        DPRINT("MM FATAL: Tried to unlock an MDL that is not locked!\n\r");
        KsBugCheckEx(MEMORY_MANAGEMENT_ERROR, 0x4444, (ULONG_PTR)Mdl, 0, 0);
        return;
    }

    if (Mdl->MdlFlags & MDL_MAPPED_TO_SYSTEM_VA)
    {
        KsBugCheckEx(MEMORY_MANAGEMENT_ERROR, 0x5555, (ULONG_PTR)Mdl, 0, 0);
    }

    PageCount = (Mdl->ByteCount + Mdl->ByteOffset + (PAGE_SIZE - 1)) / PAGE_SIZE;

    for (i = 0; i < PageCount; i++) {
        ULONG PhysAddress = Mdl->PfnArray[i] << 12;

        MmDereferencePhysicalPage(PhysAddress);
    }

    Mdl->MdlFlags &= ~MDL_PAGES_LOCKED;
}
