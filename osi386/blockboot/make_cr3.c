#include <osi386.h>

#define MAP_MEGA_BYTES   16  // Kita map 16 MB sekaligus (butuh 4 Page Table)
#define TABLES_COUNT     (MAP_MEGA_BYTES / 4)

PAGE_ENTRY *make_cr3_higher_half(void)
{
    PAGE_ENTRY* page_directory = (PAGE_ENTRY*)PAGE_DIR_ADDR;
    PAGE_ENTRY* page_table_idt = (PAGE_ENTRY*)PAGE_TABLE_IDT_ADDR;
    PAGE_ENTRY* page_table_hhk = (PAGE_ENTRY*)PAGE_TABLE_HHK_ADDR;

    cmemset((void*)PAGE_DIR_ADDR, 0, 4096);
    cmemset((void*)PAGE_TABLE_IDT_ADDR, 0, 4096);
    cmemset((void*)PAGE_TABLE_HHK_ADDR, 0, 4096);

    for (int i = 0; i < 1024; i++) {
        // Identity Map Table
        page_table_idt[i].bits.present = 1;
        page_table_idt[i].bits.rw = 1;         // Boleh dibaca tulis
        page_table_idt[i].bits.user = 0;       // Khusus ring 0 (Kernel/Loader)
        page_table_idt[i].bits.frame = i;      // Frame fisik 0, 1, 2... (Address >> 12)

        // Higher Half Map Table (Isinya sama persis, cuma ditaruh di tempat beda)
        page_table_hhk[i].bits.present = 1;
        page_table_hhk[i].bits.rw = 1;
        page_table_hhk[i].bits.user = 0;
        page_table_hhk[i].bits.frame = i; 
    }

    page_directory[0].bits.present = 1;
    page_directory[0].bits.rw = 1;
    page_directory[0].bits.user = 0;
    page_directory[0].bits.frame = PAGE_TABLE_IDT_ADDR >> 12;

    // Pasang Higher Half (Index 768 mewakili Virtual Address 0xC0000000 - 0xC03FFFFF)
    // Kenapa 768? Karena 0xC0000000 / 4 Megabyte = 768.
    page_directory[768].bits.present = 1;
    page_directory[768].bits.rw = 1;
    page_directory[768].bits.user = 0;
    page_directory[768].bits.frame = PAGE_TABLE_HHK_ADDR >> 12;

    page_directory[1023].bits.present = 1;
    page_directory[1023].bits.rw = 1;
    page_directory[1023].bits.user = 0;
    page_directory[1023].bits.frame = PAGE_DIR_ADDR >> 12;

    return page_directory;
}

BLOCK_BOOT_1 *relocate_block_boot_to_higher_half(BLOCK_BOOT_1 *boot_info, PAGE_ENTRY *pd)
{
    uint32_t lfb_phys = (uint32_t)boot_info->VideoBoot.VideoBootAddress;
    uint32_t lfb_size = boot_info->VideoBoot.W * boot_info->VideoBoot.H * 4;
    uint32_t num_pages = (lfb_size + 4095) / 4096;

    PAGE_ENTRY *lfb_pt = (PAGE_ENTRY*)PAGE_TABLE_LFB_ADDR;
    cmemset(lfb_pt, 0, 4096);

    for (uint32_t i = 0; i < num_pages; i++) {
        lfb_pt[i].bits.present = 1;
        lfb_pt[i].bits.rw = 1;
        lfb_pt[i].bits.user = 0;
        // Alamat fisik digeser 12 bit kanan buat dapet Frame
        lfb_pt[i].bits.frame = (lfb_phys / 4096) + i;
    }

    uint32_t pd_idx = LFB_VIRTUAL_ADDR >> 22;
    pd[pd_idx].bits.present = 1;
    pd[pd_idx].bits.rw = 1;
    pd[pd_idx].bits.user = 0;
    pd[pd_idx].bits.frame = PAGE_TABLE_LFB_ADDR >> 12;

    boot_info->VideoBoot.VideoBootAddress = (void*)LFB_VIRTUAL_ADDR;
    if (boot_info->AcpiTable != NULL) {
        boot_info->AcpiTable = (void*)((uint32_t)boot_info->AcpiTable + 0xC0000000);
    }

    BLOCK_BOOT_1 *virtual_boot_info = (BLOCK_BOOT_1*)((uint32_t)boot_info + 0xC0000000);

    return virtual_boot_info;
}

void load_cr3_and_enable_paging(PAGE_ENTRY *pd)
{
    asm volatile (
        "mov %0, %%cr3\n\t"         // 1. Masukin alamat Page Directory ke CR3
        "mov %%cr0, %%eax\n\t"       // 2. Ambil nilai CR0 saat ini
        "or $0x80000000, %%eax\n\t"  // 3. Set Bit 31 (PG / Paging) jadi 1
        "mov %%eax, %%cr0\n\t"       // 4. Tembak balik ke CR0! Paging AKTIF!
        :
        : "r"(pd)                    // %0 merepresentasikan variabel pd
        : "eax"                      // Kasih tau compiler kita minjem register EAX
    );
}