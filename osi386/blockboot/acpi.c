#include <osi386.h>

int IsValidRSDP(LPRSDP Rsdp)
{
    uint8_t sum = 0;
    uint8_t *ptr = (uint8_t *)Rsdp;
    int i;
    
    // Checksum RSDP versi 1.0 (20 byte pertama wajib berjumlah 0 kalau di-modulo 256)
    for (i = 0; i < 20; i++) {
        sum += ptr[i];
    }
    return (sum == 0);
}

LPRSDP find_acpi_rsdp(void)
{
    uint16_t* EBDArea = (uint16_t*)0x040E; // Alamat pointer EBDA di BDA (BIOS Data Area)
    uint16_t EBDASeg = *EBDArea;
    uint32_t Offset;
    LPRSDP Rsdp;

    // 1. CARI DI AREA EBDA (Extended BIOS Data Area)
    if(EBDASeg != 0)
    {
        // Konversi Segment 16-bit EBDA ke Physical Address 32-bit (Shift left 4)
        uint32_t ebda_linear_addr = EBDASeg << 4; 

        // Scan 1 KB pertama dari EBDA, dengan kelipatan 16 byte
        for(Offset = 0; Offset < 1024; Offset += 16)
        {
            Rsdp = (LPRSDP)(ebda_linear_addr + Offset);
            
            if(cmemcmp(Rsdp->Signature, "RSD PTR ", 8) == 0) {
                if(IsValidRSDP(Rsdp)) return Rsdp;
            }
        }
    }

    // 2. CARI DI AREA BIOS ROM (0x000E0000 - 0x000FFFFF)
    // Scan dari E0000 sampai FFFFF dengan kelipatan 16 byte
    for(uint32_t Addr = 0x000E0000; Addr < 0x00100000; Addr += 16)
    {
        Rsdp = (LPRSDP)Addr;
        
        if(cmemcmp(Rsdp->Signature, "RSD PTR ", 8) == 0) {
            if(IsValidRSDP(Rsdp)) return Rsdp;
        }
    }

    return NULL; // Gagal nemu ACPI
}