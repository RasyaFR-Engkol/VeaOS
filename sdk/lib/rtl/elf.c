/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : elf.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : ELF RTL 
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "mm.h"
#include "mmtype.h"
#include <rtl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 30-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file elf.c
 * --------------------------------------------------------------------- */

BOOLEAN
VEAPI
RtlValidateElfHeader(
    IN PVOID Buffer,
    IN SIZE_T BufferSize
)
{
    PELF32_EHDR Header = (PELF32_EHDR)Buffer;

    if (!Buffer || BufferSize < sizeof(ELF32_EHDR))
    {
        return FALSE;
    }

    if (Header->e_ident[EI_MAG0] != ELFMAG0 ||
        Header->e_ident[EI_MAG1] != ELFMAG1 ||
        Header->e_ident[EI_MAG2] != ELFMAG2 ||
        Header->e_ident[EI_MAG3] != ELFMAG3)
    {
        return FALSE;
    }

    if (Header->e_ident[EI_CLASS] != ELFCLASS32 ||
        Header->e_ident[EI_DATA]  != ELFDATA2LSB)
    {
        return FALSE;
    }

    if (Header->e_phoff == 0 || Header->e_phnum == 0)
    {
        return FALSE;
    }

    if ((Header->e_phoff + (Header->e_phnum * Header->e_phentsize)) > BufferSize)
    {
        return FALSE;
    }

    return TRUE;
}

ULONG 
VEAPI
RtlElfFlagsToVeaProtection(
    IN ULONG PfFlags
)
{
    if (PfFlags == 0)
    {
        return PAGE_NOACCESS;
    }

    if ((PfFlags & PF_X) && (PfFlags & PF_W))
    {
        return PAGE_EXECUTE_READWRITE;
    }

    if (PfFlags & PF_X)
    {
        return PAGE_EXECUTE_READ;
    }

    if (PfFlags & PF_W)
    {
        return PAGE_READWRITE;
    }

    if (PfFlags & PF_R)
    {
        return PAGE_READONLY;
    }

    return PAGE_READONLY;
}

VEASTATUS
VEAPI
RtlCalculateElfImageBounds(
    IN PELF32_EHDR Header,
    OUT PVOID *BaseAddress,
    OUT PSIZE_T ImageSize
)
{
    ULONG_PTR MinVa = (ULONG_PTR)-1; 
    ULONG_PTR MaxVa = 0;
    BOOLEAN FoundLoadable = FALSE;

    if (!Header || !BaseAddress || !ImageSize)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PELF32_PHDR PhdrTable = (PELF32_PHDR)((ULONG_PTR)Header + Header->e_phoff);

    for (USHORT i = 0; i < Header->e_phnum; i++)
    {
        PELF32_PHDR Phdr = &PhdrTable[i];

        // Cuma segmen PT_LOAD yang di-map ke RAM
        if (Phdr->p_type == PT_LOAD)
        {
            if (Phdr->p_memsz == 0)
            {
                continue; // Skip segmen kosong
            }

            ULONG_PTR SegStart = Phdr->p_vaddr;
            ULONG_PTR SegEnd   = Phdr->p_vaddr + Phdr->p_memsz;

            if (SegStart < MinVa) MinVa = SegStart;
            if (SegEnd > MaxVa)   MaxVa = SegEnd;

            FoundLoadable = TRUE;
        }
    }

    if (!FoundLoadable)
    {
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    // Align batas terluar ke boundary 4KB (PAGE_SIZE)
    ULONG_PTR AlignedMinVa = PAGE_ALIGN_DOWN(MinVa);
    ULONG_PTR AlignedMaxVa = PAGE_ALIGN_UP(MaxVa);

    *BaseAddress = (PVOID)AlignedMinVa;
    *ImageSize   = (SIZE_T)(AlignedMaxVa - AlignedMinVa);

    return STATUS_SUCCESS;
}

PELF32_PHDR
VEAPI
RtlGetElfProgramHeader(
    IN PELF32_EHDR Header,
    IN USHORT Index
)
{
    if (!Header || Index >= Header->e_phnum)
    {
        return NULL;
    }

    ULONG_PTR Address = (ULONG_PTR)Header + Header->e_phoff + (Index * Header->e_phentsize);
    return (PELF32_PHDR)Address;
}

PELF32_PHDR
VEAPI
RtlFindElfProgramHeaderByType(
    IN PELF32_EHDR Header,
    IN ULONG Type
)
{
    if (!Header) return NULL;

    for (USHORT i = 0; i < Header->e_phnum; i++)
    {
        PELF32_PHDR Phdr = RtlGetElfProgramHeader(Header, i);
        if (Phdr && Phdr->p_type == Type)
        {
            return Phdr;
        }
    }

    return NULL;
}

ULONG_PTR
VEAPI
RtlGetElfEntryPoint(
    IN PELF32_EHDR Header
)
{
    if (!Header) return 0;
    return (ULONG_PTR)Header->e_entry;
}

PELF32_SHDR
VEAPI
RtlFindElfSectionByName(
    IN PELF32_EHDR Header,
    IN PSTR SectionName
)
{
    if (!Header || !SectionName || Header->e_shoff == 0 || Header->e_shnum == 0)
    {
        return NULL;
    }

    if (Header->e_shstrndx >= Header->e_shnum)
    {
        return NULL;
    }

    ULONG_PTR ShdrBase = (ULONG_PTR)Header + Header->e_shoff;
    
    // Ambil Section Header milik String Table (.shstrtab)
    PELF32_SHDR StrTabShdr = (PELF32_SHDR)(ShdrBase + (Header->e_shstrndx * Header->e_shentsize));
    PCSTR StrTab = (PCSTR)((ULONG_PTR)Header + StrTabShdr->sh_offset);

    for (USHORT i = 0; i < Header->e_shnum; i++)
    {
        PELF32_SHDR Shdr = (PELF32_SHDR)(ShdrBase + (i * Header->e_shentsize));
        PCSTR CurrentName = StrTab + Shdr->sh_name;

        // Bandingkan nama section
        if (RtlEqualMemory((PVOID)CurrentName, (PVOID)SectionName, RtlRawStringLength(SectionName) + 1))
        {
            return Shdr;
        }
    }

    return NULL;
}

PELF32_SYM
VEAPI
RtlFindElfSymbolByName(
    IN PELF32_EHDR Header,
    IN PCSTR SymbolName
)
{
    if (!Header || !SymbolName) return NULL;

    // Cari Section Header untuk .symtab dan .strtab
    PELF32_SHDR SymTabShdr = RtlFindElfSectionByName(Header, ".symtab");
    PELF32_SHDR StrTabShdr = RtlFindElfSectionByName(Header, ".strtab");

    if (!SymTabShdr || !StrTabShdr)
    {
        // Kalo gak ada .symtab, coba cari .dynsym dan .dynstr (buat shared objects / PIE)
        SymTabShdr = RtlFindElfSectionByName(Header, ".dynsym");
        StrTabShdr = RtlFindElfSectionByName(Header, ".dynstr");
    }

    if (!SymTabShdr || !StrTabShdr)
    {
        return NULL;
    }

    PELF32_SYM SymTable = (PELF32_SYM)((ULONG_PTR)Header + SymTabShdr->sh_offset);
    PCSTR StrTable     = (PCSTR)((ULONG_PTR)Header + StrTabShdr->sh_offset);
    ULONG SymbolCount  = SymTabShdr->sh_size / sizeof(ELF32_SYM);

    for (ULONG i = 0; i < SymbolCount; i++)
    {
        PELF32_SYM Sym = &SymTable[i];
        if (Sym->st_name == 0) continue;

        PCSTR CurrentSymName = StrTable + Sym->st_name;

        if (RtlEqualMemory((PVOID)CurrentSymName, (PVOID)SymbolName, RtlRawStringLength(SymbolName) + 1))
        {
            return Sym;
        }
    }

    return NULL;
}

VEASTATUS
VEAPI
RtlApplyElfRelocations(
    IN PELF32_EHDR Header,
    IN ULONG_PTR ImageBase,
    IN PKERNEL_SYMBOL_RESOLVER KernelResolver
)
{
    if (!Header || !ImageBase)
    {
        return STATUS_INVALID_PARAMETER;
    }

    ULONG_PTR ShdrBase = (ULONG_PTR)Header + Header->e_shoff;

    // Iterasi semua Section Header buat nyari SHT_REL (Relocation Sections)
    for (USHORT i = 0; i < Header->e_shnum; i++)
    {
        PELF32_SHDR RelShdr = (PELF32_SHDR)(ShdrBase + (i * Header->e_shentsize));

        if (RelShdr->sh_type != SHT_REL)
        {
            continue; // Skip kalau bukan section relokasi
        }

        // Ambil section target yang mau di-patch
        PELF32_SHDR TargetShdr = (PELF32_SHDR)(ShdrBase + (RelShdr->sh_info * Header->e_shentsize));
        
        // Target address di RAM tempat kode/data berada
        ULONG_PTR TargetSectionBase = ImageBase + TargetShdr->sh_addr;

        // Ambil Symbol Table dan String Table terkait
        PELF32_SHDR SymTabShdr = (PELF32_SHDR)(ShdrBase + (RelShdr->sh_link * Header->e_shentsize));
        PELF32_SHDR StrTabShdr = (PELF32_SHDR)(ShdrBase + (SymTabShdr->sh_link * Header->e_shentsize));

        PELF32_SYM SymTable = (PELF32_SYM)((ULONG_PTR)Header + SymTabShdr->sh_offset);
        PCSTR StrTable     = (PCSTR)((ULONG_PTR)Header + StrTabShdr->sh_offset);

        PELF32_REL RelTable = (PELF32_REL)((ULONG_PTR)Header + RelShdr->sh_offset);
        ULONG RelCount      = RelShdr->sh_size / sizeof(ELF32_REL);

        for (ULONG j = 0; j < RelCount; j++)
        {
            PELF32_REL Rel = &RelTable[j];

            ULONG SymIndex = ELF32_R_SYM(Rel->r_info);
            UCHAR RelType  = ELF32_R_TYPE(Rel->r_info);

            PELF32_SYM Sym = &SymTable[SymIndex];
            ULONG_PTR SymValue = 0;

            if (Sym->st_shndx == 0) // SHN_UNDEF (Simbol eksternal dari Kernel!)
            {
                PCSTR SymName = StrTable + Sym->st_name;
                
                if (KernelResolver)
                {
                    SymValue = KernelResolver(SymName);
                }

                if (!SymValue)
                {
                    // Simbol kernel gak ditemukan! Driver bakal crash kalau lanjut.
                    return STATUS_PROCEDURE_NOT_FOUND;
                }
            }
            else
            {
                // Simbol lokal internal di dalam file driver itu sendiri
                SymValue = ImageBase + Sym->st_value;
            }

            // P = Tempat memori yang mau di-patch
            ULONG_PTR *PatchAddr = (ULONG_PTR *)(TargetSectionBase + Rel->r_offset);

            switch (RelType)
            {
                case R_386_32:
                    // Absolute Address: *P = *P + S
                    *PatchAddr += SymValue;
                    break;

                case R_386_PC32:
                    // Relative PC Address: *P = *P + S - P
                    *PatchAddr += (SymValue - (ULONG_PTR)PatchAddr);
                    break;

                default:
                    // Tipe relokasi x86 lain yang gak didukung
                    return STATUS_NOT_SUPPORTED;
            }
        }
    }

    return STATUS_SUCCESS;
}

/* EOF */