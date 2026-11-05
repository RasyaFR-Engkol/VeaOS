/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : elf.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : ELF RTL 
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "kdp.h"
#include "mm.h"
#include "mmtype.h"
#include <rtl.h>
#include "rtlp.h"

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
        DPRINT("[ELF/VAL] Buffer null or size too small (%u < %u)\n", 
                (ULONG)BufferSize, sizeof(ELF32_EHDR));
        return FALSE;
    }

    /* 1. Check Magic Bytes */
    if (Header->e_ident[EI_MAG0] != ELFMAG0 ||
        Header->e_ident[EI_MAG1] != ELFMAG1 ||
        Header->e_ident[EI_MAG2] != ELFMAG2 ||
        Header->e_ident[EI_MAG3] != ELFMAG3)
    {
        DPRINT("[ELF/VAL] Magic mismatch: 0x%02X '%c' '%c' '%c'\n",
                Header->e_ident[EI_MAG0], Header->e_ident[EI_MAG1],
                Header->e_ident[EI_MAG2], Header->e_ident[EI_MAG3]);
        return FALSE;
    }

    /* 2. Check Architecture & Endianness */
    if (Header->e_ident[EI_CLASS] != ELFCLASS32 ||
        Header->e_ident[EI_DATA]  != ELFDATA2LSB)
    {
        DPRINT("[ELF/VAL] Class/Data mismatch! Class=%u (req %u), Data=%u (req %u)\n",
                Header->e_ident[EI_CLASS], ELFCLASS32,
                Header->e_ident[EI_DATA], ELFDATA2LSB);
        return FALSE;
    }

    /* 3. Validasi berdasarkan Tipe ELF */
    if (Header->e_type == ET_REL)
    {
        if (Header->e_shoff == 0 || Header->e_shnum == 0)
        {
            DPRINT("[ELF/VAL] ET_REL invalid section headers: shoff=0x%X, shnum=%u\n",
                    Header->e_shoff, Header->e_shnum);
            return FALSE;
        }
    }
    else if (Header->e_type == ET_EXEC || Header->e_type == ET_DYN)
    {
        if (Header->e_phoff == 0 || Header->e_phnum == 0)
        {
            DPRINT("[ELF/VAL] ET_EXEC/DYN invalid program headers: phoff=0x%X, phnum=%u\n",
                    Header->e_phoff, Header->e_phnum);
            return FALSE;
        }
    }
    else
    {
        DPRINT("[ELF/VAL] Unknown ELF type: 0x%04X\n", Header->e_type);
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

ULONG
VEAPI
RtlElfShFlagsToVeaProtection(ULONG sh_flags)
{
    if ((sh_flags & SHF_EXECINSTR) && (sh_flags & SHF_WRITE))
        return PAGE_EXECUTE_READWRITE;
    
    if (sh_flags & SHF_EXECINSTR)
        return PAGE_EXECUTE_READ;
    
    if (sh_flags & SHF_WRITE)
        return PAGE_READWRITE;

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

        if (Phdr->p_type == PT_LOAD)
        {
            if (Phdr->p_memsz == 0)
            {
                continue;
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
    
    PELF32_SHDR StrTabShdr = (PELF32_SHDR)(ShdrBase + (Header->e_shstrndx * Header->e_shentsize));
    PCSTR StrTab = (PCSTR)((ULONG_PTR)Header + StrTabShdr->sh_offset);

    // Cek apakah StrTab berada di memori Higher Half yang valid
    if ((ULONG_PTR)StrTab < 0xC0000000)
    {
        return NULL;
    }

    for (USHORT i = 0; i < Header->e_shnum; i++)
    {
        PELF32_SHDR Shdr = (PELF32_SHDR)(ShdrBase + (i * Header->e_shentsize));
        
        if (Shdr->sh_name >= StrTabShdr->sh_size)
        {
            continue;
        }

        PCSTR CurrentName = StrTab + Shdr->sh_name;

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

    // GUARD 1: Header wajib berada di Virtual Address Ring 0 (>= 0xC0000000)
    if ((ULONG_PTR)Header < 0xC0000000)
    {
        DPRINT("[MM/ELF] Header below 0xC0000000\n");
        return NULL;
    }

    // GUARD 2: Section Header Table wajib di Virtual Address Ring 0
    ULONG_PTR ShdrAddr = (ULONG_PTR)Header + Header->e_shoff;
    if (ShdrAddr < 0xC0000000 || Header->e_shoff == 0 || Header->e_shnum == 0)
    {
        DPRINT("[MM/ELF] ShdrAddr below 0xC0000000: it's at 0x%x\n", ShdrAddr);
        return NULL;
    }

    DPRINT("[MM/ELF] Scanning Header %p for symbol '%s' (e_shoff: 0x%X, e_shnum: %u)\n",
           Header, SymbolName, Header->e_shoff, Header->e_shnum);

    PELF32_SHDR ShdrBase   = (PELF32_SHDR)ShdrAddr;
    PELF32_SHDR SymTabShdr = NULL;
    PELF32_SHDR StrTabShdr = NULL;

    // Baru aman di-loop tanpa pemicu Page Fault!
    for (USHORT i = 0; i < Header->e_shnum; i++)
    {
        if (ShdrBase[i].sh_type == SHT_SYMTAB || ShdrBase[i].sh_type == SHT_DYNSYM)
        {
            SymTabShdr = &ShdrBase[i];
            
            if (SymTabShdr->sh_link < Header->e_shnum)
            {
                StrTabShdr = &ShdrBase[SymTabShdr->sh_link];
            }
            DPRINT("[MM/ELF] Found SymTab Shdr Index %u (sh_offset: 0x%X, sh_size: %u, sh_link: %u)\n",
                   i, SymTabShdr->sh_offset, SymTabShdr->sh_size, SymTabShdr->sh_link);
            break;
        }
    }

    if (!SymTabShdr || !StrTabShdr)
    {
        DPRINT("[MM/ELF] ERROR: SymTab or StrTab Shdr NOT FOUND!\n");
        return NULL;
    }

    PELF32_SYM SymTable = (PELF32_SYM)((ULONG_PTR)Header + SymTabShdr->sh_offset);
    PCSTR StrTable     = (PCSTR)((ULONG_PTR)Header + StrTabShdr->sh_offset);

    DPRINT("[MM/ELF] SymTable VA: %p, StrTable VA: %p\n", SymTable, StrTable);

    ULONG SymbolCount = SymTabShdr->sh_size / sizeof(ELF32_SYM);
    DPRINT("[MM/ELF] Total Symbols to check: %u\n", SymbolCount);

    // GUARD 3: Validasi VA Ring 0 untuk SymTable dan StrTable
    if ((ULONG_PTR)SymTable < 0xC0000000 || (ULONG_PTR)StrTable < 0xC0000000)
    {
        return NULL;
    }

    ULONG NameLen     = RtlRawStringLength(SymbolName);

    for (ULONG i = 0; i < SymbolCount; i++)
    {
        PELF32_SYM Sym = &SymTable[i];
        if (Sym->st_name == 0 || Sym->st_name >= StrTabShdr->sh_size) continue;

        PCSTR CurrentSymName = StrTable + Sym->st_name;
        if ((ULONG_PTR)CurrentSymName < 0xC0000000) continue;

        if (RtlRawStringLength(CurrentSymName) == (LONG)NameLen &&
            RtlEqualMemory((PVOID)CurrentSymName, (PVOID)SymbolName, NameLen))
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

    for (USHORT i = 0; i < Header->e_shnum; i++)
    {
        PELF32_SHDR RelShdr = (PELF32_SHDR)(ShdrBase + (i * Header->e_shentsize));

        if (RelShdr->sh_type != SHT_REL)
        {
            continue;
        }

        // Safety check batas indeks section
        if (RelShdr->sh_info >= Header->e_shnum || RelShdr->sh_link >= Header->e_shnum)
        {
            DPRINT("[MM/ELF] ERROR: Invalid Relocation Section Info/Link index (%u / %u)\n",
                   RelShdr->sh_info, RelShdr->sh_link);
            return STATUS_INVALID_IMAGE_FORMAT;
        }

        PELF32_SHDR TargetShdr = (PELF32_SHDR)(ShdrBase + (RelShdr->sh_info * Header->e_shentsize));
        
        ULONG_PTR TargetSectionBase;
        if (Header->e_type == ET_REL)
        {
            TargetSectionBase = ImageBase + TargetShdr->sh_offset;
        }
        else
        {
            TargetSectionBase = ImageBase + TargetShdr->sh_addr;
        }

        PELF32_SHDR SymTabShdr = (PELF32_SHDR)(ShdrBase + (RelShdr->sh_link * Header->e_shentsize));
        if (SymTabShdr->sh_link >= Header->e_shnum)
        {
            DPRINT("[MM/ELF] ERROR: SymTabShdr->sh_link invalid section index %u\n", SymTabShdr->sh_link);
            return STATUS_INVALID_IMAGE_FORMAT;
        }

        PELF32_SHDR StrTabShdr = (PELF32_SHDR)(ShdrBase + (SymTabShdr->sh_link * Header->e_shentsize));

        PELF32_SYM SymTable = (PELF32_SYM)((ULONG_PTR)Header + SymTabShdr->sh_offset);
        PCSTR StrTable     = (PCSTR)((ULONG_PTR)Header + StrTabShdr->sh_offset);

        // Validasi alamat tabel sebelum digunakan
        if ((ULONG_PTR)SymTable < 0xC0000000 || (ULONG_PTR)StrTable < 0xC0000000)
        {
            DPRINT("===================================================================\n");
            DPRINT("[MM/ELF] RELOCATION FAILURE DIAGNOSTIC DUMP\n");
            DPRINT("-------------------------------------------------------------------\n");
            DPRINT("Reason            : Section .symtab or .strtab is out of Ring 0 VA!\n");
            DPRINT("Header Base Address: 0x%p\n", Header);
            DPRINT("SymTable Calculated: 0x%p (sh_offset: 0x%X)\n", SymTable, SymTabShdr->sh_offset);
            DPRINT("StrTable Calculated: 0x%p (sh_offset: 0x%X)\n", StrTable, StrTabShdr->sh_offset);
            DPRINT("===================================================================\n");
            return STATUS_INVALID_IMAGE_FORMAT;
        }

        PELF32_REL RelTable = (PELF32_REL)((ULONG_PTR)Header + RelShdr->sh_offset);
        ULONG RelCount      = RelShdr->sh_size / sizeof(ELF32_REL);

        for (ULONG j = 0; j < RelCount; j++)
        {
            PELF32_REL Rel = &RelTable[j];

            ULONG SymIndex = ELF32_R_SYM(Rel->r_info);
            UCHAR RelType  = ELF32_R_TYPE(Rel->r_info);

            PELF32_SYM Sym = &SymTable[SymIndex];
            ULONG_PTR SymValue = 0;

            if (Sym->st_shndx == SHN_UNDEF)
            {
                if (Sym->st_name >= StrTabShdr->sh_size)
                {
                    DPRINT("[MM/ELF] RELOC ERROR: Symbol %u st_name 0x%X exceeds strtab size 0x%X\n",
                           SymIndex, Sym->st_name, StrTabShdr->sh_size);
                    return STATUS_INVALID_IMAGE_FORMAT;
                }

                PCSTR SymName = StrTable + Sym->st_name;
                
                if (KernelResolver)
                {
                    SymValue = KernelResolver(SymName);
                }

                if (!SymValue)
                {
                    DPRINT("[MM/ELF] RELOC ERROR: Could not resolve external symbol '%s' (Index %u)\n",
                           SymName, SymIndex);
                    return STATUS_PROCEDURE_NOT_FOUND;
                }
            }
            else if (Sym->st_shndx == SHN_ABS)
            {
                SymValue = Sym->st_value;
            }
            else if (Sym->st_shndx >= Header->e_shnum)
            {
                // DUMP LENGKAP & ABORT GRACEFULLY! Jangan biarkan loop lanjut dengan nilai sampah!
                DPRINT("===================================================================\n");
                DPRINT("[MM/ELF] CORRUPTED SYMBOL SECTION INDEX DETECTED!\n");
                DPRINT("-------------------------------------------------------------------\n");
                DPRINT("Relocation Item   : %u / %u\n", j + 1, RelCount);
                DPRINT("Symbol Index      : %u\n", SymIndex);
                DPRINT("Invalid st_shndx  : 0x%04X (Total Sections in ELF Header: %u)\n", 
                       Sym->st_shndx, Header->e_shnum);
                DPRINT("Symbol Value      : 0x%08X\n", Sym->st_value);
                DPRINT("Symbol Name Offset: 0x%08X\n", Sym->st_name);
                DPRINT("Target Section    : %u (Base Address: 0x%p)\n", RelShdr->sh_info, (PVOID)TargetSectionBase);
                DPRINT("Target Patch Addr : 0x%p\n", (PVOID)(TargetSectionBase + Rel->r_offset));
                DPRINT("===================================================================\n");
                
                return STATUS_INVALID_IMAGE_FORMAT;
            }
            else
            {
                if (Header->e_type == ET_REL)
                {
                    PELF32_SHDR SymDefShdr = (PELF32_SHDR)(ShdrBase + (Sym->st_shndx * Header->e_shentsize));
                    SymValue = ImageBase + SymDefShdr->sh_offset + Sym->st_value;
                }
                else
                {
                    SymValue = ImageBase + Sym->st_value;
                }
            }

            ULONG_PTR *PatchAddr = (ULONG_PTR *)(TargetSectionBase + Rel->r_offset);

            // Bounds check target patch address di RAM Kernel
            if ((ULONG_PTR)PatchAddr < 0xC0000000)
            {
                DPRINT("[MM/ELF] RELOC ERROR: Patch Address %p is outside Ring 0 Virtual Memory!\n", PatchAddr);
                return STATUS_ACCESS_VIOLATION;
            }

            switch (RelType)
            {
                case R_386_32:
                    *PatchAddr += SymValue;
                    break;

                case R_386_PC32:
                    *PatchAddr += (SymValue - (ULONG_PTR)PatchAddr);
                    break;

                default:
                    DPRINT("[MM/ELF] RELOC ERROR: Unsupported relocation type %u\n", RelType);
                    return STATUS_NOT_SUPPORTED;
            }
        }
    }

    return STATUS_SUCCESS;
}

/* EOF */