#include <osi386.h>

void extract_memory_map(BLOCK_BOOT_1 *block_boot)
{
    uint32_t ebx = 0;
    int count = 0;
    uint16_t buffer_segment = 0x0000;
    uint16_t buffer_offset = 0x9000;
    PVEA_MEMORY_DESCRIPTOR mmap = (PVEA_MEMORY_DESCRIPTOR)buffer_offset;

    bios_print_string("Extracting Physical Memory Map (E820)...\n\r");

    do {
        // Hitung offset DI untuk entry saat ini (tiap entry ukurannya 24 bytes)
        uint16_t current_di = buffer_offset + (count * sizeof(VEA_MEMORY_DESCRIPTOR));
        
        // Panggil thunk ASM ke mode 16-bit
        int success = _INT15_E820_CALL(&ebx, buffer_segment, current_di);
        
        // Kalau fungsi return 0 (Carry Flag set), berarti list udah habis atau BIOS error
        if (!success) {
            break;
        }

        // Terkadang BIOS ngasih entry sampah dengan panjang 0 byte, abaikan saja
        if (mmap[count].Length == 0) {
            // Kita gak nambahin `count`, tapi EBX tetep jalan ke entry berikutnya
            continue; 
        }

        count++;

        // Batasi maksimal entry biar DI gak overflow dari 0xFFFF (100 entry udah sangat cukup buat PC standar)
        if (count >= 100) {
            break;
        }

    } while (ebx != 0);

    block_boot->MemoryMap = mmap;
    block_boot->MemoryMapCount = count;

    if (count == 0) {
        bios_print_string("ERROR: Failed to get memory map!\n\r");
        restart_n_message();
    }
}