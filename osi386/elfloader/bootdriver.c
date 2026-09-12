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

    /* Search Symbol Name */
    for (ULONG i = 0; i < SymCount; i++)
    {
        PSTR Name = StrTab + SymTab[i].st_name;
        if (SymTab[i].st_name != 0 && RtlRawStringCompareN(Name, SymbolName, RtlRawStringLength(SymbolName)) == 0)
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
                    return STATUS_PROCEDURE_NOT_FOUND;
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
    DriverEntry->EntryPoint = (PVOID)((PUCHAR)FileBuffer + Header->e_entry);
    DriverEntry->Flags      = 0x00000001; // LDRP_DRIVER_LOADED / RELOCATED
    
    ULONG PathLen = (ULONG)RtlRawStringLength(ResolvedPath);
    ULONG ServiceLen = (ULONG)RtlRawStringLength(ServiceName);
    PSTR FilePathBuf = (PSTR)LmAllocatePool(LdrUnreclaimablePool, (PathLen + 1));
    PSTR RegPathBuf  = (PSTR)LmAllocatePool(LdrUnreclaimablePool, (ServiceLen + 1));

    if (!FilePathBuf || !RegPathBuf)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyRawString(FilePathBuf, ResolvedPath);
    RtlCopyRawString(RegPathBuf, ServiceName);

    DriverEntry->FilePath.Buffer = FilePathBuf;
    DriverEntry->FilePath.Length = PathLen + 1;
    DriverEntry->FilePath.MaximumLength = DriverEntry->FilePath.Length;
    DriverEntry->RegistryPath.Buffer = RegPathBuf;
    DriverEntry->RegistryPath.Length = ServiceLen + 1;
    DriverEntry->RegistryPath.MaximumLength = DriverEntry->RegistryPath.Length;

    InsertTailList(&LdrBlockBoot->BootDriverList, &DriverEntry->Link);

    LdrInfo(ResolvedPath);

    return STATUS_SUCCESS;
}