#include <osi386.h>

#define PAGE_SIZE 0x1000

extern PAGE_ENTRY *g_KernelPageDirectory;
static ULONG g_NextIoSpaceVirt = 0xE0000000;
static ULONG g_NextPoolVirtAddress = 0xD0000000;

ULONG
VEAPI
LmAllocatePhysicalPages(ULONG PageCount)
{
    if (PageCount == 0) return 0;

    ULONGLONG AllocSize = (ULONGLONG)PageCount * PAGE_SIZE;

    for(LONG i = (LONG)LmExtendedMapCount - 1; i >= 0; i--)
    {
        if (LmExtendedMap[i].ExtendedType == LoaderGood &&
            LmExtendedMap[i].Descriptor.Length >= AllocSize &&
            LmExtendedMap[i].Descriptor.BaseAddress >= 0x100000)
        {
            ULONGLONG AllocatedAddr = 
               (LmExtendedMap[i].Descriptor.BaseAddress + LmExtendedMap[i].Descriptor.Length) - AllocSize;

            LmExtendedMap[i].Descriptor.Length -= AllocSize;

            if (LmExtendedMapCount < 128)
            {
                VEA_MEMORY_DESCRIPTOR_EX *NewEntry = &LmExtendedMap[LmExtendedMapCount];
                
                NewEntry->Descriptor.BaseAddress = AllocatedAddr;
                NewEntry->Descriptor.Length = AllocSize;
                NewEntry->Descriptor.Type = 1;
                NewEntry->ExtendedType = LoaderOccupied;
                
                LmExtendedMapCount++;
            }
            else
            {
                bios_print_string("E820 ERROR.\n\r");
                restart_n_message();
            }

            return (ULONG)AllocatedAddr; // Balikin Alamat Fisik
        }
    }

    return 0;
}

PVOID
VEAPI
LmAllocatePagesPreVirt(ULONG PageCount)
{
    ULONG PhysAddr = LmAllocatePhysicalPages(PageCount);
    if (PhysAddr == 0) return NULL;

    // Karena Paging belum aktif, Alamat Fisik = Alamat Virtual
    cmemset((PVOID)PhysAddr, 0, PageCount * PAGE_SIZE);
    return (PVOID)PhysAddr;
}

PVOID
VEAPI
LmAllocatePages(ULONG PageCount)
{
    // 1. Ambil Physical Frame dari E820
    ULONG PhysAddr = LmAllocatePhysicalPages(PageCount);
    if (PhysAddr == 0) return NULL;

    // 2. Tentukan Virtual Address (di sini kita pakai Identity Map: Virt == Phys)
    ULONG VirtAddr = PhysAddr;

    // 3. MAP Physical Address ke Virtual Address di Page Directory aktif!
    LmMapVirtualMemory(g_KernelPageDirectory, VirtAddr, PhysAddr, PageCount);

    // 4. SEKARANG VirtAddr SUDAH MAPPED! Safe buat cmemset dan di-return
    cmemset((PVOID)VirtAddr, 0, PageCount * PAGE_SIZE);

    return (PVOID)VirtAddr;
}

PVOID
VEAPI
LmAllocateVirtualPages(ULONG TargetVirtualAddr, ULONG PageCount)
{
    // 1. Minta RAM Fisik murni
    ULONG PhysAddr = LmAllocatePhysicalPages(PageCount);
    if (PhysAddr == 0) return NULL;

    // 2. Langsung MAP ke alamat Virtual tujuan kita (misal ke Higher Half)
    LmMapVirtualMemory(g_KernelPageDirectory, TargetVirtualAddr, PhysAddr, PageCount);

    // 3. Karena sudah di-map ke TargetVirtualAddr, cmemset ke alamat itu AMAN
    cmemset((PVOID)TargetVirtualAddr, 0, PageCount * PAGE_SIZE);

    return (PVOID)TargetVirtualAddr; // BOOM! Langsung di alamat yang kamu mau.
}

VOID
VEAPI
LmMapVirtualMemory(
    PAGE_ENTRY *PageDirectory,
    ULONG VirtualAddr,
    ULONG PhysicalAddr,
    ULONG PageCount
)
{
    PAGE_ENTRY *VirtualPD = (PAGE_ENTRY*)0xFFFFF000;

    for (ULONG i = 0; i < PageCount; i++)
    {
        ULONG CurrentVirt = VirtualAddr + (i * PAGE_SIZE);
        ULONG CurrentPhys = PhysicalAddr + (i * PAGE_SIZE);

        ULONG PDIndex = PDE_INDEX(CurrentVirt);
        ULONG PTIndex = PTE_INDEX(CurrentVirt);

        if (VirtualPD[PDIndex].bits.present == 0)
        {
            // 1. Minta RAM fisik untuk Page Table baru
            ULONG NewPTPhys = LmAllocatePhysicalPages(1);
            
            // 2. Langsung colok ke Page Directory!
            VirtualPD[PDIndex].bits.present = 1;
            VirtualPD[PDIndex].bits.rw = 1;
            VirtualPD[PDIndex].bits.user = 0;
            VirtualPD[PDIndex].bits.frame = NewPTPhys >> 12;
            
            // 3. BERSENANG-SENANG DENGAN RECURSIVE MAPPING
            // Alamat virtual ajaib untuk Page Table yang baru dicolok:
            PAGE_ENTRY *RecursivePT = (PAGE_ENTRY*)(0xFFC00000 + (PDIndex * PAGE_SIZE));
            
            // cmemset dengan AMAN tanpa #PF
            cmemset(RecursivePT, 0, PAGE_SIZE);
        }

        // Ambil pointer Virtual dari Page Table via Recursive Mapping
        PAGE_ENTRY *PageTable = (PAGE_ENTRY*)(0xFFC00000 + (PDIndex * PAGE_SIZE));

        // Pasang alamat fisiknya ke PT
        PageTable[PTIndex].bits.present = 1;
        PageTable[PTIndex].bits.rw = 1;
        PageTable[PTIndex].bits.user = 0;
        PageTable[PTIndex].bits.frame = CurrentPhys >> 12;

        if (CurrentPhys >= 0xF0000000) {
            PageTable[PTIndex].bits.cache_disable = 1; // Cache Disable
            PageTable[PTIndex].bits.write_through = 1; // Write-Through
        }

        asm volatile("invlpg (%0)" ::"r"(CurrentVirt) : "memory");
    }
}

PVOID
VEAPI
LmMapIoSpace(
    ULONG PhysicalAddress,
    ULONG Size
)
{
    if (Size == 0) return NULL;

    // 1. Hitung offset jika PhysicalAddress tidak sejajar dengan batas Page (4KB)
    ULONG PageOffset = PhysicalAddress & (PAGE_SIZE - 1);
    ULONG AlignedPhys = PhysicalAddress & ~(PAGE_SIZE - 1);

    // 2. Hitung berapa jumlah Page yang dibutuhkan
    ULONG TotalSize = Size + PageOffset;
    ULONG PageCount = (TotalSize + PAGE_SIZE - 1) / PAGE_SIZE;

    // 3. Ambil Virtual Address yang tersedia untuk I/O Space
    ULONG TargetVirt = g_NextIoSpaceVirt;
    
    // Geser pointer I/O Space untuk alokasi berikutnya
    g_NextIoSpaceVirt += (PageCount * PAGE_SIZE);

    // 4. Lakukan Mapping fisik ke virtual pakai fungsi milikmu!
    LmMapVirtualMemory(g_KernelPageDirectory, TargetVirt, AlignedPhys, PageCount);

    // 5. Kembalikan Pointer Virtual + Offset aslinya
    return (PVOID)(TargetVirt + PageOffset);
}

ULONG
VEAPI
LmAllocatePhysicalPagesEx(
    IN BOOT_POOL_TYPE PoolType,
    IN ULONG PageCount
)
{
    if (PageCount == 0) return 0;

    ULONGLONG AllocSize = (ULONGLONG)PageCount * PAGE_SIZE;

    // Cari blok memori LoaderGood dari belakang (Top-Down allocation)
    for (LONG i = (LONG)LmExtendedMapCount - 1; i >= 0; i--)
    {
        if (LmExtendedMap[i].ExtendedType == LoaderGood &&
            LmExtendedMap[i].Descriptor.Length >= AllocSize &&
            LmExtendedMap[i].Descriptor.BaseAddress >= 0x100000) // Di atas 1MB
        {
            // Potong dari batas atas (High Address)
            ULONGLONG AllocatedAddr = 
               (LmExtendedMap[i].Descriptor.BaseAddress + LmExtendedMap[i].Descriptor.Length) - AllocSize;

            // Kurangi ukuran blok bebas asli
            LmExtendedMap[i].Descriptor.Length -= AllocSize;

            // Tambahkan entri baru khusus untuk Pool yang baru dialokasikan
            if (LmExtendedMapCount < 128)
            {
                VEA_MEMORY_DESCRIPTOR_EX *NewEntry = &LmExtendedMap[LmExtendedMapCount];
                
                NewEntry->Descriptor.BaseAddress = AllocatedAddr;
                NewEntry->Descriptor.Length = AllocSize;
                
                // Terjemahkan BOOT_POOL_TYPE ke Extended Memory Descriptor
                switch (PoolType)
                {
                    case LdrReclaimablePool:
                        NewEntry->Descriptor.Type = 1; // Usable RAM di E820
                        NewEntry->ExtendedType = LoaderOldMemory;
                        break;

                    case LdrUnreclaimablePool:
                        NewEntry->Descriptor.Type = 2; // Reserved di mata E820 (Kernel Only)
                        NewEntry->ExtendedType = LoaderOccupied;
                        break;

                    case LdrReservedHardwarePool:
                        NewEntry->Descriptor.Type = 2; // Reserved Hardware
                        NewEntry->ExtendedType = LoaderAcpiNVS;
                        break;
                }
                
                LmExtendedMapCount++;
            }
            else
            {
                bios_print_string("E820 ERROR: Extended Map Overflow!\n\r");
                restart_n_message();
            }

            return (ULONG)AllocatedAddr; // Balikin Alamat Fisik
        }
    }

    return 0; // Out of Memory
}

PVOID
VEAPI
LmAllocatePoolPagesPreVirt(BOOT_POOL_TYPE PoolType, ULONG PageCount)
{
    ULONG PhysAddr = LmAllocatePhysicalPagesEx(PoolType, PageCount);
    if (PhysAddr == 0) return NULL;

    cmemset((PVOID)PhysAddr, 0, PageCount * PAGE_SIZE);
    return (PVOID)PhysAddr;
}

PVOID
VEAPI
LmAllocatePool(BOOT_POOL_TYPE PoolType, ULONG ByteSize)
{
    if (ByteSize == 0) return NULL;

    // 1. Hitung otomatis butuh berapa Page (4KB) dari ByteSize
    ULONG PageCount = (ByteSize + PAGE_SIZE - 1) / PAGE_SIZE;

    // 2. Minta RAM fisik murni dari E820 sesuai PoolType
    ULONG PhysAddr = LmAllocatePhysicalPagesEx(PoolType, PageCount);
    if (PhysAddr == 0) return NULL;

    // 3. Ambil Virtual Address yang sedang tersedia secara otomatis
    ULONG VirtAddr = g_NextPoolVirtAddress;

    // 4. Geser (bump) pointer virtual ke depan untuk alokasi berikutnya
    g_NextPoolVirtAddress += (PageCount * PAGE_SIZE);

    // 5. Map Physical Address ke Virtual Address yang baru dibuat
    LmMapVirtualMemory(g_KernelPageDirectory, VirtAddr, PhysAddr, PageCount);

    // 6. Zeroing memori di alamat Virtual baru
    cmemset((PVOID)VirtAddr, 0, PageCount * PAGE_SIZE);

    // 7. BOOM! Balikin pointer virtual sebersih malloc()
    return (PVOID)VirtAddr; 
}