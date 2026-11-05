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
    return ehdr->e_entry; 
}

int map_elf_to_cr3(void *elf_buffer, PAGE_ENTRY *pd)
{
    (void)pd;
    
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)elf_buffer;
    Elf32_Phdr *phdr = (Elf32_Phdr *)((uint8_t *)elf_buffer + ehdr->e_phoff);

    uint32_t phys_file_base = 0;
    for (int i = 0; i < ehdr->e_phnum; i++) 
    {
        if (phdr[i].p_type == PT_LOAD) 
        {
            phys_file_base = (phdr[i].p_vaddr - 0xC0000000) - phdr[i].p_offset;
            break;
        }
    }

    for (int i = 0; i < ehdr->e_phnum; i++) 
    {
        if (phdr[i].p_type == PT_LOAD) 
        {
            uint8_t *dest_phys = (uint8_t *)(phdr[i].p_vaddr - 0xC0000000);
            uint8_t *src_file = (uint8_t *)elf_buffer + phdr[i].p_offset;

            if (phdr[i].p_filesz > 0) 
            {
                cmemcpy(dest_phys, src_file, phdr[i].p_filesz);
            }

            if (phdr[i].p_memsz > phdr[i].p_filesz) 
            {
                uint32_t bss_size = phdr[i].p_memsz - phdr[i].p_filesz;
                cmemset(dest_phys + phdr[i].p_filesz, 0, bss_size);
            }
        }
    }

    cmemcpy((void *)phys_file_base, elf_buffer, sizeof(Elf32_Ehdr));

    // 3. Copy Section Header Table ke Offset e_shoff
    if (ehdr->e_shoff != 0 && ehdr->e_shnum > 0)
    {
        cmemcpy((void *)(phys_file_base + ehdr->e_shoff),
                (uint8_t *)elf_buffer + ehdr->e_shoff,
                ehdr->e_shentsize * ehdr->e_shnum);
    }

    Elf32_Shdr *shdr = (Elf32_Shdr *)((uint8_t *)elf_buffer + ehdr->e_shoff);
    for (int i = 0; i < ehdr->e_shnum; i++)
    {
        if (shdr[i].sh_type == SHT_SYMTAB || 
            shdr[i].sh_type == SHT_STRTAB)
        {
            if (shdr[i].sh_offset != 0 && shdr[i].sh_size > 0)
            {
                cmemcpy((void *)(phys_file_base + shdr[i].sh_offset),
                        (uint8_t *)elf_buffer + shdr[i].sh_offset,
                        shdr[i].sh_size);
            }
        }
    }

    return 0;
}