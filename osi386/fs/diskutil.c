#include <osi386.h>
#include <stdint.h>

void LBA_READ_TO_BUFFER(uint32_t lba, void* buffer, uint32_t size)
{
    static DiskAddressPacket dap;
    
    // Ubah pointer 32-bit menjadi angka integer
    uint32_t linear_addr = (uint32_t)buffer;
    
    // Konversi ke Segment:Offset Real Mode buat BIOS
    uint16_t real_mode_segment = (uint16_t)((linear_addr >> 4) & 0xFFFF);
    uint16_t real_mode_offset  = (uint16_t)(linear_addr & 0x000F);

    dap.size = 16;           // Standar BIOS
    dap.reserved = 0;
    dap.sectors = (size + 511) / 512;
    dap.buffer_offset = real_mode_offset;  
    dap.buffer_segment = real_mode_segment;
    dap.lba_lower = lba;     // LBA target
    dap.lba_upper = 0;       // Atas kosongin

    // 0x80 = Hard Drive 0 (Ubah kalau lu boot dari USB/Floppy)
    _LBA_READ(0x80, &dap);
}