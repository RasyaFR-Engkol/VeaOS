#include <osi386.h>
#include <fat32intrnl.h>
#include <elf_ldr.h>

void LdrPrepareBlockBootAndBoot(
    IN PAGE_ENTRY *Pde,
    IN PCHAR StringOption,
    IN BOOLEAN Chainload
);
BOOLEAN vbe_active = 0;
PAGE_ENTRY *LmPde;

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

BOOLEAN
VEAPI
LdrLoadVeakrnl(OUT PUCHAR *BufferToKernel)
{
    VFS_FILE_INFO FileInfo;
    cmemset(&FileInfo, 0, sizeof(FileInfo));

    if(FsLookupFileInformation("/veakrnl.elf", &FileInfo) < 0)
    {
        LdrError(STATUS_FILE_NOT_FOUND);
    }
    
    PVOID Buffer = LmAllocatePool(LdrUnreclaimablePool, FileInfo.FileSize);
    if(!Buffer)
    {
        LdrError(STATUS_INSUFFICIENT_RESOURCES);
    }

    ULONG BytesReaded = 0;
    BytesReaded = FsReadFile(&FileInfo, Buffer);
    if(BytesReaded == 0)
    {
        LdrError(STATUS_READ_DISK_ERROR);
    }

    *BufferToKernel = (PUCHAR)Buffer;
    return TRUE;
}

VOID
VEAPI
LdrSelectionMenu(VOID);

VOID
VEAPI
LdrAdvancedMode(VOID)
{
    BvRerenderLayout();
    LdrInfo("Now you're in advanced mode. Now go fuck yourself.\n");
    LdrInfo("Or, Preff F8 again to go back to Selection Menu.\n");
    while(1)
    {
        ULONG KeyCode = GetKeycode();
        if(KeyCode == KEY_F8)
        {
            LdrSelectionMenu();
        }
    }
}

VOID
VEAPI
LdrSelectionMenu(VOID)
{
    BvRerenderLayout();

    VEA_FOOTER_KEY MenuKeys[] = {
        { "ENTER=Choose" },
        { "UP/DOWN=Choose Option" },
        {"F8=Advanced Mode"}
    };
    BvSetFooterKeyCombination(MenuKeys, sizeof(MenuKeys) / sizeof(MenuKeys[0]));

    BvPrintLog("Please choose available OS based on this entry:\n");

    static const OSI386_BOOT_OPTION BootOptions[] = 
    {
        { "VeaOS 32 bit",         "",             FALSE },
        { "VeaOS 32 bit (DEBUG)", "/DEBUG", FALSE },
        { "Ubuntu Linux (GRUB)",  "/boot/grub/grub.cfg",              TRUE  }
    };
    ULONG SizeOfList = sizeof(BootOptions) / sizeof(BootOptions[0]);
    PCHAR DisplayNames[SizeOfList];
    for (ULONG i = 0; i < SizeOfList; i++)
    {
        DisplayNames[i] = BootOptions[i].BootName;
    }
    PBV_LIST_OPTION ListOption = BvCreateListOption(DisplayNames, SizeOfList, 400);
    ULONG CurrentIndex = 0;

    BvDownListOption(ListOption, 0, 0);

    BvPrintLog("Or press F8 for advanced mode.\n");

    while(TRUE)
    {
        USHORT KeyCode = GetKeycode();
        
        switch (KeyCode)
        {
            case KEY_UP:
            {
                if(CurrentIndex == 0) 
                {
                    break;
                }

                ULONG OldIndex = CurrentIndex;
                CurrentIndex--;

                BvUpListOption(ListOption, OldIndex, CurrentIndex);
                break;
            }
            case KEY_DOWN:
            {
                if(CurrentIndex >= SizeOfList - 1) 
                {
                    break;
                }

                ULONG OldIndex = CurrentIndex;
                CurrentIndex++;

                BvDownListOption(ListOption, OldIndex, CurrentIndex);
                break;
            }
            case KEY_ENTER:
            {
                goto BootNow;
            }
            case KEY_F8:
            {
                goto AdvancedMode;
            }
            default:
            {
                break;
            }
        }
    }

BootNow:
    {
        CHAR BootArguments[128];
        BOOLEAN IsBootChainload = BootOptions[CurrentIndex].Chainloading;

        cmemset(BootArguments, 0, 128);
        cstrcpy(BootArguments, BootOptions[CurrentIndex].ArgumentLineOptions);

        LdrPrepareBlockBootAndBoot(LmPde, BootArguments, IsBootChainload);

        if(IsBootChainload)
        {
            LdrError(STATUS_BOOTING_CHAINLOADING_FAILURE);
        }
        else
        {
            LdrError(STATUS_BOOTING_FAILED);
        }
    }

AdvancedMode: 
    {
        LdrAdvancedMode();
    }
}

void main(void)
{
    PAGE_ENTRY *Pde = LmInitSystem();
    if(!Pde)
    {
        LdrError(STATUS_LOADER_MEMORY_FAILURE);
        while(1) asm("hlt");
    }

    LmPde = Pde;
    LdrInfo("HELLO FROM OSI386\n");

    FsInitialize();
    BvInitScreen();
    LdrSelectionMenu();
}

void LdrPrepareBlockBootAndBoot(
    IN PAGE_ENTRY *Pde,
    IN PCHAR StringOption,
    IN BOOLEAN Chainload
)
{
    if(Chainload)
    {
        // TODO: Chainload
        return;
    }

    BLOCK_BOOT_2 *block_boot = (BLOCK_BOOT_2*)LmAllocateVirtualPages(0xC0090000, 1);

    if (block_boot == NULL)
    {
        LdrError(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // cari video buffer
    //
    BvMoveVideoInfoToBootBlock(block_boot);

    //
    // cari VeaKey SYSTEM
    //
    LdrLoadSystemHive(block_boot);

    //
    // cari ACPI
    //
    extern void* AcpiMapRsdpPointer(LPRSDP physical_rsdp);
    static ACPI_RSDP_EXTENDED g_WritableRsdp;

    LPRSDP acpi_rsdp = find_acpi_rsdp();
    if(acpi_rsdp != NULL) {
        // 1. Copy RSDP dari BIOS ROM (Read-Only) ke RAM Writable kita
        cmemcpy(&g_WritableRsdp, acpi_rsdp, sizeof(ACPI_RSDP_EXTENDED));
        
        // 2. Set BootBlock nunjuk ke RSDP di RAM kita
        block_boot->AcpiTable = &g_WritableRsdp;
        block_boot->AcpiTableByteSize = 0xFFFF;

        // 3. SEKARANG BISA DI-OVERWRITE DENGAN AMAN!
        AcpiMapExtendedPointer((LPRSDP)block_boot->AcpiTable);
    } else {
        block_boot->AcpiTable = NULL;
        block_boot->AcpiTableByteSize = 0;
    }

    //
    // Create first Kernel Initial Thread and
    // process
    //
    if(!LdrCreateInitialThreadAndProcess(block_boot))
    {
        LdrError(STATUS_INSUFFICIENT_RESOURCES);
    }

    // 
    // Get information to SMBIOS
    //
    if(!LdrGetSmbiosInformation(block_boot))
    {
        LdrWarning("Can't get SMBIOS Information.\n\r");
    }

    //
    // Create FS segment base (Note: we need to set Thread and Process from BlockBoot
    // to PRCB inside kernel)
    //
    LdrBuildProcessorControlBlock(block_boot);

    //
    // Should we extract our disk information?
    //
    LdrExtractDiskInformation(block_boot);

    //
    // Copy our StringOption booting, But we need to allocate them first.
    // It's fine to be permanent
    //
    ULONG StringLength = cstrlen(StringOption);
    block_boot->VeaBootArgument = (PSTR)LmAllocatePool(LdrUnreclaimablePool, StringLength + 1);
    if(!block_boot->VeaBootArgument)
    {
        LdrError(STATUS_INSUFFICIENT_RESOURCES);
    }

    cmemset(block_boot->VeaBootArgument, 0, StringLength + 1);
    cstrcpy(block_boot->VeaBootArgument, StringOption);

    //
    // Read the veakrnl ELF File
    //
    PUCHAR BufferToVeaKrnl = NULL;
    if(!LdrLoadVeakrnl(&BufferToVeaKrnl))
    {
        LdrError(STATUS_FILE_NOT_FOUND);
    }

    /* Save it to global so we can resolve BootDriver */
    LdrBufferToKernel = BufferToVeaKrnl;
    
    //
    // Parse VeaKey to find each import resolve
    //
    LdrLoadBootDriver(block_boot);

    //
    // Extract memory map
    //
    extract_memory_map(block_boot);

    //
    // We need to save our pointer, load our boot driver here
    // TODO
    //
    /* if(!LdrLoadBootDriver(block_boot))
       {
        LdrError(STATUS_INSUFFICIENT_RESOURCES);
       }
    */
    
    //
    // eksekusi elf 
    //
    if(validate_elf(BufferToVeaKrnl) != 0)
    {
        LdrError(STATUS_FILE_EXECUTABLE_INVALID);
    }

    if(map_elf_to_cr3(BufferToVeaKrnl, Pde) < 0)
    {
        bios_print_string("ERROR: Failed to map ELF to memory!\n\r");
        restart_n_message();
    }

    uint32_t entry_point_addr = get_elf_entry_point(BufferToVeaKrnl);
    if(entry_point_addr == 0)
    {
        bios_print_string("ERROR: Entry point not found!\n\r");
        restart_n_message();
    }

    typedef void (*KernelMain)(BLOCK_BOOT_2*);
    KernelMain kernel_entry = (KernelMain)entry_point_addr;

    //
    // Loncat ke kernel (say bye)
    //
    BvClearScreen();
    kernel_entry(block_boot);

    while(1)
    {
        asm("cli");
        asm("hlt");
    }
}