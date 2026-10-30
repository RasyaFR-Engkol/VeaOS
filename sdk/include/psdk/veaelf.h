// VEA ELF

#pragma once

#include <procbind.h>
#include <veastatus.h>

#define EI_NIDENT       16

//
// E_IDENT Indices
//
#define EI_MAG0         0
#define EI_MAG1         1
#define EI_MAG2         2
#define EI_MAG3         3
#define EI_CLASS        4
#define EI_DATA         5
#define EI_VERSION      6
#define EI_OSABI        7

//
// ELF Magic Bytes
//
#define ELFMAG0         0x7F
#define ELFMAG1         'E'
#define ELFMAG2         'L'
#define ELFMAG3         'F'

//
// ELF Architecture Class
//
#define ELFCLASSNONE    0
#define ELFCLASS32      1
#define ELFCLASS64      2

//
// ELF Endianness
//
#define ELFDATANONE     0
#define ELFDATA2LSB     1 // Little Endian (x86 / x86_64)
#define ELFDATA2MSB     2 // Big Endian

//
// ELF File Type
//
#define ET_NONE         0
#define ET_REL          1 // Relocatable object file
#define ET_EXEC         2 // Executable
#define ET_DYN         3 // Shared object / PIE Executable
#define ET_CORE         4 // Core dump

//
// ELF Machine Type
//
#define EM_386          3  // Intel 80386
#define EM_X86_64       62 // AMD x86-64

//
// ELF Program Header Type
//
#define PT_NULL         0
#define PT_LOAD         1 // Region memori yang bakal di-map ke RAM (Code/Data)
#define PT_DYNAMIC      2 // Dynamic linking info
#define PT_INTERP       3 // Path ke ELF interpreter/linker (misal /system/lib/ld.so)
#define PT_NOTE         4 // Aux info
#define PT_SHDR         5
#define PT_PHDR         6 // Location of Program Header Table itself
#define PT_TLS          7 // Thread Local Storage template

//
// ELF Program Header Flags
//
#define PF_X            0x1 // Executable
#define PF_W            0x2 // Writable
#define PF_R            0x4 // Readable

//
// Section Header Type
//
#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_RELA        4
#define SHT_HASH        5
#define SHT_DYNAMIC     6
#define SHT_NOBITS      8 // BSS section (uninitialized data)
#define SHT_REL         9

#pragma pack(push, 1)

typedef struct _ELF32_EHDR {
    UCHAR  e_ident[EI_NIDENT];
    USHORT e_type;
    USHORT e_machine;
    ULONG  e_version;
    ULONG  e_entry;       // Virtual address entry point OS
    ULONG  e_phoff;       // Offset ke Program Header Table
    ULONG  e_shoff;       // Offset ke Section Header Table
    ULONG  e_flags;
    USHORT e_ehsize;      // Ukuran ELF Header ini
    USHORT e_phentsize;   // Ukuran 1 entri Program Header
    USHORT e_phnum;       // Jumlah entri Program Header
    USHORT e_shentsize;   // Ukuran 1 entri Section Header
    USHORT e_shnum;       // Jumlah entri Section Header
    USHORT e_shstrndx;    // Index section header yang menyimpan string tabel nama section
} ELF32_EHDR, *PELF32_EHDR;

typedef struct _ELF32_PHDR {
    ULONG p_type;   // PT_LOAD, PT_DYNAMIC, dll.
    ULONG p_offset; // Offset byte di dalam file
    ULONG p_vaddr;  // Virtual address target di RAM
    ULONG p_paddr;  // Physical address (diabaikan di OS modern)
    ULONG p_filesz; // Ukuran data asli di file
    ULONG p_memsz;  // Ukuran di RAM (kalau memsz > filesz -> BSS/Zero fill)
    ULONG p_flags;  // PF_R | PF_W | PF_X
    ULONG p_align;  // Alignment (misal 4096 / 0x1000)
} ELF32_PHDR, *PELF32_PHDR;

typedef struct _ELF32_SHDR {
    ULONG sh_name;      // Index string nama section di .shstrtab
    ULONG sh_type;
    ULONG sh_flags;
    ULONG sh_addr;      // Virtual address jika diallocated
    ULONG sh_offset;    // Offset di file
    ULONG sh_size;
    ULONG sh_link;
    ULONG sh_info;
    ULONG sh_addralign;
    ULONG sh_entsize;
} ELF32_SHDR, *PELF32_SHDR;

typedef struct _ELF32_REL {
    ULONG r_offset; // Offset lokasi yang mau di-patch
    ULONG r_info;   // Symbol index (bits 8..31) + Relocation type (bits 0..7)
} ELF32_REL, *PELF32_REL;

typedef struct _ELF32_SYM {
    ULONG st_name;  // Index nama di string table
    ULONG st_value; // Offset / Virtual Address
    ULONG st_size;  // Size dari simbol (misal ukuran fungsi/variabel)
    UCHAR st_info;  // Type dan Binding attributes
    UCHAR st_other;
    USHORT st_shndx;// Section Index (0 = SHN_UNDEF)
} ELF32_SYM, *PELF32_SYM;

#pragma pack(pop)

#define ELF32_R_SYM(i)    ((i) >> 8)
#define ELF32_R_TYPE(i)   ((UCHAR)(i))

#define R_386_32   1
#define R_386_PC32 2

typedef ULONG_PTR (VEAPI *PKERNEL_SYMBOL_RESOLVER)(IN PCSTR SymbolName);

BOOLEAN
VEAPI
RtlValidateElfHeader(
    IN PVOID Buffer,
    IN SIZE_T BufferSize
);

ULONG
VEAPI
RtlElfFlagsToVeaProtection(
    IN ULONG PfFlags
);

VEASTATUS
VEAPI
RtlCalculateElfImageBounds(
    IN PELF32_EHDR Header,
    OUT PVOID *BaseAddress,
    OUT PSIZE_T ImageSize
);

PELF32_PHDR
VEAPI
RtlGetElfProgramHeader(
    IN PELF32_EHDR Header,
    IN USHORT Index
);

PELF32_PHDR
VEAPI
RtlFindElfProgramHeaderByType(
    IN PELF32_EHDR Header,
    IN ULONG Type
);

ULONG_PTR
VEAPI
RtlGetElfEntryPoint(
    IN PELF32_EHDR Header
);

PELF32_SHDR
VEAPI
RtlFindElfSectionByName(
    IN PELF32_EHDR Header,
    IN PSTR SectionName
);

PELF32_SYM
VEAPI
RtlFindElfSymbolByName(
    IN PELF32_EHDR Header,
    IN PCSTR SymbolName
);

VEASTATUS
VEAPI
RtlApplyElfRelocations(
    IN PELF32_EHDR Header,
    IN ULONG_PTR ImageBase,
    IN PKERNEL_SYMBOL_RESOLVER KernelResolver
);

/* EOF */