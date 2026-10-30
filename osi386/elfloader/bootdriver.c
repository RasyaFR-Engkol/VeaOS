/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : bootdriver.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Boot Driver Initializer
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include "elf_ldr.h"
#include <osi386.h>
#include <../ndk/veastatus.h>

/* Revision History ------------------------------------------------------
 * DATE       : 04-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file bootdriver.c
 * --------------------------------------------------------------------- */

PBLOCK_BOOT_2 LdrBlockBoot = NULL;
PVOID LdrBufferToKernel    = NULL;

VOID
VEAPI
LdrLoadBootDriver(IN PBLOCK_BOOT_2 BlockBoot2)
{
    /* Make sure Vk Parser is ready */
    if(!LvkParserReady)
    {
        LvkInitializeLoaderParser(BlockBoot2);
    }

    /* Set our BlockBoot2 as GLOBAL */
    LdrBlockBoot = BlockBoot2;

    /* Make sure our List Entry is initialized */
    InitializeListHead(&LdrBlockBoot->BootDriverList);

    /* Call our internal helper */
    LvkLoadBootDrivers();
}

static ULONG
LdrpFindKernelSymbol(
    IN Elf32_Ehdr* KernelEhdr,
    IN PCSTR SymbolName
)
{
    Elf32_Shdr* Shdr = (Elf32_Shdr*)((PUCHAR)KernelEhdr + KernelEhdr->e_shoff);
    Elf32_Sym*  SymTab = NULL;
    PSTR        StrTab = NULL;
    ULONG       SymCount = 0;

    /* Cari Seksi .symtab dan .strtab Kernel */
    for (ULONG i = 0; i < KernelEhdr->e_shnum; i++)
    {
        if (Shdr[i].sh_type == SHT_SYMTAB)
        {
            SymTab   = (Elf32_Sym*)((PUCHAR)KernelEhdr + Shdr[i].sh_offset);
            SymCount = Shdr[i].sh_size / sizeof(Elf32_Sym);
            
            /* Linked Section untuk SHT_SYMTAB adalah .strtab-nya */
            Elf32_Shdr* StrShdr = &Shdr[Shdr[i].sh_link];
            StrTab = (PSTR)((PUCHAR)KernelEhdr + StrShdr->sh_offset);
            break;
        }
    }

    if (!SymTab || !StrTab) return 0;

    ULONG SymbolNameLength = (ULONG)RtlRawStringLength(SymbolName);

    /* Search Symbol Name */
    for (ULONG i = 0; i < SymCount; i++)
    {
        PSTR Name = StrTab + SymTab[i].st_name;
        if (SymTab[i].st_name != 0 &&
            RtlRawStringLength(Name) == (LONG)SymbolNameLength &&
            RtlRawStringCompareN(Name, SymbolName, SymbolNameLength) == 0)
        {
            /* 
             * st_value di kernel ELF adalah alamat Virtual/Memory tempat
             * fungsi tersebut berada.
             */
            return SymTab[i].st_value;
        }
    }

    return 0; /* Symbol tidak ditemukan */
}

static PVOID
LdrpFindDriverEntryPoint(
    IN PVOID DriverBuffer
)
{
    Elf32_Ehdr* DriverEhdr = (Elf32_Ehdr*)DriverBuffer;
    Elf32_Shdr* SectionHeaders;
    Elf32_Sym* SymbolTable = NULL;
    PSTR StringTable = NULL;
    ULONG SymbolCount = 0;

    if (!DriverEhdr)
    {
        return NULL;
    }

    SectionHeaders = (Elf32_Shdr*)((PUCHAR)DriverBuffer + DriverEhdr->e_shoff);

    for (ULONG i = 0; i < DriverEhdr->e_shnum; i++)
    {
        if (SectionHeaders[i].sh_type == SHT_SYMTAB)
        {
            SymbolTable = (Elf32_Sym*)(
                (PUCHAR)DriverBuffer + SectionHeaders[i].sh_offset
            );
            SymbolCount = SectionHeaders[i].sh_size / sizeof(Elf32_Sym);

            if (SectionHeaders[i].sh_link >= DriverEhdr->e_shnum)
            {
                return NULL;
            }

            StringTable = (PSTR)(
                (PUCHAR)DriverBuffer +
                SectionHeaders[SectionHeaders[i].sh_link].sh_offset
            );
            break;
        }
    }

    if (!SymbolTable || !StringTable)
    {
        return NULL;
    }

    for (ULONG i = 0; i < SymbolCount; i++)
    {
        PSTR SymbolName;
        Elf32_Shdr* SymbolSection;

        if (SymbolTable[i].st_name == 0 ||
            SymbolTable[i].st_shndx == SHN_UNDEF ||
            SymbolTable[i].st_shndx >= DriverEhdr->e_shnum)
        {
            continue;
        }

        SymbolName = StringTable + SymbolTable[i].st_name;
        if (RtlRawStringCompareN(
                SymbolName,
                "DriverEntry",
                RtlRawStringLength("DriverEntry")) != 0 ||
            RtlRawStringLength(SymbolName) != RtlRawStringLength("DriverEntry"))
        {
            continue;
        }

        SymbolSection = &SectionHeaders[SymbolTable[i].st_shndx];
        if (SymbolTable[i].st_value >= SymbolSection->sh_size)
        {
            return NULL;
        }

        return (PVOID)(
            (PUCHAR)DriverBuffer +
            SymbolSection->sh_offset +
            SymbolTable[i].st_value
        );
    }

    return NULL;
}

VEASTATUS
VEAPI
LdrResolveBootDriverWithKernel(
    IN PVOID BootDriver,
    IN PVOID KernelElf
)
{
    Elf32_Ehdr* DriverEhdr = (Elf32_Ehdr*)BootDriver;
    Elf32_Ehdr* KernelEhdr = (Elf32_Ehdr*)KernelElf;

    if (!DriverEhdr || !KernelEhdr) return STATUS_INVALID_PARAMETER;

    Elf32_Shdr* DriverShdr = (Elf32_Shdr*)((PUCHAR)BootDriver + DriverEhdr->e_shoff);

    Elf32_Sym* DriverSymTab = NULL;
    PSTR       DriverStrTab = NULL;

    for (ULONG i = 0; i < DriverEhdr->e_shnum; i++)
    {
        if (DriverShdr[i].sh_type == SHT_SYMTAB)
        {
            DriverSymTab = (Elf32_Sym*)((PUCHAR)BootDriver + DriverShdr[i].sh_offset);
            
            Elf32_Shdr* StrShdr = &DriverShdr[DriverShdr[i].sh_link];
            DriverStrTab = (PSTR)((PUCHAR)BootDriver + StrShdr->sh_offset);
            break;
        }
    }

    if (!DriverSymTab || !DriverStrTab)
    {
        return STATUS_FILE_EXECUTABLE_INVALID;
    }

    for (ULONG i = 0; i < DriverEhdr->e_shnum; i++)
    {
        if (DriverShdr[i].sh_type != SHT_REL) continue;

        Elf32_Rel* RelArray = (Elf32_Rel*)((PUCHAR)BootDriver + DriverShdr[i].sh_offset);
        ULONG      RelCount = DriverShdr[i].sh_size / sizeof(Elf32_Rel);

        /* Target Seksi yang akan di-patch (sh_info menunjuk target section) */
        Elf32_Shdr* TargetSection = &DriverShdr[DriverShdr[i].sh_info];
        PUCHAR TargetSectionBase = (PUCHAR)BootDriver + TargetSection->sh_offset;

        for (ULONG j = 0; j < RelCount; j++)
        {
            Elf32_Rel* Rel = &RelArray[j];
            
            ULONG SymIndex = ELF32_R_SYM(Rel->r_info);
            ULONG RelType  = ELF32_R_TYPE(Rel->r_info);

            Elf32_Sym* Sym = &DriverSymTab[SymIndex];
            PSTR SymName   = DriverStrTab + Sym->st_name;

            ULONG SymbolAddress = 0;

            /* Case 1: Simbol berasal dari luar (Kernel) -> SHN_UNDEF */
            if (Sym->st_shndx == SHN_UNDEF)
            {
                SymbolAddress = LdrpFindKernelSymbol(KernelEhdr, SymName);
                if (SymbolAddress == 0)
                {
                    LdrWarning("Undefined Kernel Symbol:\n");
                    BvPrintLog(SymName);
                    while(1);
                    return STATUS_SYMBOL_RESOLVE_NOT_FOUND;
                }
            }
            /* Case 2: Simbol internal milik Driver sendiri */
            else
            {
                Elf32_Shdr* SymSection = &DriverShdr[Sym->st_shndx];
                SymbolAddress = (ULONG)((PUCHAR)BootDriver + SymSection->sh_offset + Sym->st_value);
            }

            /* Lokasi persis di memori yang harus di-patch (P) */
            PULONG Location = (PULONG)(TargetSectionBase + Rel->r_offset);

            /* Aplikasikan rumus Relokasi x86 */
            switch (RelType)
            {
                case R_386_32:
                    /* Absolute Address: *Location += S */
                    *Location += SymbolAddress;
                    break;

                case R_386_PC32:
                    /* Relative Call/Jmp: *Location += S - P */
                    *Location += (SymbolAddress - (ULONG)Location);
                    break;

                case R_386_NONE:
                    break;

                default:
                    LdrWarning("Unsupported Relocation Type: \n");
                    return STATUS_NOT_SUPPORTED;
            }
        }
    }

    return STATUS_SUCCESS;
}

LONG
VEAPI
LdrInitLoadBootDriver(
    IN PSTR ServiceName,
    IN PSTR ResolvedPath
)
{
    VEASTATUS Status;

    if(!ServiceName || !ResolvedPath) return STATUS_FILE_NOT_FOUND;

    VFS_FILE_INFO FileInfo;
    if(FsLookupFileInformation(ResolvedPath, &FileInfo) < 0)
    {
        return STATUS_FILE_NOT_FOUND;
    }

    PVOID FileBuffer = (PVOID)LmAllocatePool(LdrUnreclaimablePool, FileInfo.FileSize);
    if(!FileBuffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ULONG BytesReaded;
    BytesReaded = FsReadFile(&FileInfo, FileBuffer);
    if(BytesReaded == 0)
    {
        return STATUS_READ_DISK_ERROR;
    }

    Elf32_Ehdr* Header = (Elf32_Ehdr*)FileBuffer;
    if(Header->e_ident[0] != 0x7F || 
        Header->e_ident[1] != 'E' || 
        Header->e_ident[2] != 'L' || 
        Header->e_ident[3] != 'F')
    {
        return STATUS_FILE_EXECUTABLE_INVALID;
    }

    /* Relocate it*/
    Status = LdrResolveBootDriverWithKernel((PVOID)Header, LdrBufferToKernel);
    if(!VEA_SUCCESS(Status))
    {
        return Status;
    }

    PBOOT_DRIVER_LIST_ENTRY DriverEntry = (PBOOT_DRIVER_LIST_ENTRY)LmAllocatePool(
        LdrUnreclaimablePool, 
        sizeof(BOOT_DRIVER_LIST_ENTRY)
    );

    if (!DriverEntry)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(DriverEntry, sizeof(BOOT_DRIVER_LIST_ENTRY));

    /* Fill drier metadata */
    DriverEntry->ImageBase = FileBuffer;
    DriverEntry->ImageSize = FileInfo.FileSize;
    DriverEntry->EntryPoint = LdrpFindDriverEntryPoint(FileBuffer);
    if (!DriverEntry->EntryPoint)
    {
        return STATUS_PROCEDURE_NOT_FOUND;
    }
    DriverEntry->Flags      = 0x00000001; // LDRP_DRIVER_LOADED / RELOCATED

    /* Build string */
    CHAR RegistryPrefix[53];
    RtlCopyMemory(RegistryPrefix, "/REGISTRY/MACHINE/SYSTEM/CurrentControlSet/Services/", 53);
    ULONG RegistryPrefixLen = (ULONG)RtlRawStringLength(RegistryPrefix);
    
    /* Calculate string size */
    ULONG PathLen = (ULONG)RtlRawStringLength(ResolvedPath);
    ULONG ServiceLen = (ULONG)RtlRawStringLength(ServiceName);
    ULONG RegistryPathLen = RegistryPrefixLen + ServiceLen;
    PSTR FilePathBuf = (PSTR)LmAllocatePool(LdrUnreclaimablePool, (PathLen + 1));
    PSTR RegPathBuf  = (PSTR)LmAllocatePool(
        LdrUnreclaimablePool,
        RegistryPathLen + 1
    );

    if (!FilePathBuf || !RegPathBuf)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyRawString(FilePathBuf, ResolvedPath);
    
    /* Fill first registry prefix first and append it */
    RtlCopyRawString(RegPathBuf, RegistryPrefix);
    if (!RtlAppendRawString(
            RegPathBuf,
            RegistryPathLen + 1,
            ServiceName))
    {
        return STATUS_NAME_TOO_LONG;
    }

    /* Fill the FilePath and RegistryPath */
    DriverEntry->FilePath.Buffer = FilePathBuf;
    DriverEntry->FilePath.Length = (USHORT)PathLen;
    DriverEntry->FilePath.MaximumLength = (USHORT)(PathLen + 1);
    DriverEntry->RegistryPath.Buffer = RegPathBuf;
    DriverEntry->RegistryPath.Length = (USHORT)RegistryPathLen;
    DriverEntry->RegistryPath.MaximumLength = (USHORT)(RegistryPathLen + 1);

    InsertTailList(&LdrBlockBoot->BootDriverList, &DriverEntry->Link);

    LdrInfo(ResolvedPath);

    return STATUS_SUCCESS;
}