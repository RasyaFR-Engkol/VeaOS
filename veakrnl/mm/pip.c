#include <veakrnl.h>

BOOLEAN 
VEAPI 
MmInitializePip(PBLOCK_BOOT_2 BlockBoot)
{
#define KERNEL_PHYS_BASE 0x1000000              // 16MB (Tempat kernel ditaruh)
#define KERNEL_RESERVE_SIZE (50 * 1024 * 1024)  // 50MB (Area aman untuk kernel)

    ULONG highest_address = 0;
    PVEA_MEMORY_DESCRIPTOR mmap = BlockBoot->MemoryMap;
    ULONG mmap_count = BlockBoot->MemoryMapCount;

    for (ULONG i = 0; i < mmap_count; i++) {
        if (mmap[i].Type == VeaMemoryTypeUsable) {
            ULONG end_addr = mmap[i].BaseAddress + mmap[i].Length;
            if (end_addr > highest_address) {
                highest_address = end_addr;
            }
        }
    }

    MmTotalPages = highest_address / 4096;
    ULONG DatabaseSize = MmTotalPages * sizeof(PIP);

    ULONG kernel_phys_end = (ULONG)&_veaos_end - 0xC0000000;
    kernel_phys_end = (kernel_phys_end + 0xFFF) & ~0xFFF;

    ULONG pip_db_phys = 0;
    for (ULONG i = 0; i < mmap_count; i++) {
        // Cari area Usable yang muat ukuran database PIP dan berada di atas 1MB
        if (mmap[i].Type == VeaMemoryTypeUsable) {
            if (mmap[i].Length >= DatabaseSize) {
                ULONG usable_start = mmap[i].BaseAddress;

                if (usable_start < kernel_phys_end) {
                    usable_start = kernel_phys_end;
                }

                ULONG block_end = mmap[i].BaseAddress + mmap[i].Length;

                if (block_end > usable_start && (block_end - usable_start) >= DatabaseSize) {
                    
                    // Amannnn! Kita taruh PIP DB di sini
                    pip_db_phys = usable_start;
                    
                    // Update map memori: sisa blok yang available sekarang dimulai setelah PIP DB
                    mmap[i].BaseAddress = usable_start + DatabaseSize;
                    mmap[i].Length = block_end - mmap[i].BaseAddress;
                    
                    break;
                }
            }
        }
    }

    if (pip_db_phys == 0) {
        kdp_print("MM ERROR: Not enough memory for PIP database!\n\r");
        return 0;
    }

    MmPipDatabase = (PPIP)(pip_db_phys + 0xC0000000);

    MmMapPip();

    for (ULONG i = 0; i < MmTotalPages; i++) {
        MmPipDatabase[i].State = PIP_STATE_RESERVED;
        MmPipDatabase[i].ReferenceCount = 0;
        MmPipDatabase[i].Flags = 0;
        MmPipDatabase[i].Flink.NextPip = 0xFFFFFFFF;
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
                // Halaman free pertama yang ditemukan jadi kepala list (Head)
                MmFirstFreePip = i;
            } else {
                // Sambungkan halaman free sebelumnya ke indeks halaman ini
                MmPipDatabase[last_free_index].Flink.NextPip = i;
            }
            last_free_index = i;
        }
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
            // Pinjam 1 halaman fisik tepat setelah database PIP selesai
            ULONG new_pt_phys = PhysicalStart + DatabaseSize;
            DatabaseSize += 4096; 

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

/* Fungsi ini untuk memperbaiki rantai linked list */
static
VOID
VEAPI
MmpRebuildFreeList(VOID)
{
    MmFirstFreePip = 0xFFFFFFFF;
    ULONG LastFreeIndex = 0xFFFFFFFF;

    for(ULONG i = 0; i < MmTotalPages; i++)
    {
        if (MmPipDatabase[i].State == PIP_STATE_FREE) {
            if (MmFirstFreePip == 0xFFFFFFFF) {
                MmFirstFreePip = i;
            } else {
                MmPipDatabase[LastFreeIndex].Flink.NextPip = i;
            }
            LastFreeIndex = i;
        }
    }
    if(LastFreeIndex != 0xFFFFFFFF)
    {
        MmPipDatabase[LastFreeIndex].Flink.NextPip = 0xFFFFFFFF;
    }
}

/* ALLOCATOR for PIP */

ULONG 
VEAPI
MmAllocatePhysicalPage(VOID)
{
    if (MmFirstFreePip == 0xFFFFFFFF) {
        kdp_print("MM FATAL: Out of Physical Memory!\n\r");
        return 0; // Return 0 tandanya gagal (OOM)
    }

    ULONG AllocatedIndex = MmFirstFreePip;

    MmFirstFreePip = MmPipDatabase[AllocatedIndex].Flink.NextPip;

    MmPipDatabase[AllocatedIndex].State = PIP_STATE_ACTIVE;
    MmPipDatabase[AllocatedIndex].ReferenceCount = 1;
    MmPipDatabase[AllocatedIndex].PteAddress = 0;

    return (AllocatedIndex * 4096);
}

VOID 
VEAPI 
MmFreePhysicalPage(ULONG PhysicalAddress)
{
    if (PhysicalAddress % 4096 != 0) {
        kdp_print("MM WARNING: Tried to free unaligned physical address!\n\r");
        return;
    }

    ULONG Index = PhysicalAddress / 4096;

    if (Index >= MmTotalPages) {
        kdp_print("MM WARNING: Tried to free invalid physical address!\n\r");
        return;
    }

    if (MmPipDatabase[Index].State != PIP_STATE_ACTIVE) {
        kdp_print("MM WARNING: Tried to free a non-active physical page!\n\r");
        return;
    }

    if (MmPipDatabase[Index].ShareCount > 1) {
        MmPipDatabase[Index].ShareCount--;
        return;   // masih dipegang proses lain, jangan dibebasin
    }

    MmPipDatabase[Index].ShareCount = 0;
    MmPipDatabase[Index].State = PIP_STATE_FREE;
    MmPipDatabase[Index].ReferenceCount = 0;

    MmPipDatabase[Index].Flink.NextPip = MmFirstFreePip;
    MmFirstFreePip = Index;
}

ULONG
VEAPI
MmAllocateContiguousPhysicalPages(ULONG PageCount)
{
    if (PageCount == 0) return 0;

    ULONG contiguous_found = 0;
    ULONG start_index = 0xFFFFFFFF;

    for(ULONG i = 0; i < MmTotalPages; i++)
    {
        if (MmPipDatabase[i].State == PIP_STATE_FREE) {
            if (contiguous_found == 0) {
                start_index = i;
            }
            contiguous_found++;

            // Jika sudah menemukan jumlah halaman berurutan yang diminta
            if (contiguous_found == PageCount) {
                // Tandai semua halaman tersebut menjadi ACTIVE
                for (ULONG j = start_index; j < start_index + PageCount; j++) {
                    MmPipDatabase[j].State = PIP_STATE_ACTIVE;
                    MmPipDatabase[j].ReferenceCount = 1;
                    MmPipDatabase[j].PteAddress = 0;
                }

                // Bangun ulang rantai Free List karena ada halaman di tengah yang dicabut
                MmpRebuildFreeList();

                return (start_index * 4096);
            }
        } else {
            // Reset pencarian jika terputus oleh halaman RESERVED atau ACTIVE
            contiguous_found = 0;
            start_index = 0xFFFFFFFF;
        }
    }

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