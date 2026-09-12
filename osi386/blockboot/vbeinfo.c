#include <osi386.h>

void extract_vbe_info(BLOCK_BOOT_2 *boot_info)
{
    uint16_t es_seg = (VBE_BUFFER_ADDR >> 4) & 0xFFFF;
    uint16_t di_off = VBE_BUFFER_ADDR & 0x000F;

    VBE_INFO_BLOCK *vbe_info = (VBE_INFO_BLOCK*)VBE_BUFFER_ADDR;

    vbe_info->VbeSignature[0] = 'V';
    vbe_info->VbeSignature[1] = 'B';
    vbe_info->VbeSignature[2] = 'E';
    vbe_info->VbeSignature[3] = '2';

    uint32_t status = _INT10_CALL_BUFFER(0x4F00, 0, 0, es_seg, di_off);

    if ((status & 0xFFFF) != 0x004F) {
        bios_print_string("ERROR: Failed to get Framebuffer.\n\r");
        restart_n_message();
        return;
    }

    uint32_t mode_list_ptr = ((vbe_info->VideoModePtr >> 16) * 16) + (vbe_info->VideoModePtr & 0xFFFF);
    uint16_t* modes = (uint16_t*)mode_list_ptr;

    uint32_t MODE_INFO_ADDR = VBE_BUFFER_ADDR + 0x200;
    uint16_t mode_es_seg = (MODE_INFO_ADDR >> 4) & 0xFFFF;
    uint16_t mode_di_off = MODE_INFO_ADDR & 0x000F;
    
    VBE_MODE_INFO* mode_info = (VBE_MODE_INFO*)MODE_INFO_ADDR;

    int i = 0;
    while (modes[i] != 0xFFFF) {
        uint16_t mode = modes[i];

        // Panggil AX = 0x4F01 (Get Mode Info). Nomor mode taruh di CX.
        status = _INT10_CALL_BUFFER(0x4F01, 0, mode, mode_es_seg, mode_di_off);

        if ((status & 0xFFFF) == 0x004F) {
            // Cek apakah LFB disupport (Bit 7 di ModeAttributes)
            if ((mode_info->ModeAttributes & 0x80) && 
                 mode_info->XResolution == 1024 && 
                 mode_info->YResolution == 768 && 
                 mode_info->BitsPerPixel == 32) {
                
                uint32_t fb_size = mode_info->XResolution * mode_info->YResolution * (mode_info->BitsPerPixel / 8);
                uint32_t fb_phys = mode_info->PhysBasePtr;
                uint32_t page_count = (fb_size + 4095) / 4096;

                ULONG_PTR fb_virt = (ULONG_PTR)LmMapIoSpace(fb_phys, fb_size);
                if(!fb_virt)
                {
                    bios_print_string("ERROR: VBE/LM ERROR.\n\r");
                    restart_n_message();
                    return;
                }

                boot_info->VideoBoot.VideoBootAddress = (PVOID)fb_virt;;
                boot_info->VideoBoot.X = 0;
                boot_info->VideoBoot.Y = 0;
                boot_info->VideoBoot.W = mode_info->XResolution;
                boot_info->VideoBoot.H = mode_info->YResolution;
                boot_info->VideoBoot.AdditionalInformation[0] = (void*)(uint32_t)mode_info->BytesPerScanLine; 
                break;
            }
        }
        i++;
    }
}