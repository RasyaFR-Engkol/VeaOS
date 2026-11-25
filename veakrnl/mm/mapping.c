#include <veakrnl.h>

KSPIN_LOCK MmpTempMapLock;

#define MI_KERNEL_PDE_FIRST 768
#define MI_KERNEL_PDE_COUNT 255

static ULONG MmKernelPageDirectory[MI_KERNEL_PDE_COUNT];
static KSPIN_LOCK MmKernelPageDirectoryLock;
static BOOLEAN MmKernelPageDirectoryInitialized;

LONG 
VEAPI
MmpCompareVad(PRTL_BALANCED_NODE NodeA, PRTL_BALANCED_NODE NodeB);

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
        if (!MmpEnsurePageTable(CurrentVa)) {
            kdp_print("MM: MmMapIoSpace failed to ensure Page Table!\n\r");
            if (i != 0) {
                MmUnmapIoSpace((PVOID)BaseVa, i * PAGE_SIZE);
            } else {
                MmFreeVirtualRange(BaseVa);
            }
            return NULL;
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

BOOLEAN
VEAPI
MmInitializeKernelPageDirectory(VOID)
{
    PULONG PageDirectory = (PULONG)MM_GET_PD();

    KsInitializeSpinlock(&MmKernelPageDirectoryLock);

    for (ULONG i = 0; i < MI_KERNEL_PDE_COUNT; i++)
    {
        ULONG PdeIndex = MI_KERNEL_PDE_FIRST + i;

        /* Index 1023 adalah recursive mapping milik masing-masing proses */
        if (PdeIndex != 1023 && (PageDirectory[PdeIndex] & 0x1) == 0)
        {
            ULONG PtPhys = MmAllocatePhysicalPage();
            if (PtPhys == 0)
            {
                return FALSE;
            }

            PVOID PtVa = MmMapIoSpace(PtPhys, PAGE_SIZE, MmCached);
            if (!PtVa)
            {
                MmFreePhysicalPage(PtPhys);
                return FALSE;
            }

            RtlZeroMemory(PtVa, PAGE_SIZE);
            MmUnmapIoSpace(PtVa, PAGE_SIZE);

            /* Present (0x1) | Read/Write (0x2), Supervisor-only */
            PageDirectory[PdeIndex] = PtPhys | 0x03;
        }

        MmKernelPageDirectory[i] = PageDirectory[PdeIndex];
    }

    MmKernelPageDirectoryInitialized = TRUE;
    return TRUE;
}

BOOLEAN
VEAPI
MmpCopyKernelPageDirectory(PULONG Destination)
{
    KIRQL OldIrql;

    if (!Destination || !MmKernelPageDirectoryInitialized)
    {
        return FALSE;
    }

    KsAcquireSpinLock(&MmKernelPageDirectoryLock, &OldIrql);
    for (ULONG i = 0; i < MI_KERNEL_PDE_COUNT; i++)
    {
        Destination[MI_KERNEL_PDE_FIRST + i] = MmKernelPageDirectory[i];
    }
    KsReleaseSpinLock(&MmKernelPageDirectoryLock, OldIrql);

    return TRUE;
}

BOOLEAN
VEAPI
MmpSynchronizeKernelPageTable(ULONG VirtualAddress)
{
    ULONG PdeIndex = VirtualAddress >> 22;
    PMMPDE Pde = MiAddressToPde(VirtualAddress);
    KIRQL OldIrql;
    ULONG MasterPde;

    if (MI_IS_USER_ADDRESS(VirtualAddress) ||
        PdeIndex < MI_KERNEL_PDE_FIRST ||
        PdeIndex >= 1023)
    {
        return Pde->Hard.Valid;
    }

    if (Pde->Hard.Valid || !MmKernelPageDirectoryInitialized)
    {
        return Pde->Hard.Valid;
    }

    KsAcquireSpinLock(&MmKernelPageDirectoryLock, &OldIrql);
    MasterPde = MmKernelPageDirectory[PdeIndex - MI_KERNEL_PDE_FIRST];
    if (MasterPde != 0)
    {
        Pde->Long = MasterPde;
        MMI_INVALIDATE_TLB((ULONG)MiAddressToPte(VirtualAddress) & ~(PAGE_SIZE - 1));
        MMI_INVALIDATE_TLB(VirtualAddress);
    }
    KsReleaseSpinLock(&MmKernelPageDirectoryLock, OldIrql);

    return MasterPde != 0;
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
        ULONG PdeIndex = (ULONG)((ULONG_PTR)MI_TEMP_ZERO_VA >> 22);
        ULONG PtPhys = 0;
        KIRQL PdeIrql;

        if (MmKernelPageDirectoryInitialized)
        {
            KsAcquireSpinLock(&MmKernelPageDirectoryLock, &PdeIrql);
            ULONG MasterPde = MmKernelPageDirectory[PdeIndex - MI_KERNEL_PDE_FIRST];
            if (MasterPde != 0)
            {
                TempPde->Long = MasterPde;
                PVOID PtVirtualPage =
                    (PVOID)((ULONG_PTR)TempPte & ~(PAGE_SIZE - 1));
                MMI_INVALIDATE_TLB(PtVirtualPage);
            }
            KsReleaseSpinLock(&MmKernelPageDirectoryLock, PdeIrql);

            if (MasterPde == 0)
            {
                PtPhys = MmAllocatePhysicalPage();
                if (PtPhys == 0)
                {
                    KsReleaseSpinLock(&MmpTempMapLock, *OldIrql);
                    return NULL;
                }

                KsAcquireSpinLock(&MmKernelPageDirectoryLock, &PdeIrql);
                MasterPde = MmKernelPageDirectory[PdeIndex - MI_KERNEL_PDE_FIRST];
                if (MasterPde == 0)
                {
                    MMPTE NewPde;
                    MI_MAKE_VALID_PTE(NewPde, PtPhys >> 12, 0, 1);
                    TempPde->Long = NewPde.Long;
                    MmKernelPageDirectory[PdeIndex - MI_KERNEL_PDE_FIRST] =
                        NewPde.Long;

                    PVOID PtVirtualPage =
                        (PVOID)((ULONG_PTR)TempPte & ~(PAGE_SIZE - 1));
                    MMI_INVALIDATE_TLB(PtVirtualPage);
                    RtlZeroMemory(PtVirtualPage, PAGE_SIZE);
                    PtPhys = 0;
                }
                else
                {
                    TempPde->Long = MasterPde;
                    PVOID PtVirtualPage =
                        (PVOID)((ULONG_PTR)TempPte & ~(PAGE_SIZE - 1));
                    MMI_INVALIDATE_TLB(PtVirtualPage);
                }
                KsReleaseSpinLock(&MmKernelPageDirectoryLock, PdeIrql);

                if (PtPhys != 0)
                {
                    MmFreePhysicalPage(PtPhys);
                }
            }
        }
        else
        {
            PtPhys = MmAllocatePhysicalPage();
            if (PtPhys == 0)
            {
                KsReleaseSpinLock(&MmpTempMapLock, *OldIrql);
                return NULL;
            }

            MI_MAKE_VALID_PTE(NewEntry, PtPhys >> 12, 0, 1);
            TempPde->Long = NewEntry.Long;

            PVOID PtVirtualPage =
                (PVOID)((ULONG_PTR)TempPte & ~(PAGE_SIZE - 1));
            MMI_INVALIDATE_TLB(PtVirtualPage);
            RtlZeroMemory(PtVirtualPage, PAGE_SIZE);
        }
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

    if (!MI_IS_USER_ADDRESS(VirtualAddress) &&
        ((ULONG_PTR)VirtualAddress >> 22) >= MI_KERNEL_PDE_FIRST &&
        ((ULONG_PTR)VirtualAddress >> 22) < 1023 &&
        MmKernelPageDirectoryInitialized)
    {
        ULONG PdeIndex = VirtualAddress >> 22;
        ULONG MasterPde;

        KsAcquireSpinLock(&MmKernelPageDirectoryLock, &OldIrql);
        MasterPde = MmKernelPageDirectory[PdeIndex - MI_KERNEL_PDE_FIRST];
        if (MasterPde != 0)
        {
            if (!Pde->Hard.Valid)
            {
                Pde->Long = MasterPde;
                MMI_INVALIDATE_TLB((ULONG)MiAddressToPte(VirtualAddress) &
                                   ~(PAGE_SIZE - 1));
                MMI_INVALIDATE_TLB(VirtualAddress);
            }
            KsReleaseSpinLock(&MmKernelPageDirectoryLock, OldIrql);
            return Pde->Hard.Valid;
        }

        if (Pde->Hard.Valid)
        {
            MmKernelPageDirectory[PdeIndex - MI_KERNEL_PDE_FIRST] = Pde->Long;
            KsReleaseSpinLock(&MmKernelPageDirectoryLock, OldIrql);
            return TRUE;
        }
        KsReleaseSpinLock(&MmKernelPageDirectoryLock, OldIrql);
    }

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
    if (!TempPtVa)
    {
        MmFreePhysicalPage(NewPtPhys);
        return FALSE;
    }
    RtlZeroMemory(TempPtVa, PAGE_SIZE);
    MiUnmapTempPhysicalPage(OldIrql);

    /* VA user wajib dapat PDE dengan Owner (user) = 1, VA kernel = 0 */
    MI_MAKE_VALID_PTE(TempPde, NewPtPhys >> 12,
                      MI_IS_USER_ADDRESS(VirtualAddress), 1);

    if (!MI_IS_USER_ADDRESS(VirtualAddress) &&
        ((ULONG_PTR)VirtualAddress >> 22) >= MI_KERNEL_PDE_FIRST &&
        ((ULONG_PTR)VirtualAddress >> 22) < 1023 &&
        MmKernelPageDirectoryInitialized)
    {
        ULONG PdeIndex = VirtualAddress >> 22;
        ULONG MasterPde;
        BOOLEAN InstalledNewPde = FALSE;

        KsAcquireSpinLock(&MmKernelPageDirectoryLock, &OldIrql);
        MasterPde = MmKernelPageDirectory[PdeIndex - MI_KERNEL_PDE_FIRST];
        if (MasterPde != 0)
        {
            if (!Pde->Hard.Valid)
            {
                Pde->Long = MasterPde;
                MMI_INVALIDATE_TLB((ULONG)MiAddressToPte(VirtualAddress) &
                                   ~(PAGE_SIZE - 1));
                MMI_INVALIDATE_TLB(VirtualAddress);
            }
            KsReleaseSpinLock(&MmKernelPageDirectoryLock, OldIrql);
            MmFreePhysicalPage(NewPtPhys);
            return Pde->Hard.Valid;
        }

        if (!Pde->Hard.Valid)
        {
            Pde->Long = TempPde.Long;
            MmKernelPageDirectory[PdeIndex - MI_KERNEL_PDE_FIRST] =
                TempPde.Long;
            InstalledNewPde = TRUE;
        }
        else
        {
            MmKernelPageDirectory[PdeIndex - MI_KERNEL_PDE_FIRST] = Pde->Long;
        }
        KsReleaseSpinLock(&MmKernelPageDirectoryLock, OldIrql);
        if (!InstalledNewPde)
        {
            MmFreePhysicalPage(NewPtPhys);
        }
        MMI_INVALIDATE_TLB((ULONG)MiAddressToPte(VirtualAddress) &
                           ~(PAGE_SIZE - 1));
        MMI_INVALIDATE_TLB(VirtualAddress);
        return TRUE;
    }

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