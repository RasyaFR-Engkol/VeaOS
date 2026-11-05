/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : sysldr.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : System Loader
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>


/* Revision History ------------------------------------------------------
 * DATE       : 31-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file sysldr.c
 * --------------------------------------------------------------------- */
extern POBJECT_TYPE MmSectionObjectType;

/* DEBUG */

VOID
MiDumpMemory(
    IN PVOID BaseAddress,
    IN ULONG Length
)
{
    PUCHAR Buffer = (PUCHAR)BaseAddress;
    
    DPRINT("=================== MEMORY DUMP @ %p (%u bytes) ===================\n", BaseAddress, Length);
    for (ULONG i = 0; i < Length; i += 16)
    {
        DPRINT("%08X: %02X %02X %02X %02X %02X %02X %02X %02X - %02X %02X %02X %02X %02X %02X %02X %02X\n",
               i,
               Buffer[i+0], Buffer[i+1], Buffer[i+2], Buffer[i+3],
               Buffer[i+4], Buffer[i+5], Buffer[i+6], Buffer[i+7],
               Buffer[i+8], Buffer[i+9], Buffer[i+10], Buffer[i+11],
               Buffer[i+12], Buffer[i+13], Buffer[i+14], Buffer[i+15]);
    }
    DPRINT("===================================================================\n");
}

ULONG_PTR
VEAPI
MiFindKernelExport(IN PCSTR SymbolName)
{
    if (!SymbolName) return 0;

    PVEA_EXPORT_ENTRY Entry = __veasymboltable_start;

    // Scan seksi .veasymboltable dari start sampai end (atau NULL terminator)
    while (Entry < __veasymboltable_end && Entry->Name != NULL)
    {
        LONG NameLen = RtlRawStringLength(SymbolName);
        
        if (RtlRawStringLength(Entry->Name) == NameLen &&
            RtlEqualMemory((PVOID)Entry->Name, (PVOID)SymbolName, NameLen))
        {
            /* KETEMU! Return Virtual Address murni fungsi kernel */
            return (ULONG_PTR)Entry->Address;
        }

        Entry++;
    }

    DPRINT("[MM/LDR] ERROR: Unresolved kernel symbol '%s'!\n", SymbolName);
    return 0;
}

VEASTATUS
VEAPI
LdrRelocateImage(
    IN PVOID BaseAddress,
    IN PKERNEL_SYMBOL_RESOLVER KernelResolver OPTIONAL
)
{
    PELF32_EHDR ElfHeader = (PELF32_EHDR)BaseAddress;

    if (!BaseAddress)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (!RtlValidateElfHeader(ElfHeader, sizeof(ELF32_EHDR)))
    {
        DPRINT("[MM/LDR] Invalid ELF Header at %p!\n", BaseAddress);
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    PKERNEL_SYMBOL_RESOLVER Resolver = KernelResolver ? KernelResolver : MiFindKernelExport;

    VEASTATUS Status = RtlApplyElfRelocations(
        ElfHeader,
        (ULONG_PTR)BaseAddress,
        Resolver
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[MM/LDR] RtlApplyElfRelocations failed with status 0x%08X\n", Status);
        return Status;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
MiLoadImageSection(
    IN OUT PSECTION *SectionPtr,
    OUT PVOID *ImageBase,
    IN PANSI_STRING FileName,
    IN BOOLEAN SessionLoad,
    IN PDATA_TABLE_ENTRY LdrEntry
)
{
    PSECTION Section;
    VEASTATUS Status;
    PEPROCESS Process;
    PVOID TempBase = NULL;
    SIZE_T ViewSize = 0;
    LARGE_INTEGER SectionOffset;
    ULONG ImageSize;
    PFN_COUNT PageCount;
    PVOID DriverBase = NULL;
    ULONG_PTR CurrentVa;

    if (!SectionPtr || !*SectionPtr || !ImageBase)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Section = *SectionPtr;

    if (SessionLoad)
    {
        DPRINT("[MM/LDR] Session loading not supported!\n");
        return STATUS_NOT_IMPLEMENTED;
    }

    UNREFERENCED_PARAMETER(LdrEntry);

    Process = (PEPROCESS)PtGetCurrentProcess();
    RtlZeroMemory(&SectionOffset, sizeof(LARGE_INTEGER));

    Status = MmMapViewOfSection(
        Section,
        Process,
        &TempBase,
        0,
        0,
        &SectionOffset,
        &ViewSize,
        ViewUnmap,
        0,
        PAGE_EXECUTE_READ
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[MM/LDR] MmMapViewOfSection failed with status 0x%08X\n", Status);
        return Status;
    }

    ImageSize = (ULONG)Section->SizeOfSection.QuadPart;
    if (ImageSize == 0)
    {
        MmUnmapViewOfSection(Process, TempBase);
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    PageCount = (PFN_COUNT)((ImageSize + PAGE_SIZE - 1) >> PAGE_SHIFT);

TargetVa_Alloc:
    DriverBase = (PVOID)MmAllocateVirtualRange(PageCount);
    if (!DriverBase)
    {
        DPRINT("[MM/LDR] Failed to allocate virtual range for driver (%u pages)\n", PageCount);
        MmUnmapViewOfSection(Process, TempBase);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    DPRINT("[MM/LDR] Mapping Driver '%Z' at System VA 0x%p (%u KB, %u Pages)\n",
           FileName, DriverBase, ImageSize / 1024, PageCount);

    CurrentVa = (ULONG_PTR)DriverBase;

    for (PFN_COUNT i = 0; i < PageCount; i++, CurrentVa += PAGE_SIZE)
    {
        ULONG PhysPage = MmAllocatePhysicalPage();
        if (PhysPage == 0)
        {
            DPRINT("[MM/LDR] Out of physical memory during driver map!\n");
            MmUnmapViewOfSection(Process, TempBase);
            return STATUS_NO_MEMORY;
        }

        /* Dapatkan pointer ke Hardware PTE milik Kernel */
        PAGE_ENTRY *Pte = MM_GET_PTE(CurrentVa);

        /* Isi Bit PTE (Valid, Read/Write, Ring 0 Only) */
        Pte->raw = 0;
        Pte->bits.present = 1;
        Pte->bits.rw = 1;
        Pte->bits.user = 0;       // Kernel Mode (Ring 0)
        Pte->bits.frame = PhysPage >> PAGE_SHIFT;

        /* Invalidate CPU TLB untuk halaman virtual ini */
        MMI_INVALIDATE_TLB(CurrentVa);
    }

    RtlCopyMemory(DriverBase, TempBase, ImageSize);
    
    Status = MmUnmapViewOfSection(Process, TempBase);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[MM/LDR] Warning: Failed to unmap temporary section view: 0x%08X\n", Status);
    }

    *ImageBase = DriverBase;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
MmCheckSystemImage(IN HANDLE FileHandle)
{
    VEASTATUS Status;
    ELF32_EHDR ElfHeader;
    OIP_INFORMATION IoStatusBlock;
    LARGE_INTEGER ByteOffset;

    if (!FileHandle)
    {
        return STATUS_INVALID_PARAMETER;
    }

    /* Set offset pembacaan ke awal file (Offset 0) */
    ByteOffset.QuadPart = 0;

    /* 1. Baca Header ELF32 dari disk/file system via VeaReadFile */
    Status = VeaReadFile(
        FileHandle,
        NULL,
        NULL,
        NULL,
        &IoStatusBlock,
        &ElfHeader,
        sizeof(ELF32_EHDR),
        &ByteOffset,
        NULL
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[MM/LDR] Failed to read ELF header from FileHandle: 0x%08X\n", Status);
        return Status;
    }

    /* Pastikan byte yang terbaca cukup untuk satu struktur ELF32_EHDR */
    if (IoStatusBlock.Information < sizeof(ELF32_EHDR))
    {
        DPRINT("[MM/LDR] File too small to contain a valid ELF header!\n");
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    /* 2. Validasi Magic & Struktur Dasar ELF32 via Rtl */
    if (!RtlValidateElfHeader(&ElfHeader, sizeof(ELF32_EHDR)))
    {
        DPRINT("[MM/LDR] Invalid ELF magic or corrupt header structure!\n");
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    /* 3. Cek Arsitektur Target CPU (Wajib x86 32-bit / EM_386) */
    if (ElfHeader.e_machine != EM_386)
    {
        DPRINT("[MM/LDR] Machine type mismatch! Expected EM_386 (%u), got %u\n",
                EM_386, ElfHeader.e_machine);
        return STATUS_IMAGE_MACHINE_TYPE_MISMATCH;
    }

    /* 4. Cek Tipe File ELF (Wajib ET_REL untuk Driver Module .ko, atau ET_DYN/ET_EXEC) */
    if (ElfHeader.e_type != ET_REL && 
        ElfHeader.e_type != ET_DYN && 
        ElfHeader.e_type != ET_EXEC)
    {
        DPRINT("[MM/LDR] Invalid ELF file type for system image: %u\n", ElfHeader.e_type);
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    /* 5. Cek kelayakan Program/Section Header Table offset */
    if (ElfHeader.e_shoff == 0 && ElfHeader.e_phoff == 0)
    {
        DPRINT("[MM/LDR] ELF binary has no section or program headers!\n");
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    DPRINT("[MM/LDR] System image header validated successfully!\n");
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
MmLoadSystemImage(
    IN PANSI_STRING FileName,
    IN PANSI_STRING NamePrefix OPTIONAL,
    IN PANSI_STRING LoadedName OPTIONAL,
    IN ULONG Flags,
    OUT PVOID *ModuleObject,
    OUT PVOID *ImageBaseAddress
)
{
    PCHAR Buffer;
    ANSI_STRING BaseName, BaseDirectory;
    PLIST_ENTRY NextEntry;
    PLOADED_MODULE_LIST_ENTRY ModuleEntry;
    PSECTION Section = NULL;
    OBJECT_ATTRIBUTES ObjAttr;
    HANDLE FileHandle = NULL, SectionHandle = NULL;
    OIP_INFORMATION IoInformation;
    ACCESS_MASK DesiredAccess;
    PVOID LoadedModuleBase = NULL;
    ULONG DriverSize, EntrySize;
    PELF32_EHDR ElfHeader;
    ULONG EntryOffset;
    VEASTATUS Status;
    
    Buffer = UlAllocatePoolWithTag(
        NonPagedPool,
        256,
        'Ldr '
    );
    if (!Buffer) return STATUS_INSUFFICIENT_RESOURCES;

    if (FileName->Buffer[0] == '/')
    {
        PCHAR P;
        ULONG BaseLength;

        P = &FileName->Buffer[FileName->Length];
        while (*(P - 1) != '/') P--;

        BaseLength = (ULONG)(&FileName->Buffer[FileName->Length] - P);
        BaseLength *= sizeof(CHAR);

        BaseName.Length =(USHORT)BaseLength;
        BaseName.Buffer = P;
    }
    else
    {
        BaseName.Length = FileName->Length;
        BaseName.Buffer = FileName->Buffer;
    }

    BaseName.MaximumLength = BaseName.Length;

    BaseDirectory = *FileName;
    BaseDirectory.Length -= BaseName.Length;
    BaseDirectory.MaximumLength = BaseDirectory.Length;

    /* TODO: Support Prefix Name */

    if (LoadedName) BaseName = *LoadedName;

LoaderScan:
    NextEntry = PtLoadedModuleList.Flink;

    while (NextEntry != &PtLoadedModuleList)
    {
        ModuleEntry = CONTAINING_RECORD(NextEntry, LOADED_MODULE_LIST_ENTRY, Link);

        if (RtlEqualString(&BaseName, &ModuleEntry->FilePath, TRUE))
        {
            break; // Driver ketemu di list!
        }

        NextEntry = NextEntry->Flink;
    }

    if (NextEntry != &PtLoadedModuleList)
    {
        if (Section)
        {
            ObDereferenceObject(Section);
            Section = NULL;
        }

        goto Quickie;
    }
    else if (!Section)
    {
        KsLeaveCriticalRegion();
        
        ModuleEntry = NULL;

        InitializeObjectAttributes(
            &ObjAttr,
            FileName,
            OBJ_KERNEL_HANDLE,
            NULL,
            NULL
        );

        Status = VeaOpenFile(
            &FileHandle,
            FILE_EXECUTE,
            &ObjAttr,
            &IoInformation,
            FILE_SHARE_READ | FILE_SHARE_DELETE,
            0
        );
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("[Mm] VeaOpenFile failed: 0x%x\n", Status);
            goto Quickie;
        }

#if 0
        //Status = MmCheckSystemImage(FileHandle);
        //if ((Status == STATUS_INVALID_IMAGE_FORMAT) || 
        //    (Status == STATUS_INVALID_FILE_FOR_SECTION) ||
        //    (Status == STATUS_IMAGE_MACHINE_TYPE_MISMATCH))
        //{
        //    goto Quickie;
        //}
#endif
        
        if (Flags)
        {
            DesiredAccess = SECTION_MAP_READ | SECTION_MAP_WRITE;
        }
        else
        {
            DesiredAccess = SECTION_ALL_ACCESS;
        }

        InitializeObjectAttributes(
            &ObjAttr,
            NULL,
            OBJ_KERNEL_HANDLE,
            NULL,
            NULL
        );

        DPRINT("[LDR] FileHandle=%p\n", FileHandle);

        Status = VeaCreateSection(
            &SectionHandle,
            DesiredAccess,
            &ObjAttr,
            NULL,
            PAGE_EXECUTE,
            SEC_IMAGE,
            FileHandle
        );
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("VeaCreateSection failed: 0x%x\n", Status);
            goto Quickie;
        }

        Status = ObReferenceObjectByHandle(
            SectionHandle,
            SECTION_MAP_EXECUTE,
            MmSectionObjectType,
            KernelMode,
            (PVOID*)&Section
        );
        VeaClose(SectionHandle);

        if (!VEA_SUCCESS(Status)) goto Quickie;

        if (Flags)
        {
            /* TODO: Support Session Loading*/
            DPRINT("[Mm] Session Loading unsupported\n");
            goto Quickie;
        }
    }
    else
    {
        ModuleEntry = NULL;
    }

    Status = MiLoadImageSection(
        &Section,
        &LoadedModuleBase,
        FileName,
        FALSE,
        NULL
    );

    DriverSize = ((PMM_IMAGE_SECTION_OBJECT)Section->Segment)->ImageInformation.ImageFileSize;

    if (!Flags)
    {
        // NO SESSION THO. THIS IS TODO
    }

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("MiLoadImageSection failed: 0x%x\n", Status);
        goto Quickie;
    }

    Status = LdrRelocateImage(
        LoadedModuleBase, NULL
    );
    
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("LdrRelocateImage failed with status 0x%08X\n", Status);
        goto Quickie;
    }

    ElfHeader = (PELF32_EHDR)LoadedModuleBase;

    EntrySize = sizeof(LOADED_MODULE_LIST_ENTRY);
    ModuleEntry = (PLOADED_MODULE_LIST_ENTRY)UlAllocatePoolWithTag(
        NonPagedPool,
        EntrySize,
        TAG_MODULE_OBJECT
    );
    if (!ModuleEntry)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Quickie;
    }

    RtlZeroMemory(ModuleEntry, EntrySize);
    ModuleEntry->ImageBase = LoadedModuleBase;
    ModuleEntry->ImageSize = DriverSize;

    EntryOffset = ElfHeader->e_entry;
    if (ElfHeader->e_type == ET_REL)
    {
        PELF32_SHDR ShdrTable = (PELF32_SHDR)((ULONG_PTR)LoadedModuleBase + ElfHeader->e_shoff);
        for (ULONG i = 0; i < ElfHeader->e_shnum; i++)
        {
            /* Cari seksi yang punya flag Executable (biasanya .text) */
            if (ShdrTable[i].sh_flags & SHF_EXECINSTR)
            {
                EntryOffset += ShdrTable[i].sh_offset;
                break;
            }
        }
    }
    ModuleEntry->EntryPoint = (PVOID)((ULONG_PTR)LoadedModuleBase + EntryOffset);
    ModuleEntry->Flags = LDRP_SYSTEM_MAPPED | LDRP_DRIVER_LOADED;

    RtlDuplicateAnsiString(
        RTL_DUPLICATE_ANSI_STRING_NULL_TERMINATE,
        FileName,
        &ModuleEntry->FilePath
    );

    /* TODO: Notify every registered callback */

    InsertTailList(&PtLoadedModuleList, &ModuleEntry->Link);

    // MiWriteProtectSystemImage(LoadedModuleBase);

    if (ModuleObject)
    {
        *ModuleObject = ModuleEntry;
    }

    if (ImageBaseAddress)
    {
        *ImageBaseAddress = LoadedModuleBase;
    }
    
    Status = STATUS_SUCCESS;

Quickie:
    if (!VEA_SUCCESS(Status) && LoadedModuleBase != NULL)
    {
        // Unmap virtual memory jika loading gagal
        // MiUnmapSystemImage(LoadedModuleBase, DriverSize);
        LoadedModuleBase = NULL;
    }

    if (FileHandle) VeaClose(FileHandle);
    if (Buffer) UlFreePoolWithTag(Buffer, 'Ldr ');

    return Status;
}