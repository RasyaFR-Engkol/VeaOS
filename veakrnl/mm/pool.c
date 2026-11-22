#include <veakrnl.h>

PVOID
VEAPI
MmAllocatePoolPages(POOL_TYPE PoolType, ULONG NumberOfPages)
{
    ULONG ReservedPages;
    ULONG VirtualAddress;
    ULONG CurrentVa;
    ULONG MappedPages = 0;
 
    if (PoolType >= MaxPoolType ||
        NumberOfPages == 0 ||
        NumberOfPages == (ULONG)-1)
    {
        return NULL;
    }
 
    /* Keep one unmapped page after each pool allocation as an overrun guard. */
    ReservedPages = NumberOfPages + 1;
    VirtualAddress = MmAllocateVirtualRange(ReservedPages);
    if (VirtualAddress == 0) {
        kdp_print("MM FATAL: Failed to get Virtual Range for Pool!\n\r");
        return NULL;
    }
 
    CurrentVa = VirtualAddress;
 
    for (ULONG i = 0; i < NumberOfPages; i++) {
        ULONG PhysPage = MmAllocatePhysicalPage();
        if (PhysPage == 0) {
            kdp_print("MM: Out of physical memory during pool allocation.\n\r");
            goto Failure;
        }
 
        PMMPDE Pde = MiAddressToPde(CurrentVa);
        if (!Pde->Hard.Valid) {
            ULONG NewPtPhys = MmAllocatePhysicalPage();
            if (NewPtPhys == 0) {
                MmFreePhysicalPage(PhysPage);
                goto Failure;
            }
 
            MI_WRITE_PDE_VALID(Pde, NewPtPhys >> 12); 
 
            PVOID PtVirtualAddr = (PVOID)((ULONG)MiAddressToPte(CurrentVa) & 0xFFFFF000);
            MMI_INVALIDATE_TLB(PtVirtualAddr);
            RtlZeroMemory(PtVirtualAddr, PAGE_SIZE);
        }
 
        PMMPTE Pte = MiAddressToPte(CurrentVa);
        if (Pte->Hard.Valid) {
            MmFreePhysicalPage(PhysPage);
            goto Failure;
        }
 
        MMI_WRITE_PTE_VALID(Pte, PhysPage >> 12);
        MMI_INVALIDATE_TLB(CurrentVa);
 
        MappedPages++;
        CurrentVa += PAGE_SIZE;
    }
 
    {
        ULONG GuardVa = VirtualAddress + NumberOfPages * PAGE_SIZE;
 
        if (MiAddressToPde(GuardVa)->Hard.Valid &&
            MiAddressToPte(GuardVa)->Hard.Valid)
        {
            KsBugCheckEx(BAD_POOL_HEADER, 14, GuardVa,
                         MiAddressToPte(GuardVa)->Long, NumberOfPages);
            return NULL;
        }
    }
 
    return (PVOID)VirtualAddress;
 
Failure:
    if (MappedPages != 0)
    {
        MmFreePoolPages((PVOID)VirtualAddress, MappedPages);
    }
    else
    {
        MmFreeVirtualRange(VirtualAddress);
    }
    return NULL;
}


PVOID
VEAPI
MmAllocatePoolPage(POOL_TYPE PoolType)
{
    // Cukup panggil versi jamaknya dengan jumlah 1 Page
    return MmAllocatePoolPages(PoolType, 1);
}

PVOID
VEAPI
MmAllocateContiguousMemory(
    IN SIZE_T NumberOfBytes,
    IN PHYSICAL_ADDRESS HighestAcceptableAddress
)
{
    ULONG PageCount;
    ULONG PhysicalAddress;
    ULONGLONG LastPhysicalAddress;
    PVOID VirtualAddress;

    if (NumberOfBytes == 0 || NumberOfBytes > 0xFFFFFFFFUL - (PAGE_SIZE - 1))
    {
        return NULL;
    }

    PageCount = ((ULONG)NumberOfBytes + PAGE_SIZE - 1) / PAGE_SIZE;
    PhysicalAddress = MmAllocateContiguousPhysicalPages(PageCount);
    if (PhysicalAddress == 0)
    {
        return NULL;
    }

    LastPhysicalAddress = (ULONGLONG)PhysicalAddress +
                          ((ULONGLONG)PageCount * PAGE_SIZE) - 1;
    if (LastPhysicalAddress > (ULONGLONG)HighestAcceptableAddress.QuadPart)
    {
        MmFreeContiguousPhysicalPages(PhysicalAddress, PageCount);
        return NULL;
    }

    VirtualAddress = MmMapIoSpace(
        PhysicalAddress,
        (ULONG)NumberOfBytes,
        MmCached
    );
    if (!VirtualAddress)
    {
        MmFreeContiguousPhysicalPages(PhysicalAddress, PageCount);
        return NULL;
    }

    return VirtualAddress;
}

VOID
VEAPI
MmFreeContiguousMemory(IN PVOID BaseAddress)
{
    PHYSICAL_ADDRESS BasePhysicalAddress;
    PHYSICAL_ADDRESS CurrentPhysicalAddress;
    ULONG PageCount;

    if (BaseAddress == NULL)
    {
        return;
    }

    PageCount = MmGetVirtualRangePageCount((ULONG)BaseAddress);
    if (PageCount == 0)
    {
        kdp_print("MM WARNING: Tried to free an invalid contiguous allocation!\n\r");
        return;
    }

    BasePhysicalAddress = MmGetPhysicalAddress(BaseAddress);
    if (BasePhysicalAddress.QuadPart == 0 ||
        (BasePhysicalAddress.LowPart & (PAGE_SIZE - 1)) != 0)
    {
        kdp_print("MM WARNING: Contiguous allocation has no valid physical base!\n\r");
        return;
    }

    for (ULONG i = 1; i < PageCount; i++)
    {
        CurrentPhysicalAddress = MmGetPhysicalAddress(
            (PVOID)((ULONG)BaseAddress + i * PAGE_SIZE)
        );
        if (CurrentPhysicalAddress.QuadPart !=
            BasePhysicalAddress.QuadPart + (ULONGLONG)i * PAGE_SIZE)
        {
            kdp_print("MM WARNING: Contiguous allocation mapping is not contiguous!\n\r");
            return;
        }
    }

    MmUnmapIoSpace(BaseAddress, PageCount * PAGE_SIZE);
    MmFreeContiguousPhysicalPages(BasePhysicalAddress.LowPart, PageCount);
}

VOID
VEAPI
MmFreePoolPages(PVOID BaseAddress, ULONG NumberOfPages)
{
    if (BaseAddress == NULL || NumberOfPages == 0) return;
 
    ULONG CurrentVa = (ULONG)BaseAddress;
 
    // 1. Unmap Physical Pages satu per satu
    for (ULONG i = 0; i < NumberOfPages; i++, CurrentVa += PAGE_SIZE) {
        /* PDE harus valid dulu, kalau tidak membaca PTE lewat recursive mapping fault */
        if (!MiAddressToPde(CurrentVa)->Hard.Valid) {
            continue;
        }
 
        PMMPTE Pte = MiAddressToPte(CurrentVa);
        if (Pte->Hard.Valid) {
            ULONG PhysPage = Pte->Hard.PageFrameNumber << PAGE_SHIFT;
 
            // Hapus isi PTE sepenuhnya biar nggak ada sisa akses
            MI_ERASE_PTE(Pte);
            MMI_INVALIDATE_TLB(CurrentVa);
 
            // Do not recycle the frame until its old virtual mapping is gone.
            MmFreePhysicalPage(PhysPage);
        }
    }
 
    // 2. Kembalikan rentang Virtual Address ke VAD/AVL
    MmFreeVirtualRange((ULONG)BaseAddress);
}


VOID
VEAPI
MmFreePoolPage(PVOID BaseAddress)
{
    MmFreePoolPages(BaseAddress, 1);
}