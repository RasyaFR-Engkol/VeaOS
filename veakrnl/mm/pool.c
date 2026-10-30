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

        PAGE_ENTRY *pde = MM_GET_PDE(CurrentVa);
        if (!pde->bits.present) {
            ULONG NewPtPhys = MmAllocatePhysicalPage();
            if (NewPtPhys == 0) {
                MmFreePhysicalPage(PhysPage);
                goto Failure;
            }

            MMI_WRITE_PDE_VALID(pde, NewPtPhys >> 12);

            PVOID PtVirtualAddr = (PVOID)((ULONG)MM_GET_PTE(CurrentVa) & 0xFFFFF000);
            MMI_INVALIDATE_TLB((ULONG)PtVirtualAddr);
            RtlZeroMemory(PtVirtualAddr, 4096);
        }

        PAGE_ENTRY *pte = MM_GET_PTE(CurrentVa);
        if (pte->bits.present) {
            MmFreePhysicalPage(PhysPage);
            goto Failure;
        }

        MMI_WRITE_PTE_VALID(pte, PhysPage >> 12);
        MMI_INVALIDATE_TLB(CurrentVa);

        MappedPages++;
        CurrentVa += 4096;
    }

    {
        ULONG GuardVa = VirtualAddress + NumberOfPages * PAGE_SIZE;
        PAGE_ENTRY *GuardPde = MM_GET_PDE(GuardVa);

        if (GuardPde->bits.present &&
            MM_GET_PTE(GuardVa)->bits.present)
        {
            KsBugCheckEx(BAD_POOL_HEADER, 14, GuardVa,
                         MM_GET_PTE(GuardVa)->raw, NumberOfPages);
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
    for (ULONG i = 0; i < NumberOfPages; i++) {
        PAGE_ENTRY *pte = MM_GET_PTE(CurrentVa);
        
        if (pte->bits.present) {
            // Dapatkan alamat fisik murni (hilangkan 12 bit flag di bawah)
            ULONG PhysPage = (*(ULONG*)pte) & 0xFFFFF000; 

            // Hapus isi PTE sepenuhnya biar nggak ada sisa akses
            MI_WRITE_INVALID_PTE(pte);
            MMI_INVALIDATE_TLB(CurrentVa);

            // Do not recycle the frame until its old virtual mapping is gone.
            MmFreePhysicalPage(PhysPage);
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