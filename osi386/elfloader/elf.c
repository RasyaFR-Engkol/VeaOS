#include <stdint.h>
#include <elf_ldr.h>
#include <osi386.h>

int validate_elf(void *elf_buffer)
{
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)elf_buffer;

    // Cek Magic Number ELF: 0x7F, 'E', 'L', 'F'
    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') 
    {
        return -1; // Bukan file ELF beneran!
    }

    // Cek apakah dia tipe Executable (2) dan buat arsitektur x86/i386 (3)
    if (ehdr->e_type != 2 || ehdr->e_machine != 3) 
    {
        return -2; // Salah format arsitektur
    }

    return 0; // ELF Valid!
}

uint32_t get_elf_entry_point(void *elf_buffer)
{
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)elf_buffer;
    return ehdr->e_entry; // Mengembalikan nilai e_entry (misal: 0xC0100000)
}

int map_elf_to_cr3(void *elf_buffer, PAGE_ENTRY *pd)
{
    (void)pd; // Sementara PD gak perlu di-katak-katik karena udah dicover 4MB Page Table kemarin
    
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)elf_buffer;
    
    // Cari lokasi tabel Program Header di dalam file
    Elf32_Phdr *phdr = (Elf32_Phdr *)((uint8_t *)elf_buffer + ehdr->e_phoff);

    // Looping semua Program Header
    for (int i = 0; i < ehdr->e_phnum; i++) 
    {
        // Kita HANYA peduli sama segment yang tipenya PT_LOAD (1)
        if (phdr[i].p_type == PT_LOAD) 
        {
            // Ambil alamat fisik tujuan (LMA). Biasanya di 0x100000 (1MB)
            uint8_t *dest_phys = (uint8_t *)(phdr[i].p_vaddr - 0xC0000000);
            
            // Ambil alamat data mentah di dalam buffer FAT32
            uint8_t *src_file = (uint8_t *)elf_buffer + phdr[i].p_offset;

            // 1. Salin data segment (.text atau .data) ke RAM fisik
            if (phdr[i].p_filesz > 0) 
            {
                cmemcpy(dest_phys, src_file, phdr[i].p_filesz);
            }

            // 2. JEBAKAN `.bss`: Kalau ukuran di memori (memsz) lebih gede dari di file (filesz),
            // sisa memorinya WAJIB di-set ke NOL! Kalau nggak, kernel lu bakal dapet variabel gaib berisi sampah.
            if (phdr[i].p_memsz > phdr[i].p_filesz) 
            {
                uint32_t bss_size = phdr[i].p_memsz - phdr[i].p_filesz;
                cmemset(dest_phys + phdr[i].p_filesz, 0, bss_size);
            }
        }
    }
    return 0;
}