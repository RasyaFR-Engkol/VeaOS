#pragma once

#include <stdint.h>

#include <osi386.h>

// Tipe data standar ELF 32-bit biar kodenya rapi
typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t  Elf32_Sword;
typedef uint32_t Elf32_Word;

#define EI_NIDENT 16
#define PT_LOAD   1   // Tipe Program Header yang wajib di-load ke RAM

#pragma pack(push, 1)

// 1. ELF Header (Berada di offset 0 dari file)
typedef struct {
    unsigned char e_ident[EI_NIDENT]; // Magic number (0x7F, 'E', 'L', 'F', dll)
    Elf32_Half    e_type;             // Tipe file (misal Executable)
    Elf32_Half    e_machine;          // Arsitektur (0x03 buat x86/i386)
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;            // INI ENTRY POINT KERNEL LU! (misal 0xC0100000)
    Elf32_Off     e_phoff;            // Offset ke tabel Program Header
    Elf32_Off     e_shoff;            // Offset ke tabel Section Header
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;           // Ukuran ELF header ini
    Elf32_Half    e_phentsize;        // Ukuran 1 struct Program Header
    Elf32_Half    e_phnum;            // JUMLAH Program Header di file ini
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
} Elf32_Ehdr;

// 2. Program Header (Penunjuk lokasi text/data segment)
typedef struct {
    Elf32_Word p_type;   // Cari yang isinya PT_LOAD (1)
    Elf32_Off  p_offset; // Offset data mentahnya di dalam file g_kernel_p
    Elf32_Addr p_vaddr;  // VIRTUAL ADDRESS target (misal 0xC0100000)
    Elf32_Addr p_paddr;  // PHYSICAL ADDRESS target (LMA)
    Elf32_Word p_filesz; // Ukuran data di dalam file
    Elf32_Word p_memsz;  // Ukuran di memori (Bisa lebih gede dari filesz karena .bss)
    Elf32_Word p_flags;  // Read/Write/Execute flags
    Elf32_Word p_align;
} Elf32_Phdr;

#pragma pack(pop)

int validate_elf(void *elf_buffer);
uint32_t get_elf_entry_point(void *elf_buffer);
int map_elf_to_cr3(void *elf_buffer, PAGE_ENTRY *pd);
void load_cr3_and_enable_paging(PAGE_ENTRY *pd);
