#include <osi386.h>
#include <fat32intrnl.h>
#include <elf_ldr.h>

char *g_kernel_p = NULL;
void prepare_boot_block_and_boot(void);
_Bool vbe_active = 0;

void restart_n_message(void)
{
    if(vbe_active == 0)
    {
        volatile char *vga = (char*)0xB8000;
        char *string = "ERROR";
        int i = 0;

        while (string[i] != '\0') {
            vga[i * 2] = string[i];     // Character byte
            vga[(i * 2) + 1] = 0x7F;   // Attribute byte (Light Gray on Light Cyan)
            i++;
        }
        

        bios_print_string("If you are a technician, please reformat your disk or"
                                " change your disk to a better one.\r\n");
        bios_print_string("Press any key to restart.\n\r");
    }

    wait_for_keypress();

    force_reboot();
}

void printA(int* i)
{
    volatile char *vga = (char*)0xB8000;
    
    // Gunakan perkalian 2 agar melompat per 2 byte (Cell karakter utuh)
    vga[(*i) * 2] = 'A';
    vga[((*i) * 2) + 1] = 0xF;
    
    // Increment index selnya
    (*i)++;
}

void main(void)
{
    // debugging purpose
    volatile char *vga = (char*)0xB8000;
    int i = 0;
    printA(&i);

    bios_print_char('O');
    bios_print_char('K');
    bios_print_char('!');

    bios_print_string("well.\n\r");
    printA(&i);

    mbr_detect();
    printA(&i);

    //
    // inisialisasi FAT32
    //
    if(fat32_init() < 0)
    {
        bios_print_string("ERROR: FAT32 is not available, or the structure is broken"
                                " and corrupted\r\n");
        restart_n_message();
        
    }
    else
    {
        bios_print_string("MODE: FAT32\r\n");
        printA(&i);
    }

    //
    // find the kernel
    //
    
    FAT32_DirEntry kernel_entry;

    if(fat32_find_entry(root_cluster, "veakrnl.elf", &kernel_entry) < 0)
    {
        bios_print_string("ERROR: No operating system found in your storage system.\n\r");
        restart_n_message();
    }
    
    char *kernel_p = (char*)KERNEL_SAFE_PLACE;
    unsigned long bytes_read;

    cmemset(kernel_p, 0, kernel_entry.size);

    bytes_read = fat32_read_file(&kernel_entry, (unsigned char*)kernel_p);
    if(bytes_read == 0)
    {
        bios_print_string("ERROR: Disk read error.\n\r");
        restart_n_message();
    }

    g_kernel_p = kernel_p;

    prepare_boot_block_and_boot();
}

void prepare_boot_block_and_boot(void)
{
    BLOCK_BOOT_1 *block_boot = (BLOCK_BOOT_1*)BLOCK_BOOT_1_ADDR;

    //
    // cari video buffer
    //
    extract_vbe_info(block_boot);

    LPRSDP acpi_rsdp = find_acpi_rsdp();
    if(acpi_rsdp != NULL) {
        block_boot->AcpiTable = (void*)acpi_rsdp;
        block_boot->AcpiTableByteSize = 0xFFFF;
    } else {
        block_boot->AcpiTable = NULL;
        block_boot->AcpiTableByteSize = 0;
    }

    extract_memory_map(block_boot);

    // eksekusi buat cr3 disini harusnya
    PAGE_ENTRY *page_entry;
    BLOCK_BOOT_1 *v_block_boot = NULL;

    page_entry = make_cr3_higher_half();
    if(page_entry == NULL)
    {
        bios_print_string("ERROR: Failed to make new CR3\n\r");
        restart_n_message();
    }
    else
    {
        v_block_boot = relocate_block_boot_to_higher_half(block_boot, page_entry);
    }
    
    // eksekusi elf disini harusnya

    if(validate_elf(g_kernel_p) != 0)
    {
        bios_print_string("ERROR: Invalid Kernel ELF!\n\r");
        restart_n_message();
    }

    if(map_elf_to_cr3(g_kernel_p, page_entry) < 0)
    {
        bios_print_string("ERROR: Failed to map ELF to memory!\n\r");
        restart_n_message();
    }

    uint32_t entry_point_addr = get_elf_entry_point(g_kernel_p);
    if(entry_point_addr == 0)
    {
        bios_print_string("ERROR: Entry point not found!\n\r");
        restart_n_message();
    }

    typedef void (*KernelMain)(BLOCK_BOOT_1*);
    KernelMain kernel_entry = (KernelMain)entry_point_addr;

    load_cr3_and_enable_paging(page_entry);

    kernel_entry(v_block_boot);

    while(1)
    {
        asm("cli");
        asm("hlt");
    }
}