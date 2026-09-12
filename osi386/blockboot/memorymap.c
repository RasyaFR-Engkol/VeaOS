#include <osi386.h>

void extract_memory_map(BLOCK_BOOT_2 *block_boot)
{
    // 1. Alokasikan pakai tipe STANDARD, bukan yang EX!
    PVEA_MEMORY_DESCRIPTOR final_mmap = 
        (PVEA_MEMORY_DESCRIPTOR)LmAllocateVirtualPages(0xCF000000, 1);

    if (final_mmap == NULL) {
        bios_print_string("ERROR: Out of memory for Memory Map!\n\r");
        restart_n_message();
    }

    for (int i = 0; i < LmExtendedMapCount; i++) {
        // 2. Hanya copy bagian .Descriptor saja ke array standard
        cmemcpy(&final_mmap[i], &LmExtendedMap[i].Descriptor, sizeof(VEA_MEMORY_DESCRIPTOR));

        // 3. Paksa ubah Type untuk blok yang diokupasi Loader
        if (LmExtendedMap[i].ExtendedType == LoaderOccupied) {
            final_mmap[i].Type = 2; // 2 = Reserved
        }
    }

    // 4. Sekarang lempar ke Kernel dengan AMAN, ukurannya 100% cocok!
    block_boot->MemoryMap = final_mmap; 
    block_boot->MemoryMapCount = LmExtendedMapCount;
}