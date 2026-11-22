#include <veakrnl.h>

KSPIN_LOCK MmpTempMapLock;

PHYSICAL_ADDRESS
VEAPI
MmGetPhysicalAddress(
    IN PVOID BaseAddress
)
{
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG VirtualAddress = (ULONG)BaseAddress;
    PMMPDE Pde;
    PMMPTE Pte;
 
    PhysicalAddress.QuadPart = 0;
 
    Pde = MiAddressToPde(VirtualAddress);
    if (!Pde->Hard.Valid)
    {
        return PhysicalAddress;
    }
 
    /* Halaman 4MB: frame ada di PDE */
    if (Pde->Hard.LargePage)
    {
        PhysicalAddress.QuadPart =
            ((ULONGLONG)(Pde->Long & 0xFFC00000)) |
            (VirtualAddress & 0x003FFFFF);
        return PhysicalAddress;
    }
 
    Pte = MiAddressToPte(VirtualAddress);
    if (!Pte->Hard.Valid)
    {
        return PhysicalAddress;
    }
 
    PhysicalAddress.QuadPart =
        ((ULONGLONG)Pte->Hard.PageFrameNumber << PAGE_SHIFT) |
        (VirtualAddress & (PAGE_SIZE - 1));
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
    ULONG Offset = PhysicalAddress & (PAGE_SIZE - 1);
    ULONG AlignedPhys = PhysicalAddress & ~(PAGE_SIZE - 1);
    ULONG PageCount;
    ULONG BaseVa;
    ULONG CurrentVa;
    ULONG CurrentPhys;
 
    if (NumberOfBytes == 0) return NULL;
 
    PageCount = (NumberOfBytes + Offset + (PAGE_SIZE - 1)) / PAGE_SIZE;
 
    BaseVa = MmAllocateVirtualRange(PageCount);
    if (BaseVa == 0) {
        kdp_print("MM: MmMapIoSpace failed to allocate virtual range!\n\r");
        return NULL;
    }
 
    CurrentVa = BaseVa;
    CurrentPhys = AlignedPhys;
 
    for (ULONG i = 0; i < PageCount; i++) {
        PMMPDE Pde = MiAddressToPde(CurrentVa);
 
        if (!Pde->Hard.Valid) {
            ULONG NewPtPhys = MmAllocatePhysicalPage();
            if (NewPtPhys == 0) {
                kdp_print("MM: MmMapIoSpace OOM creating Page Table!\n\r");
 
                /*
                 * i == 0: belum ada PTE yang dipasang, cukup lepas VAD-nya.
                 * MmUnmapIoSpace dengan 0 byte langsung return dan akan
                 * membocorkan VAD.
                 * i > 0: PTE yang sudah dipasang harus dihapus lewat
                 * MmUnmapIoSpace (bukan MmFreeVirtualRange langsung), supaya
                 * frame IO tidak dimasukkan ke free list fisik.
                 */
                if (i != 0) {
                    MmUnmapIoSpace((PVOID)BaseVa, i * PAGE_SIZE);
                } else {
                    MmFreeVirtualRange(BaseVa);
                }
                return NULL;
            }
 
            MI_WRITE_PDE_VALID(Pde, NewPtPhys >> 12);   /* VA kernel: user=0 */
 
            PVOID PtVirtualAddr = (PVOID)((ULONG)MiAddressToPte(CurrentVa) & 0xFFFFF000);
            MMI_INVALIDATE_TLB(PtVirtualAddr);
            RtlZeroMemory(PtVirtualAddr, PAGE_SIZE);
        }
 
        PMMPTE Pte = MiAddressToPte(CurrentVa);
        if (Pte->Hard.Valid)
        {
            KsBugCheckEx(
                MEMORY_MANAGEMENT_ERROR,
                MM_BUGCHECK_PTE_CORRUPTED,
                CurrentVa,
                Pte->Long,
                CurrentPhys
            );
        }
 
        /* Bangun PTE lengkap dengan atribut cache, lalu tulis sekali */
        MMPTE TempPte;
        MI_MAKE_VALID_PTE(TempPte, CurrentPhys >> 12, 0, 1);
 
        switch (CacheType)
        {
            case MmNonCached:
            case MmNonCachedUnordered:
                TempPte.Hard.CacheDisable = 1;
                TempPte.Hard.WriteThrough = 1;
                break;
 
            case MmWriteCombined:
                // TODO: PAT need to be programmed. Fallback to uncached.
                TempPte.Hard.CacheDisable = 1;
                TempPte.Hard.WriteThrough = 1;
                break;
 
            default:
                break;
        }
 
        MI_WRITE_VALID_PTE(Pte, TempPte);
 
        // Refresh TLB CPU
        MMI_INVALIDATE_TLB(CurrentVa);
 
        CurrentVa += PAGE_SIZE;
        CurrentPhys += PAGE_SIZE;
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
 
    ULONG BaseVa = (ULONG)BaseAddress & ~(PAGE_SIZE - 1);
    ULONG Offset = (ULONG)BaseAddress & (PAGE_SIZE - 1);
    ULONG PageCount = (NumberOfBytes + Offset + (PAGE_SIZE - 1)) / PAGE_SIZE;
 
    ULONG CurrentVa = BaseVa;
 
    // 1. Bersihkan PTE tanpa menyentuh PMM/PipDatabase
    for (ULONG i = 0; i < PageCount; i++, CurrentVa += PAGE_SIZE) {
        if (!MiAddressToPde(CurrentVa)->Hard.Valid) {
            continue;
        }
 
        PMMPTE Pte = MiAddressToPte(CurrentVa);
        if (Pte->Hard.Valid) {
            MI_ERASE_PTE(Pte); // Hapus mapping murni
            MMI_INVALIDATE_TLB(CurrentVa);
        }
    }
 
    // 2. Lepas VAD-nya. Karena PTE sudah dihapus (Hard.Valid == 0),
    // MmFreeVirtualRange tidak akan memanggil MmFreePhysicalPage untuk frame IO.
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
    PMMPDE Pde;
    PMMPTE Pte;

    if (Mdl->MdlFlags & MDL_PAGES_LOCKED)
    {
        KsBugCheckEx(MEMORY_MANAGEMENT_ERROR, 0x1111, (ULONG_PTR)Mdl, 0, 0);
    }

    PageCount = (Mdl->ByteCount + Mdl->ByteOffset + (PAGE_SIZE - 1)) / PAGE_SIZE;
    CurrentVa = (ULONG)Mdl->StartVa;

    for (i = 0; i < PageCount; i++)
    {
        /* Cegah user-mode menyentuh ruang alamat kernel */
        if (AccessMode != KernelMode && CurrentVa >= 0x80000000UL)
        {
            KsBugCheckEx(MEMORY_MANAGEMENT_ERROR, 0x1112, CurrentVa, 0, 0);
        }

        volatile UCHAR *TouchVa = (volatile UCHAR *)CurrentVa;

RetryFault:
        Pde = MiAddressToPde(CurrentVa);
        if (!Pde->Hard.Valid)
        {
            if (Operation != IoReadAccess) {
                *TouchVa = *TouchVa;
            } else {
                (void)*TouchVa;
            }
            goto RetryFault;
        }

        /* Halaman 4MB kernel: tidak punya PTE terpisah, PFN langsung dari PDE */
        if (Pde->Hard.LargePage)
        {
            Mdl->PfnArray[i] = ((Pde->Long & 0xFFC00000UL) |
                                (CurrentVa & 0x003FF000UL)) >> PAGE_SHIFT;
            CurrentVa += PAGE_SIZE;
            continue;
        }

        Pte = MiAddressToPte(CurrentVa);
        if (!Pte->Hard.Valid)
        {
            if (Operation != IoReadAccess) {
                *TouchVa = *TouchVa;
            } else {
                (void)*TouchVa;
            }
            goto RetryFault;
        }

        /* Cek apakah butuh Write tapi page masih Read-Only (CoW scenario) */
        if (Operation != IoReadAccess && !Pte->Hard.Write)
        {
            *TouchVa = *TouchVa;
            goto RetryFault;
        }

        ULONG Pfn = Pte->Hard.PageFrameNumber;
        MmReferencePhysicalPage(Pfn << PAGE_SHIFT);
        Mdl->PfnArray[i] = Pfn;

        CurrentVa += PAGE_SIZE;
    }

    Mdl->MdlFlags |= MDL_PAGES_LOCKED;
}

NO_THREAD_SAFETY_ANALYSIS
PVOID
VEAPI
MiMapTempPhysicalPage(
    IN ULONG PhysAddr,
    OUT PKIRQL OldIrql
)
{
    PMMPDE TempPde = MiAddressToPde(MI_TEMP_ZERO_VA);
    PMMPTE TempPte = MiAddressToPte(MI_TEMP_ZERO_VA);
    MMPTE NewEntry;

    KsAcquireSpinLock(&MmpTempMapLock, OldIrql);

    /* Pastikan Page Table untuk MI_TEMP_ZERO_VA (0xDFC00000) sudah ada di CR3 aktif */
    if (!TempPde->Hard.Valid)
    {
        ULONG PtPhys = MmAllocatePhysicalPage();
        if (PtPhys == 0)
        {
            KsReleaseSpinLock(&MmpTempMapLock, *OldIrql);
            return NULL;
        }

        MI_MAKE_VALID_PTE(NewEntry, PtPhys >> 12, 0, 1); // KernelMode, ReadWrite
        TempPde->Long = NewEntry.Long;

        /* Invalidate halaman recursive PT (0xfff7f000) lalu nol-kan isinya */
        PVOID PtVirtualPage = (PVOID)((ULONG_PTR)TempPte & ~(PAGE_SIZE - 1));
        MMI_INVALIDATE_TLB(PtVirtualPage);
        RtlZeroMemory(PtVirtualPage, PAGE_SIZE);
    }

    /* Map halaman fisik target ke MI_TEMP_ZERO_VA */
    MI_MAKE_VALID_PTE(NewEntry, PhysAddr >> 12, 0, 1);
    TempPte->Long = NewEntry.Long;

    MMI_INVALIDATE_TLB(MI_TEMP_ZERO_VA);
    return (PVOID)MI_TEMP_ZERO_VA;
}

NO_THREAD_SAFETY_ANALYSIS
VOID
VEAPI
MiUnmapTempPhysicalPage(
    IN KIRQL OldIrql
)
{
    PMMPTE TempPte = MiAddressToPte(MI_TEMP_ZERO_VA);

    TempPte->Long = 0;
    MMI_INVALIDATE_TLB(MI_TEMP_ZERO_VA);

    KsReleaseSpinLock(&MmpTempMapLock, OldIrql);
}
 
BOOLEAN
VEAPI
MmpEnsurePageTable(ULONG VirtualAddress)
{
    PMMPDE Pde = MiAddressToPde(VirtualAddress);
    MMPTE TempPde;
    ULONG NewPtPhys;
    KIRQL OldIrql;

    if (Pde->Hard.Valid)
    {
        return TRUE;
    }

    NewPtPhys = MmAllocatePhysicalPage();
    if (NewPtPhys == 0)
    {
        DPRINT("[PDE TRACE] FAIL: MmAllocatePhysicalPage for PT returned 0!\n\r");
        return FALSE;
    }

    /* Nol-kan lewat Temp/Hyperspace PTE SEBELUM PDE dipublikasikan */
    PVOID TempPtVa = MiMapTempPhysicalPage(NewPtPhys, &OldIrql);
    RtlZeroMemory(TempPtVa, PAGE_SIZE);
    MiUnmapTempPhysicalPage(OldIrql);

    /* VA user wajib dapat PDE dengan Owner (user) = 1, VA kernel = 0 */
    MI_MAKE_VALID_PTE(TempPde, NewPtPhys >> 12,
                      MI_IS_USER_ADDRESS(VirtualAddress), 1);

    /* Publikasikan PDE secara atomik */
    if (!__sync_bool_compare_and_swap((volatile ULONG *)&Pde->Long, 0, TempPde.Long))
    {
        MmFreePhysicalPage(NewPtPhys);
        return Pde->Hard.Valid ? TRUE : FALSE;
    }

    MMI_INVALIDATE_TLB(MiAddressToPte(VirtualAddress));
    MMI_INVALIDATE_TLB(VirtualAddress);

    return TRUE;
}

VOID
VEAPI
MmUnlockPages(
    PMDL Mdl
)
{
    ULONG PageCount;
    ULONG CurrentVa;
    ULONG i;

    if (!Mdl || !(Mdl->MdlFlags & MDL_PAGES_LOCKED))
    {
        return;
    }

    PageCount = (Mdl->ByteCount + Mdl->ByteOffset + (PAGE_SIZE - 1)) / PAGE_SIZE;
    CurrentVa = (ULONG)Mdl->StartVa;

    for (i = 0; i < PageCount; i++)
    {
        if (Mdl->PfnArray[i] != 0)
        {
            BOOLEAN IsLargeKernelPage = (CurrentVa >= 0x80000000UL) &&
                                        MiAddressToPde(CurrentVa)->Hard.Valid &&
                                        MiAddressToPde(CurrentVa)->Hard.LargePage;

            if (!IsLargeKernelPage)
            {
                MmDereferencePhysicalPage(Mdl->PfnArray[i] << PAGE_SHIFT);
            }
        }

        Mdl->PfnArray[i] = 0;
        CurrentVa += PAGE_SIZE;
    }

    Mdl->MdlFlags &= ~MDL_PAGES_LOCKED;
}

/* EOF */