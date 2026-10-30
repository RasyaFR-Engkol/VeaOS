/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : pip.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PIP and Physical Page management
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 19-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file pip.c
 * --------------------------------------------------------------------- */

/* VARIABLE */

static ULONG MmPipPageTableNextPhys;
static ULONG MmPipPageTableEndPhys;
static KSPIN_LOCK MmPipFreeListLock;
ULONG MmNumberOfPhysicalPages = 0;
MM_SYSTEMSIZE MmSystemSize = MmSmallSystem;
ULONG MmMaximumDeadKernelStacks = 0;
ULONG MmSystemCacheWsMinimum = 0;
ULONG MmFirstFreePip = 0xFFFFFFFF;
ULONG MmFirstZeroedPip = 0xFFFFFFFF;
ULONG MmZeroScratchVa = 0;
KEVENT MmFreePageEvent;
RTL_BITMAP MiPipBitmap;
PFN_NUMBER MmHighestPhysicalPage;

/* PRIVATE */

static VOID
MmpInsertZeroedPageHead(IN ULONG Index)
{
    MmPipDatabase[Index].Flink.NextPip = MmFirstZeroedPip;
    MmPipDatabase[Index].Blink.PrevPip = 0xFFFFFFFF;

    if (MmFirstZeroedPip != 0xFFFFFFFF) {
        MmPipDatabase[MmFirstZeroedPip].Blink.PrevPip = Index;
    }

    MmFirstZeroedPip = Index;
}

static
VOID
MmpUnlinkFreePage(IN ULONG Index)
{
    ULONG Flink = MmPipDatabase[Index].Flink.NextPip;
    ULONG Blink = MmPipDatabase[Index].Blink.PrevPip;

    if (Blink != 0xFFFFFFFF) {
        MmPipDatabase[Blink].Flink.NextPip = Flink;
    } else {
        MmFirstFreePip = Flink; // Node pertama (Head) yang dicabut
    }

    if (Flink != 0xFFFFFFFF) {
        MmPipDatabase[Flink].Blink.PrevPip = Blink;
    }

    MmPipDatabase[Index].Flink.NextPip = 0xFFFFFFFF;
    MmPipDatabase[Index].Blink.PrevPip = 0xFFFFFFFF;
}

static
VOID
MmpInsertFreePageHead(IN ULONG Index)
{
    MmPipDatabase[Index].Flink.NextPip = MmFirstFreePip;
    MmPipDatabase[Index].Blink.PrevPip = 0xFFFFFFFF;

    if (MmFirstFreePip != 0xFFFFFFFF) {
        MmPipDatabase[MmFirstFreePip].Blink.PrevPip = Index;
    }

    MmFirstFreePip = Index;
}

/* FUNCTION */

VOID
VEAPI
MmReferencePhysicalPage(ULONG PhysicalAddress)
{
    ULONG Index = PhysicalAddress / PAGE_SIZE;
    KIRQL OldIrql;

    if (Index >= MmTotalPages) return;

    KsAcquireSpinLock(&MmPipFreeListLock, &OldIrql);
    MmPipDatabase[Index].ReferenceCount++;
    KsReleaseSpinLock(&MmPipFreeListLock, OldIrql);
}

VOID
VEAPI
MmDereferencePhysicalPage(ULONG PhysicalAddress)
{
    ULONG Index = PhysicalAddress / 4096;
    KIRQL OldIrql;

    if (Index >= MmTotalPages) return;

    KsAcquireSpinLock(&MmPipFreeListLock, &OldIrql);
    
    if (MmPipDatabase[Index].ReferenceCount > 0) {
        MmPipDatabase[Index].ReferenceCount--;
        
        if (MmPipDatabase[Index].ReferenceCount == 0) {
            MmPipDatabase[Index].ShareCount = 0;
            MmPipDatabase[Index].State = PIP_STATE_FREE;
            MmpInsertFreePageHead(Index);
        }
    }

    KsReleaseSpinLock(&MmPipFreeListLock, OldIrql);
}

BOOLEAN 
VEAPI 
MmInitializePip(PBLOCK_BOOT_2 BlockBoot)
{
    /* Initialize SpinLock */
    KsInitializeSpinlock(&MmPipFreeListLock);

    /* Initialize Event */
    KsInitializeEvent(&MmFreePageEvent, SynchronizationEvent, FALSE);

    /* Get Memory Descriptor Information from MemoryMap */
    ULONG highest_address = 0;
    PVEA_MEMORY_DESCRIPTOR mmap = BlockBoot->MemoryMap;
    ULONG mmap_count = BlockBoot->MemoryMapCount;
    MmNumberOfPhysicalPages = 0;

    /* Check for Highest Address, and count Number of Physical Pages */
    for (ULONG i = 0; i < mmap_count; i++) {
        if (mmap[i].Type == VeaMemoryTypeUsable) {
            ULONG end_addr = mmap[i].BaseAddress + mmap[i].Length;
            if (end_addr > highest_address) {
                highest_address = end_addr;
            }

            MmNumberOfPhysicalPages += (mmap[i].Length / PAGE_SIZE);
        }
    }

    if (highest_address > 0) {
        MmHighestPhysicalPage = (PFN_NUMBER)((highest_address - 1) / PAGE_SIZE);
    } else {
        MmHighestPhysicalPage = 0;
    }

    DPRINT("MM: Highest Physical Page set to PFN %u (Address: 0x%08X)\n", 
           (ULONG)MmHighestPhysicalPage, highest_address);

    /* Determine what is the system size */
    ULONG TotalRamMb = MmNumberOfPhysicalPages / 256;

    if (TotalRamMb < 256)
    {
        MmSystemSize = MmSmallSystem;
        MmMaximumDeadKernelStacks = 2;
        MmSystemCacheWsMinimum = 100;
    }
    else if (TotalRamMb < 1024)
    {
        MmSystemSize = MmMediumSystem;
        MmMaximumDeadKernelStacks = 8;
        MmSystemCacheWsMinimum = 512;
    }
    else
    {
        MmSystemSize = MmLargeSystem;
        MmMaximumDeadKernelStacks = 32;
        MmSystemCacheWsMinimum = 2048;
    }

    DPRINT("MM: System size configured as %s (%u MB RAM detected)\n",
       (MmSystemSize == MmSmallSystem) ? "Small" :
       (MmSystemSize == MmMediumSystem) ? "Medium" : "Large",
       TotalRamMb);

    /* Count available Total Pages */
    MmTotalPages = highest_address / 4096;

    ULONG DatabaseSize = MmTotalPages * sizeof(PIP);
    ULONG DatabasePageSpan = (DatabaseSize + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    ULONG kernel_phys_end = (ULONG)&_veaos_end - 0xC0000000;
    kernel_phys_end = (kernel_phys_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    ULONG pip_db_phys = 0;
    for (ULONG i = 0; i < mmap_count; i++) {
        if (mmap[i].Type == VeaMemoryTypeUsable) {
            ULONG usable_start = mmap[i].BaseAddress;
            if (usable_start < kernel_phys_end) {
                usable_start = kernel_phys_end;
            }
            usable_start = (usable_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

            ULONG block_end = mmap[i].BaseAddress + mmap[i].Length;
            ULONG candidate_va = usable_start + 0xC0000000;
            ULONG candidate_va_end = candidate_va + DatabaseSize;
            ULONG first_pde = candidate_va >> 22;
            ULONG last_pde = (candidate_va_end - 1) >> 22;
            ULONG page_table_pages = 0;

            for (ULONG pde_index = first_pde; pde_index <= last_pde; pde_index++) {
                if (!MM_GET_PD()[pde_index].bits.present) {
                    page_table_pages++;
                }
            }

            ULONG reserved_end = usable_start + DatabasePageSpan +
                                 (page_table_pages * PAGE_SIZE);
            if (block_end <= usable_start || block_end < reserved_end) {
                continue;
            }

            pip_db_phys = usable_start;
            MmPipPageTableNextPhys = usable_start + DatabasePageSpan;
            MmPipPageTableEndPhys = reserved_end;

            mmap[i].BaseAddress = reserved_end;
            mmap[i].Length = block_end - reserved_end;
            break;
        }
    }

    if (pip_db_phys == 0) {
        kdp_print("MM ERROR: Not enough memory for PIP database!\n\r");
        return 0;
    }

    MmPipDatabase = (PPIP)(pip_db_phys + 0xC0000000);
    DPRINT("MM: PIP db phys=%08X bytes=%08X span=%08X PT phys=%08X-%08X\n",
           pip_db_phys, DatabaseSize, DatabasePageSpan,
           MmPipPageTableNextPhys, MmPipPageTableEndPhys);

    if (!MmMapPip()) {
        kdp_print("MM ERROR: Failed to map PIP database!\n\r");
        return FALSE;
    }

    for (ULONG i = 0; i < MmTotalPages; i++) {
        MmPipDatabase[i].State = PIP_STATE_RESERVED;
        MmPipDatabase[i].ReferenceCount = 0;
        MmPipDatabase[i].ShareCount = 0;
        MmPipDatabase[i].PteAddress = NULL;
        MmPipDatabase[i].Flags = 0;
        MmPipDatabase[i].Flink.NextPip = 0xFFFFFFFF;
        MmPipDatabase[i].Blink.PrevPip = 0xFFFFFFFF;
    }
    
    for (ULONG i = 0; i < mmap_count; i++) {
        if (mmap[i].Type == VeaMemoryTypeUsable) {
            ULONG start_page = mmap[i].BaseAddress / 4096;
            ULONG page_count = mmap[i].Length / 4096;

            for (ULONG j = 0; j < page_count; j++) {
                ULONG current_index = start_page + j;
                if (current_index < MmTotalPages) {
                    MmPipDatabase[current_index].State = PIP_STATE_FREE;
                }
            }
        }
    }

    for (ULONG i = 0; i < (0x100000 / 4096); i++) {
        MmPipDatabase[i].State = PIP_STATE_RESERVED;
    }

    ULONG kernel_start_page = KERNEL_PHYS_BASE / 4096;
    ULONG kernel_end_page = (KERNEL_PHYS_BASE + KERNEL_RESERVE_SIZE) / 4096;

    for (ULONG i = kernel_start_page; i < kernel_end_page; i++) {
        if (i < MmTotalPages) {
            MmPipDatabase[i].State = PIP_STATE_RESERVED;
        }
    }

    ULONG last_free_index = 0xFFFFFFFF;

    for (ULONG i = 0; i < MmTotalPages; i++) {
        if (MmPipDatabase[i].State == PIP_STATE_FREE) {
            if (MmFirstFreePip == 0xFFFFFFFF) {
                MmFirstFreePip = i;
                MmPipDatabase[i].Blink.PrevPip = 0xFFFFFFFF;
            } else {
                MmPipDatabase[last_free_index].Flink.NextPip = i;
                MmPipDatabase[i].Blink.PrevPip = last_free_index;
            }
            last_free_index = i;
        }
    }

    if (last_free_index != 0xFFFFFFFF) {
        MmPipDatabase[last_free_index].Flink.NextPip = 0xFFFFFFFF;
    }

    kdp_print("MM: PIP Database initialized successfully.\n\r");
    return 1;
}

BOOLEAN 
VEAPI 
MmMapPip(VOID)
{
    ULONG DatabaseSize = MmTotalPages * sizeof(PIP);
    ULONG VirtualStart = (ULONG)MmPipDatabase;
    ULONG VirtualEnd = VirtualStart + DatabaseSize;
    ULONG PhysicalStart = VirtualStart - 0xC0000000;

    kdp_print("MM: Mapping PIP Database to Virtual Memory...\n\r");

    for (ULONG va = VirtualStart, phys = PhysicalStart; va < VirtualEnd; va += 4096, phys += 4096) 
    {
        PAGE_ENTRY *pde = MM_GET_PDE(va);
        
        // JIKA PAGE TABLE BELUM ADA: Kita harus bikin!
        if (!pde->bits.present) {
            if (MmPipPageTableNextPhys >= MmPipPageTableEndPhys) {
                kdp_print("MM ERROR: PIP page-table reserve exhausted!\n\r");
                return FALSE;
            }

            ULONG new_pt_phys = MmPipPageTableNextPhys;
            MmPipPageTableNextPhys += PAGE_SIZE;

            DPRINT("MM: PIP page-table backing phys=%08X for va=%08X\n", new_pt_phys, va);

            MMI_WRITE_PDE_VALID(pde, new_pt_phys >> 12);

            PVOID pt_virtual_addr = (PVOID)((ULONG)MM_GET_PTE(va) & 0xFFFFF000);
            MMI_INVALIDATE_TLB((ULONG)pt_virtual_addr);
            RtlZeroMemory(pt_virtual_addr, 4096);
        }

        PAGE_ENTRY *pte = MM_GET_PTE(va);
        MMI_WRITE_PTE_VALID(pte, phys >> 12);
        MMI_INVALIDATE_TLB(va);
    }

    kdp_print("MM: PIP Database fully mapped and safe to use.\n\r");
    return TRUE;
}

/* ALLOCATOR for PIP */

ULONG 
VEAPI
MmAllocatePhysicalPage(VOID)
{
    KIRQL OldIrql;

    KsAcquireSpinLock(&MmPipFreeListLock, &OldIrql);
    if (MmFirstFreePip == 0xFFFFFFFF) {
        KsReleaseSpinLock(&MmPipFreeListLock, OldIrql);
        DPRINT("MM FATAL: Out of Physical Memory!\n\r");
        return 0; // Return 0 tandanya gagal (OOM)
    }

    ULONG AllocatedIndex = MmFirstFreePip;
    if (AllocatedIndex >= MmTotalPages ||
        MmPipDatabase[AllocatedIndex].State != PIP_STATE_FREE)
    {
        KsReleaseSpinLock(&MmPipFreeListLock, OldIrql);
        KsBugCheckEx(BAD_POOL_HEADER, 15, AllocatedIndex,
                     MmTotalPages, 0);
        return 0;
    }

    MmpUnlinkFreePage(AllocatedIndex);

    MmPipDatabase[AllocatedIndex].State = PIP_STATE_ACTIVE;
    MmPipDatabase[AllocatedIndex].ReferenceCount = 1;
    MmPipDatabase[AllocatedIndex].ShareCount = 1;
    MmPipDatabase[AllocatedIndex].PteAddress = 0;

    KsReleaseSpinLock(&MmPipFreeListLock, OldIrql);
    return (AllocatedIndex * 4096);
}

VOID 
VEAPI 
MmFreePhysicalPage(ULONG PhysicalAddress)
{
    KIRQL OldIrql;
    BOOLEAN PageFreed = FALSE;

    if (PhysicalAddress % 4096 != 0) {
        kdp_print("MM WARNING: Tried to free unaligned physical address!\n\r");
        return;
    }

    ULONG Index = PhysicalAddress / 4096;

    if (Index >= MmTotalPages) {
        kdp_print("MM WARNING: Tried to free invalid physical address!\n\r");
        return;
    }

    KsAcquireSpinLock(&MmPipFreeListLock, &OldIrql);
    if (MmPipDatabase[Index].State != PIP_STATE_ACTIVE) {
        KsReleaseSpinLock(&MmPipFreeListLock, OldIrql);
        kdp_print("MM WARNING: Tried to free a non-active physical page!\n\r");
        return;
    }

    if (MmPipDatabase[Index].ShareCount > 0) {
        MmPipDatabase[Index].ShareCount--;
    }

    if (MmPipDatabase[Index].ReferenceCount > 0) {
        MmPipDatabase[Index].ReferenceCount--;
    }

    if (MmPipDatabase[Index].ReferenceCount == 0) {
        MmPipDatabase[Index].State = PIP_STATE_FREE;
        MmpInsertFreePageHead(Index);
        PageFreed = TRUE;
    }

    KsReleaseSpinLock(&MmPipFreeListLock, OldIrql);

    if (PageFreed)
    {
        KsSetEvent(&MmFreePageEvent, FALSE);
    }
}

ULONG
VEAPI
MmAllocateContiguousPhysicalPages(ULONG PageCount)
{
    if (PageCount == 0) return 0;

    KIRQL OldIrql;
    KsAcquireSpinLock(&MmPipFreeListLock, &OldIrql);

    ULONG contiguous_found = 0;
    ULONG start_index = 0xFFFFFFFF;

    for (ULONG i = 0; i < MmTotalPages; i++)
    {
        if (MmPipDatabase[i].State == PIP_STATE_FREE) {
            if (contiguous_found == 0) {
                start_index = i;
            }
            contiguous_found++;

            // Jika sudah menemukan jumlah halaman berurutan yang diminta
            if (contiguous_found == PageCount) {
                for (ULONG j = start_index; j < start_index + PageCount; j++) {
                    MmpUnlinkFreePage(j);
                    MmPipDatabase[j].State = PIP_STATE_ACTIVE;
                    MmPipDatabase[j].ReferenceCount = 1;
                    MmPipDatabase[j].ShareCount = 1;
                    MmPipDatabase[j].PteAddress = 0;
                }

                KsReleaseSpinLock(&MmPipFreeListLock, OldIrql);
                return (start_index * 4096);
            }
        } else {
            // Reset pencarian jika terputus oleh halaman RESERVED atau ACTIVE
            contiguous_found = 0;
            start_index = 0xFFFFFFFF;
        }
    }

    KsReleaseSpinLock(&MmPipFreeListLock, OldIrql);
    kdp_print("MM ERROR: Failed to allocate contiguous physical pages!\n\r");
    return 0;
}

VOID 
VEAPI 
MmFreeContiguousPhysicalPages(ULONG BasePhysicalAddress, ULONG PageCount)
{
    ULONG CurrentPhys = BasePhysicalAddress;

    // Tinggal di-loop dan kembalikan satu per satu ke Free List
    for (ULONG i = 0; i < PageCount; i++) {
        MmFreePhysicalPage(CurrentPhys);
        CurrentPhys += 4096; // Maju 1 Page (4KB)
    }
}

MM_SYSTEMSIZE
VEAPI
MmQuerySystemSize(VOID)
{
    return MmSystemSize;
}

VOID
VEAPI
MmZeroPageThreadRoutine(PVOID Context)
{
    KIRQL OldIrql;
    ULONG PageIndex;
    ULONG PhysAddress;
    PAGE_ENTRY *Pte = MM_GET_PTE(MmZeroScratchVa);

    while (TRUE) {
        KsAcquireSpinLock(&MmPipFreeListLock, &OldIrql);
        
        if (MmFirstFreePip == 0xFFFFFFFF) {
            // Free list kosong, tidur dulu (nanti dibangunin kalau ada memori di-free)
            KsReleaseSpinLock(&MmPipFreeListLock, OldIrql);

            DPRINT("ZeroPageThread: Sleeping...\n");
            KsWaitForSingleObject(&MmFreePageEvent);
            continue;
        }

        PageIndex = MmFirstFreePip;
        MmpUnlinkFreePage(PageIndex); 
        MmPipDatabase[PageIndex].State = PIP_STATE_ACTIVE;
        
        KsReleaseSpinLock(&MmPipFreeListLock, OldIrql);

        PhysAddress = PageIndex * PAGE_SIZE;
        MMI_WRITE_PTE_VALID(Pte, PhysAddress >> 12);
        MMI_INVALIDATE_TLB(MmZeroScratchVa);

        RtlZeroMemory((PVOID)MmZeroScratchVa, PAGE_SIZE);

        KsAcquireSpinLock(&MmPipFreeListLock, &OldIrql);
        
        MmPipDatabase[PageIndex].State = PIP_STATE_ZEROED;
        MmpInsertZeroedPageHead(PageIndex);
        
        KsReleaseSpinLock(&MmPipFreeListLock, OldIrql);
    }
}

VOID
VEAPI
MmInitializeZeroPageThreadEvent(VOID)
{
    VEASTATUS Status;
    PETHREAD Ethread;

    MmZeroScratchVa = MmAllocateVirtualRange(1);
    if (!MmZeroScratchVa)
    {
        DPRINT("MM: Failed to allocate Zero Page Scratch\n");
        return;
    }

    Ethread = PtCreateSystemThread(
        (PEPROCESS)PtGetCurrentProcess(),
        MmZeroPageThreadRoutine,
        NULL,
        NonPagedPool
    );
    if (!Ethread)
    {
        DPRINT("MM: Failed to create Zero Page Event\n");
        return;
    }
}

BOOLEAN
VEAPI
MiInitializePipBitmap(VOID)
{
    ULONG BitmapBufferSize = ((MmTotalPages + 31) / 32) * sizeof(ULONG);

    PBITMAP_BUFFER BitmapBuffer = (PBITMAP_BUFFER)UlAllocatePoolWithTag(
        NonPagedPool, 
        BitmapBufferSize, 
        TAG_MM
    );

    if (!BitmapBuffer)
    {
        return FALSE;
    }

    RtlInitializeBitmap(&MiPipBitmap, BitmapBuffer, MmTotalPages);
    RtlClearAllBits(&MiPipBitmap);

    for (ULONG i = 0; i < MmTotalPages; i++)
    {
        if (MmPipDatabase[i].State != PIP_STATE_FREE)
        {
            RtlSetBit(&MiPipBitmap, i);
        }
    }

    DPRINT("MM: MiPipBitmap initialized. Total pages: %u, Used bits synced.\n", MmTotalPages);

    return TRUE;
}