/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : elf.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : ELF RTL (Kernel & User-Mode Support)
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "kdp.h"
#include "mm.h"
#include "mmtype.h"
#include <rtl.h>
#include "rtlp.h"

typedef struct _VEA_ELF_DYNAMIC_INFO {
    PELF32_SYM SymTable;
    ULONG      SymCount;
    PCSTR      StrTable;
    ULONG      StrSize;
    PELF32_REL RelTable;
    ULONG      RelSize;
    PELF32_REL JmpRelTable;
    ULONG      JmpRelSize;
} VEA_ELF_DYNAMIC_INFO, *PVEA_ELF_DYNAMIC_INFO;

#define VEA_KERNEL_SPACE_START  0xC0000000UL
#define VEA_USER_SPACE_MIN      0x00010000UL
#define VEA_USER_SPACE_MAX      0x80000000UL

static BOOLEAN
RtlpIsValidElfAddress(
    IN ULONG_PTR Address,
    IN BOOLEAN IsKernelImage
)
{
    if (IsKernelImage)
    {
        return (Address >= VEA_KERNEL_SPACE_START);
    }
    else
    {
        return (Address >= VEA_USER_SPACE_MIN && Address < VEA_USER_SPACE_MAX);
    }
}

static ULONG_PTR
RtlpGetElfSectionVirtualAddress(
    IN PELF32_EHDR Header,
    IN PELF32_SHDR Shdr,
    IN LONG_PTR LoadBias
)
{
    if (Header->e_type == ET_REL)
    {
        return (ULONG_PTR)Header + Shdr->sh_offset;
    }

    if (Shdr->sh_flags & SHF_ALLOC)
    {
        return (ULONG_PTR)((LONG_PTR)Shdr->sh_addr + LoadBias);
    }

    return (ULONG_PTR)Header + Shdr->sh_offset;
}

/*
- Helper Internal: Parse segment PT_DYNAMIC untuk ET_EXEC / ET_DYN di memori
 */
static BOOLEAN
RtlpParseDynamicSegment(
    IN PELF32_EHDR Header,
    IN LONG_PTR LoadBias,
    IN BOOLEAN IsKernel,
    OUT PVEA_ELF_DYNAMIC_INFO DynInfo
)
{
    PELF32_PHDR DynPhdr = RtlFindElfProgramHeaderByType(Header, PT_DYNAMIC);
    if (!DynPhdr || DynPhdr->p_memsz == 0)
    {
        return FALSE;
    }

    ULONG_PTR DynAddr = (ULONG_PTR)((LONG_PTR)DynPhdr->p_vaddr + LoadBias);
    if (!RtlpIsValidElfAddress(DynAddr, IsKernel))
    {
        return FALSE;
    }

    RtlZeroMemory(DynInfo, sizeof(*DynInfo));

    ULONG_PTR SymTabVa = 0;
    ULONG_PTR StrTabVa = 0;
    ULONG_PTR RelVa    = 0;
    ULONG_PTR JmpRelVa = 0;
    ULONG_PTR HashVa   = 0;

    PELF32_DYN Dyn = (PELF32_DYN)DynAddr;
    ULONG MaxEntries = DynPhdr->p_memsz / sizeof(ELF32_DYN);

    for (ULONG i = 0; i < MaxEntries; i++)
    {
        if (Dyn[i].d_tag == DT_NULL)
        {
            break;
        }

        switch (Dyn[i].d_tag)
        {
            case DT_HASH:
                HashVa = (ULONG_PTR)((LONG_PTR)Dyn[i].d_un.d_ptr + LoadBias);
                break;
            case DT_SYMTAB:
                SymTabVa = (ULONG_PTR)((LONG_PTR)Dyn[i].d_un.d_ptr + LoadBias);
                break;
            case DT_STRTAB:
                StrTabVa = (ULONG_PTR)((LONG_PTR)Dyn[i].d_un.d_ptr + LoadBias);
                break;
            case DT_STRSZ:
                DynInfo->StrSize = Dyn[i].d_un.d_val;
                break;
            case DT_REL:
                RelVa = (ULONG_PTR)((LONG_PTR)Dyn[i].d_un.d_ptr + LoadBias);
                break;
            case DT_RELSZ:
                DynInfo->RelSize = Dyn[i].d_un.d_val;
                break;
            case DT_JMPREL:
                JmpRelVa = (ULONG_PTR)((LONG_PTR)Dyn[i].d_un.d_ptr + LoadBias);
                break;
            case DT_PLTRELSZ:
                DynInfo->JmpRelSize = Dyn[i].d_un.d_val;
                break;
            default:
                break;
        }
    }

    if (SymTabVa && RtlpIsValidElfAddress(SymTabVa, IsKernel))
    {
        DynInfo->SymTable = (PELF32_SYM)SymTabVa;
    }

    if (StrTabVa && RtlpIsValidElfAddress(StrTabVa, IsKernel))
    {
        DynInfo->StrTable = (PCSTR)StrTabVa;
    }

    if (RelVa && RtlpIsValidElfAddress(RelVa, IsKernel))
    {
        DynInfo->RelTable = (PELF32_REL)RelVa;
    }

    if (JmpRelVa && RtlpIsValidElfAddress(JmpRelVa, IsKernel))
    {
        DynInfo->JmpRelTable = (PELF32_REL)JmpRelVa;
    }

    /* Hitung jumlah simbol di .dynsym */
    if (HashVa && RtlpIsValidElfAddress(HashVa, IsKernel))
    {
        /* SysV hash header: [0] = nbucket, [1] = nchain (jumlah simbol) */
        PULONG HashHeader = (PULONG)HashVa;
        DynInfo->SymCount = HashHeader[1];
    }
    else if (SymTabVa && StrTabVa && StrTabVa > SymTabVa)
    {
        /* GNU ld selalu menempatkan .dynsym tepat sebelum .dynstr */
        DynInfo->SymCount = (ULONG)((StrTabVa - SymTabVa) / sizeof(ELF32_SYM));
    }
    else if (SymTabVa)
    {
        DynInfo->SymCount = 4096;
    }

    return TRUE;
}

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

    if (Header->e_ident[EI_CLASS] != ELFCLASS32 ||
        Header->e_ident[EI_DATA]  != ELFDATA2LSB)
    {
        DPRINT("[ELF/VAL] Class/Data mismatch! Class=%u (req %u), Data=%u (req %u)\n",
                Header->e_ident[EI_CLASS], ELFCLASS32,
                Header->e_ident[EI_DATA], ELFDATA2LSB);
        return FALSE;
    }

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
    if (PfFlags == 0) return PAGE_NOACCESS;
    if ((PfFlags & PF_X) && (PfFlags & PF_W)) return PAGE_EXECUTE_READWRITE;
    if (PfFlags & PF_X) return PAGE_EXECUTE_READ;
    if (PfFlags & PF_W) return PAGE_READWRITE;
    if (PfFlags & PF_R) return PAGE_READONLY;
    return PAGE_READONLY;
}

ULONG
VEAPI
RtlElfShFlagsToVeaProtection(ULONG sh_flags)
{
    if ((sh_flags & SHF_EXECINSTR) && (sh_flags & SHF_WRITE)) return PAGE_EXECUTE_READWRITE;
    if (sh_flags & SHF_EXECINSTR) return PAGE_EXECUTE_READ;
    if (sh_flags & SHF_WRITE) return PAGE_READWRITE;
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
            if (Phdr->p_memsz == 0) continue;

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
    if (!Header || Index >= Header->e_phnum) return NULL;
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

    if (Header->e_shstrndx >= Header->e_shnum) return NULL;

    BOOLEAN IsKernel = ((ULONG_PTR)Header >= VEA_KERNEL_SPACE_START);
    if (!RtlpIsValidElfAddress((ULONG_PTR)Header, IsKernel)) return NULL;

    ULONG_PTR ShdrBase = (ULONG_PTR)Header + Header->e_shoff;
    if (!RtlpIsValidElfAddress(ShdrBase, IsKernel)) return NULL;
    
    PELF32_SHDR StrTabShdr = (PELF32_SHDR)(ShdrBase + (Header->e_shstrndx * Header->e_shentsize));
    PCSTR StrTab = (PCSTR)((ULONG_PTR)Header + StrTabShdr->sh_offset);

    if (!RtlpIsValidElfAddress((ULONG_PTR)StrTab, IsKernel)) return NULL;

    for (USHORT i = 0; i < Header->e_shnum; i++)
    {
        PELF32_SHDR Shdr = (PELF32_SHDR)(ShdrBase + (i * Header->e_shentsize));
        if (Shdr->sh_name >= StrTabShdr->sh_size) continue;

        PCSTR CurrentName = StrTab + Shdr->sh_name;
        if (!RtlpIsValidElfAddress((ULONG_PTR)CurrentName, IsKernel)) continue;

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

    BOOLEAN IsKernel = ((ULONG_PTR)Header >= VEA_KERNEL_SPACE_START);
    if (!RtlpIsValidElfAddress((ULONG_PTR)Header, IsKernel))
    {
        DPRINT("[MM/ELF] Header address 0x%p out of valid bounds!\n", Header);
        return NULL;
    }

    LONG_PTR LoadBias = 0;
    if (Header->e_type == ET_EXEC || Header->e_type == ET_DYN)
    {
        PVOID PreferredBase = NULL;
        SIZE_T ImgSize = 0;
        if (VEA_SUCCESS(RtlCalculateElfImageBounds(Header, &PreferredBase, &ImgSize)))
        {
            LoadBias = (LONG_PTR)((ULONG_PTR)Header - (ULONG_PTR)PreferredBase);
        }

        /* Jalur utama runtime: cari lewat PT_DYNAMIC (.dynsym / .dynstr) tanpa sentuh e_shoff */
        VEA_ELF_DYNAMIC_INFO DynInfo;
        if (RtlpParseDynamicSegment(Header, LoadBias, IsKernel, &DynInfo) &&
            DynInfo.SymTable && DynInfo.StrTable)
        {
            ULONG NameLen = RtlRawStringLength(SymbolName);
            for (ULONG i = 0; i < DynInfo.SymCount; i++)
            {
                PELF32_SYM Sym = &DynInfo.SymTable[i];
                if (Sym->st_name == 0) continue;
                if (DynInfo.StrSize > 0 && Sym->st_name >= DynInfo.StrSize) continue;

                PCSTR CurrentSymName = DynInfo.StrTable + Sym->st_name;
                if (!RtlpIsValidElfAddress((ULONG_PTR)CurrentSymName, IsKernel)) continue;

                if (RtlRawStringLength(CurrentSymName) == (LONG)NameLen &&
                    RtlEqualMemory((PVOID)CurrentSymName, (PVOID)SymbolName, NameLen))
                {
                    return Sym;
                }
            }
            return NULL;
        }
    }

    /* Fallback Section Header Table (untuk ET_REL atau ELF tanpa PT_DYNAMIC) */
    ULONG_PTR ShdrAddr = (ULONG_PTR)Header + Header->e_shoff;
    if (!RtlpIsValidElfAddress(ShdrAddr, IsKernel) || Header->e_shoff == 0 || Header->e_shnum == 0)
    {
        return NULL;
    }

    PELF32_SHDR ShdrBase   = (PELF32_SHDR)ShdrAddr;
    PELF32_SHDR SymTabShdr = NULL;
    PELF32_SHDR StrTabShdr = NULL;

    for (USHORT i = 0; i < Header->e_shnum; i++)
    {
        if (ShdrBase[i].sh_type == SHT_DYNSYM || ShdrBase[i].sh_type == SHT_SYMTAB)
        {
            SymTabShdr = &ShdrBase[i];
            if (SymTabShdr->sh_link < Header->e_shnum)
            {
                StrTabShdr = &ShdrBase[SymTabShdr->sh_link];
            }
            if (ShdrBase[i].sh_type == SHT_DYNSYM) break;
        }
    }

    if (!SymTabShdr || !StrTabShdr) return NULL;

    PELF32_SYM SymTable = (PELF32_SYM)RtlpGetElfSectionVirtualAddress(Header, SymTabShdr, LoadBias);
    PCSTR StrTable     = (PCSTR)RtlpGetElfSectionVirtualAddress(Header, StrTabShdr, LoadBias);
    ULONG SymbolCount  = SymTabShdr->sh_size / sizeof(ELF32_SYM);

    if (!RtlpIsValidElfAddress((ULONG_PTR)SymTable, IsKernel) ||
        !RtlpIsValidElfAddress((ULONG_PTR)StrTable, IsKernel))
    {
        return NULL;
    }

    ULONG NameLen = RtlRawStringLength(SymbolName);

    for (ULONG i = 0; i < SymbolCount; i++)
    {
        PELF32_SYM Sym = &SymTable[i];
        if (Sym->st_name == 0 || Sym->st_name >= StrTabShdr->sh_size) continue;

        PCSTR CurrentSymName = StrTable + Sym->st_name;
        if (!RtlpIsValidElfAddress((ULONG_PTR)CurrentSymName, IsKernel)) continue;

        if (RtlRawStringLength(CurrentSymName) == (LONG)NameLen &&
            RtlEqualMemory((PVOID)CurrentSymName, (PVOID)SymbolName, NameLen))
        {
            return Sym;
        }
    }

    return NULL;
}

ULONG_PTR
VEAPI
RtlGetElfSymbolAddress(
    IN PELF32_EHDR Header,
    IN ULONG_PTR ImageBase,
    IN PCSTR SymbolName
)
{
    PELF32_SYM Sym = RtlFindElfSymbolByName(Header, SymbolName);
    if (!Sym || Sym->st_shndx == SHN_UNDEF)
    {
        return 0;
    }

    if (Sym->st_shndx == SHN_ABS)
    {
        return (ULONG_PTR)Sym->st_value;
    }

    if (Header->e_type == ET_REL)
    {
        ULONG_PTR ShdrBase = (ULONG_PTR)Header + Header->e_shoff;
        PELF32_SHDR DefShdr = (PELF32_SHDR)(ShdrBase + (Sym->st_shndx * Header->e_shentsize));
        return ImageBase + DefShdr->sh_offset + Sym->st_value;
    }

    LONG_PTR LoadBias = 0;
    PVOID PreferredBase = NULL;
    SIZE_T ImgSize = 0;
    if (VEA_SUCCESS(RtlCalculateElfImageBounds(Header, &PreferredBase, &ImgSize)))
    {
        LoadBias = (LONG_PTR)(ImageBase - (ULONG_PTR)PreferredBase);
    }

    return (ULONG_PTR)((LONG_PTR)Sym->st_value + LoadBias);
}

/*
- Helper Internal: Proses array ELF32_REL (dipakai bersama oleh DT_REL, DT_JMPREL, dan SHT_REL)
 */
static VEASTATUS
RtlpProcessRelocationEntries(
    IN PELF32_EHDR Header,
    IN ULONG_PTR ImageBase,
    IN LONG_PTR LoadBias,
    IN BOOLEAN IsKernel,
    IN ULONG_PTR TargetSectionBase, /* Hanya dipakai jika Header->e_type == ET_REL */
    IN PELF32_REL RelTable,
    IN ULONG RelCount,
    IN PELF32_SYM SymTable,
    IN ULONG SymCount,
    IN PCSTR StrTable,
    IN ULONG StrSize,
    IN PKERNEL_SYMBOL_RESOLVER SymbolResolver
)
{
    for (ULONG j = 0; j < RelCount; j++)
    {
        PELF32_REL Rel = &RelTable[j];

        ULONG SymIndex = ELF32_R_SYM(Rel->r_info);
        UCHAR RelType  = ELF32_R_TYPE(Rel->r_info);

        /* FIX UTAMA:
         * - ET_REL          : r_offset adalah offset dari TargetSectionBase
         * - ET_EXEC / ET_DYN: r_offset adalah ELF Virtual Address -> cukup + LoadBias! */
        ULONG_PTR *PatchAddr;
        if (Header->e_type == ET_REL)
        {
            PatchAddr = (ULONG_PTR *)(TargetSectionBase + Rel->r_offset);
        }
        else
        {
            PatchAddr = (ULONG_PTR *)((LONG_PTR)Rel->r_offset + LoadBias);
        }

        if (!RtlpIsValidElfAddress((ULONG_PTR)PatchAddr, IsKernel))
        {
            DPRINT("[MM/ELF] RELOC ERROR: Patch Address %p is outside valid Virtual Memory!\n", PatchAddr);
            return STATUS_ACCESS_VIOLATION;
        }

        if (RelType == R_386_NONE)
        {
            continue;
        }

        if (RelType == R_386_RELATIVE)
        {
            *PatchAddr = (ULONG_PTR)((LONG_PTR)(*PatchAddr) + LoadBias);
            continue;
        }

        if (!SymTable || (SymCount > 0 && SymIndex >= SymCount))
        {
            DPRINT("[MM/ELF] RELOC ERROR: SymIndex %u out of range (Max %u)\n", SymIndex, SymCount);
            return STATUS_INVALID_IMAGE_FORMAT;
        }

        PELF32_SYM Sym = &SymTable[SymIndex];
        ULONG_PTR SymValue = 0;

        if (Sym->st_shndx == SHN_UNDEF)
        {
            if (!StrTable || (StrSize > 0 && Sym->st_name >= StrSize))
            {
                DPRINT("[MM/ELF] RELOC ERROR: Symbol %u st_name 0x%X exceeds strtab size 0x%X\n",
                       SymIndex, Sym->st_name, StrSize);
                return STATUS_INVALID_IMAGE_FORMAT;
            }

            PCSTR SymName = StrTable + Sym->st_name;

            if (SymbolResolver)
            {
                SymValue = SymbolResolver(SymName);
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
        else
        {
            if (Header->e_type == ET_REL)
            {
                if (Sym->st_shndx >= Header->e_shnum)
                {
                    return STATUS_INVALID_IMAGE_FORMAT;
                }
                ULONG_PTR ShdrBase = (ULONG_PTR)Header + Header->e_shoff;
                PELF32_SHDR SymDefShdr = (PELF32_SHDR)(ShdrBase + (Sym->st_shndx * Header->e_shentsize));
                SymValue = ImageBase + SymDefShdr->sh_offset + Sym->st_value;
            }
            else
            {
                SymValue = (ULONG_PTR)((LONG_PTR)Sym->st_value + LoadBias);
            }
        }

        switch (RelType)
        {
            case R_386_32:
                *PatchAddr += SymValue;
                break;

            case R_386_PC32:
                *PatchAddr += (SymValue - (ULONG_PTR)PatchAddr);
                break;

            case R_386_GLOB_DAT:
            case R_386_JMP_SLOT:
                *PatchAddr = SymValue;
                break;

            case R_386_COPY:
                if (Sym->st_size > 0)
                {
                    RtlCopyMemory(PatchAddr, (PVOID)SymValue, Sym->st_size);
                }
                break;

            default:
                DPRINT("[MM/ELF] RELOC ERROR: Unsupported relocation type %u\n", RelType);
                return STATUS_NOT_SUPPORTED;
        }
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
RtlApplyElfRelocations(
    IN PELF32_EHDR Header,
    IN ULONG_PTR ImageBase,
    IN PKERNEL_SYMBOL_RESOLVER SymbolResolver
)
{
    if (!Header || !ImageBase)
    {
        return STATUS_INVALID_PARAMETER;
    }

    BOOLEAN IsKernel = (ImageBase >= VEA_KERNEL_SPACE_START);
    if (!RtlpIsValidElfAddress((ULONG_PTR)Header, IsKernel) ||
        !RtlpIsValidElfAddress(ImageBase, IsKernel))
    {
        return STATUS_ACCESS_VIOLATION;
    }

    LONG_PTR LoadBias = 0;
    if (Header->e_type == ET_EXEC || Header->e_type == ET_DYN)
    {
        PVOID PreferredBase = NULL;
        SIZE_T ImgSize = 0;
        if (VEA_SUCCESS(RtlCalculateElfImageBounds(Header, &PreferredBase, &ImgSize)))
        {
            LoadBias = (LONG_PTR)(ImageBase - (ULONG_PTR)PreferredBase);
        }

        /* Jalur Runtime Userland / Shared Library: Proses PT_DYNAMIC (DT_REL + DT_JMPREL) */
        VEA_ELF_DYNAMIC_INFO DynInfo;
        if (RtlpParseDynamicSegment(Header, LoadBias, IsKernel, &DynInfo))
        {
            VEASTATUS Status = STATUS_SUCCESS;

            if (DynInfo.RelTable && DynInfo.RelSize > 0)
            {
                Status = RtlpProcessRelocationEntries(
                    Header,
                    ImageBase,
                    LoadBias,
                    IsKernel,
                    0,
                    DynInfo.RelTable,
                    DynInfo.RelSize / sizeof(ELF32_REL),
                    DynInfo.SymTable,
                    DynInfo.SymCount,
                    DynInfo.StrTable,
                    DynInfo.StrSize,
                    SymbolResolver
                );
                if (!VEA_SUCCESS(Status)) return Status;
            }

            if (DynInfo.JmpRelTable && DynInfo.JmpRelSize > 0)
            {
                Status = RtlpProcessRelocationEntries(
                    Header,
                    ImageBase,
                    LoadBias,
                    IsKernel,
                    0,
                    DynInfo.JmpRelTable,
                    DynInfo.JmpRelSize / sizeof(ELF32_REL),
                    DynInfo.SymTable,
                    DynInfo.SymCount,
                    DynInfo.StrTable,
                    DynInfo.StrSize,
                    SymbolResolver
                );
                if (!VEA_SUCCESS(Status)) return Status;
            }

            return STATUS_SUCCESS;
        }
    }

    /* Jalur Relocatable Object (ET_REL - Kernel Drivers / Modules) */
    if (Header->e_shoff == 0 || Header->e_shnum == 0)
    {
        return STATUS_SUCCESS;
    }

    ULONG_PTR ShdrBase = (ULONG_PTR)Header + Header->e_shoff;

    for (USHORT i = 0; i < Header->e_shnum; i++)
    {
        PELF32_SHDR RelShdr = (PELF32_SHDR)(ShdrBase + (i * Header->e_shentsize));

        if (RelShdr->sh_type != SHT_REL)
        {
            continue;
        }

        if (RelShdr->sh_info >= Header->e_shnum || RelShdr->sh_link >= Header->e_shnum)
        {
            return STATUS_INVALID_IMAGE_FORMAT;
        }

        PELF32_SHDR TargetShdr = (PELF32_SHDR)(ShdrBase + (RelShdr->sh_info * Header->e_shentsize));
        ULONG_PTR TargetSectionBase = (Header->e_type == ET_REL)
            ? (ImageBase + TargetShdr->sh_offset)
            : (ULONG_PTR)((LONG_PTR)TargetShdr->sh_addr + LoadBias);

        PELF32_SHDR SymTabShdr = (PELF32_SHDR)(ShdrBase + (RelShdr->sh_link * Header->e_shentsize));
        if (SymTabShdr->sh_link >= Header->e_shnum)
        {
            return STATUS_INVALID_IMAGE_FORMAT;
        }

        PELF32_SHDR StrTabShdr = (PELF32_SHDR)(ShdrBase + (SymTabShdr->sh_link * Header->e_shentsize));

        PELF32_SYM SymTable = (PELF32_SYM)RtlpGetElfSectionVirtualAddress(Header, SymTabShdr, LoadBias);
        PCSTR StrTable     = (PCSTR)RtlpGetElfSectionVirtualAddress(Header, StrTabShdr, LoadBias);

        if (!RtlpIsValidElfAddress((ULONG_PTR)SymTable, IsKernel) ||
            !RtlpIsValidElfAddress((ULONG_PTR)StrTable, IsKernel))
        {
            return STATUS_INVALID_IMAGE_FORMAT;
        }

        PELF32_REL RelTable = (PELF32_REL)RtlpGetElfSectionVirtualAddress(Header, RelShdr, LoadBias);

        VEASTATUS Status = RtlpProcessRelocationEntries(
            Header,
            ImageBase,
            LoadBias,
            IsKernel,
            TargetSectionBase,
            RelTable,
            RelShdr->sh_size / sizeof(ELF32_REL),
            SymTable,
            SymTabShdr->sh_size / sizeof(ELF32_SYM),
            StrTable,
            StrTabShdr->sh_size,
            SymbolResolver
        );

        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }
    }

    return STATUS_SUCCESS;
}

/* EOF */