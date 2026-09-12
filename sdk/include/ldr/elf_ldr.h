#pragma once

#include <stdint.h>
#include <osi386.h>

// Tipe data standar ELF 32-bit
typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t  Elf32_Sword;
typedef uint32_t Elf32_Word;

#define EI_NIDENT 16

// Program Header Types
#define PT_NULL   0
#define PT_LOAD   1   // Tipe Program Header yang wajib di-load ke RAM
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE   4

// Section Header Types (SHT)
#define SHT_NULL     0
#define SHT_PROGBITS 1
#define SHT_SYMTAB   2   // Tabel Simbol
#define SHT_STRTAB   3   // Tabel String
#define SHT_RELA     4
#define SHT_HASH     5
#define SHT_DYNAMIC  6
#define SHT_NOTE     7
#define SHT_NOBITS   8   // Seksi .bss (tanpa payload di disk)
#define SHT_REL      9   // Seksi Relokasi (dipakai di x86 .ko)
#define SHT_SHLIB    10
#define SHT_DYNSYM   11

// Special Section Indices
#define SHN_UNDEF     0      // Symbol tidak terdefinisi (butuh resolved dari Kernel)
#define SHN_LORESERVE 0xFF00
#define SHN_ABS       0xFFF1
#define SHN_COMMON    0xFFF2
#define SHN_HIRESERVE 0xFFFF

// Relocation Types untuk i386 / x86
#define R_386_NONE 0
#define R_386_32   1  /* Absolute: S + A */
#define R_386_PC32 2  /* PC-Relative: S + A - P */

// Macro untuk Ekstraksi Relokasi & Simbol
#define ELF32_R_SYM(val)         ((val) >> 8)
#define ELF32_R_TYPE(val)        ((uint8_t)(val))
#define ELF32_R_INFO(sym, type)  (((sym) << 8) + (uint8_t)(type))

#define ELF32_ST_BIND(val)       ((val) >> 4)
#define ELF32_ST_TYPE(val)       ((val) & 0xF)
#define ELF32_ST_INFO(bind, type)(((bind) << 4) + ((type) & 0xF))

#pragma pack(push, 1)

// 1. ELF Header (Berada di offset 0 dari file)
typedef struct {
    unsigned char e_ident[EI_NIDENT]; // Magic number (0x7F, 'E', 'L', 'F', dll)
    Elf32_Half    e_type;             // Tipe file (misal ET_REL / ET_EXEC)
    Elf32_Half    e_machine;          // Arsitektur (0x03 buat x86/i386)
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;            // Entry point
    Elf32_Off     e_phoff;            // Offset ke tabel Program Header
    Elf32_Off     e_shoff;            // Offset ke tabel Section Header
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;           // Ukuran ELF header ini
    Elf32_Half    e_phentsize;        // Ukuran 1 struct Program Header
    Elf32_Half    e_phnum;            // Jumlah Program Header
    Elf32_Half    e_shentsize;        // Ukuran 1 struct Section Header
    Elf32_Half    e_shnum;            // Jumlah Section Header
    Elf32_Half    e_shstrndx;         // Index section header pembawa string nama seksi
} Elf32_Ehdr;

// 2. Program Header
typedef struct {
    Elf32_Word p_type;   // PT_LOAD (1)
    Elf32_Off  p_offset; // Offset data mentahnya di dalam file
    Elf32_Addr p_vaddr;  // Virtual Address target
    Elf32_Addr p_paddr;  // Physical Address target
    Elf32_Word p_filesz; // Ukuran data di file
    Elf32_Word p_memsz;  // Ukuran di memori (termasuk BSS)
    Elf32_Word p_flags;  // Read/Write/Execute flags
    Elf32_Word p_align;
} Elf32_Phdr;

// 3. Section Header (Informasi tiap seksi seperti .text, .data, .rel.text)
typedef struct {
    Elf32_Word sh_name;      // Index string nama seksi di .shstrtab
    Elf32_Word sh_type;      // SHT_PROGBITS, SHT_SYMTAB, SHT_REL, dll.
    Elf32_Word sh_flags;     // Attribute flags (SHF_WRITE, SHF_ALLOC, dll.)
    Elf32_Addr sh_addr;      // Virtual address seksi saat dimuat
    Elf32_Off  sh_offset;    // Offset data seksi di file
    Elf32_Word sh_size;      // Ukuran seksi dalam byte
    Elf32_Word sh_link;      // Link ke seksi lain (misal index .strtab untuk .symtab)
    Elf32_Word sh_info;      // Info tambahan (misal index seksi target relokasi)
    Elf32_Word sh_addralign; // Alignment seksi
    Elf32_Word sh_entsize;   // Ukuran tiap entry jika seksi berupa tabel (misal sizeof(Elf32_Sym))
} Elf32_Shdr;

// 4. Symbol Table Entry (Struktur tiap item di .symtab)
typedef struct {
    Elf32_Word    st_name;  // Index string nama fungsi/variabel di .strtab
    Elf32_Addr    st_value; // Alamat/offset simbol
    Elf32_Word    st_size;  // Ukuran simbol dalam byte
    unsigned char st_info;  // Type & Binding (ELF32_ST_BIND / ELF32_ST_TYPE)
    unsigned char st_other; // Reserved / Visibility
    Elf32_Half    st_shndx; // Index seksi tempat simbol terdefinisi (SHN_UNDEF jika dari luar)
} Elf32_Sym;

// 5. Relocation Entry tanpa Addend (Digunakan di x86 untuk seksi SHT_REL)
typedef struct {
    Elf32_Addr r_offset; // Offset lokasi memori yang wajib di-patch
    Elf32_Word r_info;   // Gabungan Symbol Index & Relocation Type (pake ELF32_R_SYM & ELF32_R_TYPE)
} Elf32_Rel;

#pragma pack(pop)

int validate_elf(void *elf_buffer);
uint32_t get_elf_entry_point(void *elf_buffer);
int map_elf_to_cr3(void *elf_buffer, PAGE_ENTRY *pd);
void load_cr3_and_enable_paging(PAGE_ENTRY *pd);