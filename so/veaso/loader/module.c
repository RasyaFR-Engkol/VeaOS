/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : module.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VeaSO Module
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>
#include <psdk/veaelf.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file module.c
 * --------------------------------------------------------------------- */

#define LDRP_READ_CHUNK    0x1000
#define LDRP_MAX_IMAGE     0x4000000

extern PEB_LDR_DATA LdrpPebLdr;

VOID
LdrpSetModuleName(PLDR_MODULE Mod, PCSTR Name);

VOID
LdrpInsertModule(PLDR_MODULE Mod);

static 
VEASTATUS
LdrpReadFileImage(
    IN PSTR Path, 
    OUT PVOID *Buffer, 
    OUT PULONG Size)
{
    PVOID Heap = NtCurrentPeb()->ProcessHeap;
    PUCHAR Buf = NULL, NewBuf;
    ULONG Capacity = 0, Used = 0, Got;
    HANDLE hFile = NULL;
    ANSI_STRING PathStr;
    OBJECT_ATTRIBUTES ObjAttr;
    OIP_INFORMATION Iosb;
    VEASTATUS Status;

    *Buffer = NULL;
    *Size   = 0;

    RtlInitAnsiString(&PathStr, Path);
    InitializeObjectAttributes(&ObjAttr, &PathStr, OBJ_CASE_INSENSITIVE, NULL, NULL);

    Status = VzCreateFile(&hFile, FILE_READ_DATA | VEA_SYNCHRONIZE, &ObjAttr, &Iosb,
                          NULL, 0, FILE_SHARE_READ, FILE_OPEN,
                          FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[LDR/FILE] open '%s' failed: 0x%08X\n", Path, Status);
        return Status;
    }

    for (;;)
    {
        if (Capacity - Used < LDRP_READ_CHUNK)
        {
            ULONG NewCap = Capacity ? Capacity * 2 : LDRP_READ_CHUNK * 4;

            if (NewCap > LDRP_MAX_IMAGE) { Status = STATUS_FILE_TOO_LARGE; goto fail; }

            NewBuf = RtlAllocateHeap(Heap, 0, NewCap);
            if (!NewBuf) { Status = STATUS_NO_MEMORY; goto fail; }

            if (Buf)
            {
                RtlCopyMemory(NewBuf, Buf, Used);
                RtlFreeHeap(Heap, 0, Buf);
            }
            Buf = NewBuf;
            Capacity = NewCap;
        }

        Status = VzReadFile(hFile, NULL, NULL, NULL, &Iosb,
                            Buf + Used, LDRP_READ_CHUNK, NULL, NULL);

        if (Status == STATUS_END_OF_FILE) break;
        if (!VEA_SUCCESS(Status)) goto fail;

        Got = (ULONG)Iosb.Information;
        if (Got == 0) break;            /* EOF tanpa status khusus */
        Used += Got;
    }

    VzClose(hFile);

    if (Used == 0) { RtlFreeHeap(Heap, 0, Buf); return STATUS_INVALID_IMAGE_FORMAT; }

    *Buffer = Buf;
    *Size   = Used;
    return STATUS_SUCCESS;

fail:
    KdPrintf("[LDR/FILE] read '%s' failed: 0x%08X\n", Path, Status);
    if (hFile) VzClose(hFile);
    if (Buf)   RtlFreeHeap(Heap, 0, Buf);
    return Status;
}

static BOOLEAN
LdrpValidateElf(IN PELF32_EHDR Eh, IN ULONG FileSize)
{
    PELF32_PHDR Ph;
    ULONG i, LoadCount = 0, DynCount = 0;

    if (FileSize < sizeof(ELF32_EHDR)) return FALSE;

    if (Eh->e_ident[EI_MAG0] != 0x7F || Eh->e_ident[EI_MAG1] != 'E' ||
        Eh->e_ident[EI_MAG2] != 'L'  || Eh->e_ident[EI_MAG3] != 'F')   return FALSE;
    if (Eh->e_ident[EI_CLASS]   != ELFCLASS32)  return FALSE;
    if (Eh->e_ident[EI_DATA]    != ELFDATA2LSB) return FALSE;
    if (Eh->e_type    != ET_DYN)  return FALSE;
    if (Eh->e_machine != EM_386)  return FALSE;

    if (Eh->e_phentsize != sizeof(ELF32_PHDR)) return FALSE;
    if (Eh->e_phnum == 0 || Eh->e_phnum > 64)  return FALSE;
    if (Eh->e_phoff > FileSize ||
        (ULONG)Eh->e_phnum * sizeof(ELF32_PHDR) > FileSize - Eh->e_phoff)
        return FALSE;

    Ph = (PELF32_PHDR)((PUCHAR)Eh + Eh->e_phoff);
    for (i = 0; i < Eh->e_phnum; i++)
    {
        if (Ph[i].p_type == PT_LOAD)
        {
            LoadCount++;
            if (Ph[i].p_filesz > Ph[i].p_memsz)                       return FALSE;
            if (Ph[i].p_offset > FileSize ||
                Ph[i].p_filesz > FileSize - Ph[i].p_offset)           return FALSE;
            if (Ph[i].p_memsz > 0xFFFFFFFFUL - Ph[i].p_vaddr)         return FALSE;
        }
        else if (Ph[i].p_type == PT_DYNAMIC)
            DynCount++;
    }

    return (LoadCount > 0 && DynCount == 1);
}

VEASTATUS
LdrpParseDynamic(IN PLDR_MODULE Mod)
{
    PELF32_EHDR Eh = (PELF32_EHDR)Mod->Base;
    PELF32_PHDR Ph = (PELF32_PHDR)((PUCHAR)Eh + Eh->e_phoff);
    ULONG_PTR MinV = (ULONG_PTR)-1;
    LONG_PTR LoadBias;
    PELF32_DYN D;
    ULONG i;

    for (i = 0; i < Eh->e_phnum; i++)
    {
        if (Ph[i].p_type == PT_LOAD && Ph[i].p_vaddr < MinV)
        {
            MinV = Ph[i].p_vaddr;
        }
    }
    if (MinV == (ULONG_PTR)-1) return STATUS_INVALID_IMAGE_FORMAT;
    MinV &= ~(ULONG_PTR)0xFFF;

    LoadBias = (Eh->e_type == ET_EXEC)
        ? 0
        : (LONG_PTR)((ULONG_PTR)Mod->Base - MinV);

    for (i = 0; i < Eh->e_phnum; i++)
    {
        if (Ph[i].p_type == PT_DYNAMIC)
        {
            Mod->Dynamic = (PELF32_DYN)((LONG_PTR)Ph[i].p_vaddr + LoadBias);
            break;
        }
    }

    if (!Mod->Dynamic) return STATUS_INVALID_IMAGE_FORMAT;

    for (D = Mod->Dynamic; D->d_tag != DT_NULL; D++)
    {
        switch (D->d_tag)
        {
            case DT_STRTAB:
                Mod->StrTab = (PSTR)((LONG_PTR)D->d_un.d_ptr + LoadBias);
                break;
            case DT_INIT:
                Mod->InitFunction = (PVOID)((LONG_PTR)D->d_un.d_ptr + LoadBias);
                break;
            case DT_INIT_ARRAY:
                Mod->InitArray = (PVOID)((LONG_PTR)D->d_un.d_ptr + LoadBias);
                break;
            case DT_INIT_ARRAYSZ:
                Mod->InitArrayCount = D->d_un.d_val / sizeof(PVOID);
                break;
            case DT_FINI:
                Mod->FiniFunction = (PVOID)((LONG_PTR)D->d_un.d_ptr + LoadBias);
                break;
            case DT_FINI_ARRAY:
                Mod->FiniArray = (PVOID)((LONG_PTR)D->d_un.d_ptr + LoadBias);
                break;
            case DT_FINI_ARRAYSZ:
                Mod->FiniArrayCount = D->d_un.d_val / sizeof(PVOID);
                break;
        }
    }
    return Mod->StrTab ? STATUS_SUCCESS : STATUS_INVALID_IMAGE_FORMAT;
}

VEASTATUS
VEAPI
LdrpMapModule(
    IN  PCSTR Name,
    IN  PCSTR FullPath,
    OUT PLDR_MODULE *OutModule
)
{
    PVOID File = NULL, Base = NULL;
    ULONG FileSize, i;
    SIZE_T Span;
    ULONG_PTR MinV = (ULONG_PTR)-1, MaxV = 0;
    PELF32_EHDR Eh;
    PELF32_PHDR Ph;
    PLDR_MODULE Mod;
    VEASTATUS Status;

    Status = LdrpReadFileImage((PSTR)FullPath, &File, &FileSize);
    if (!VEA_SUCCESS(Status)) return Status;

    Eh = (PELF32_EHDR)File;
    /* Validasi: magic, ELFCLASS32, ET_DYN, EM_386, e_phoff dalam batas file */
    if (!LdrpValidateElf(Eh, FileSize)) { Status = STATUS_INVALID_IMAGE_FORMAT; goto fail; }

    Ph = (PELF32_PHDR)((PUCHAR)File + Eh->e_phoff);

    /* Hitung span PT_LOAD (page-aligned) */
    for (i = 0; i < Eh->e_phnum; i++)
    {
        if (Ph[i].p_type != PT_LOAD) continue;
        if (Ph[i].p_vaddr < MinV) MinV = Ph[i].p_vaddr;
        if (Ph[i].p_vaddr + Ph[i].p_memsz > MaxV) MaxV = Ph[i].p_vaddr + Ph[i].p_memsz;
    }
    if (MaxV == 0) { Status = STATUS_INVALID_IMAGE_FORMAT; goto fail; }

    MinV &= ~(ULONG_PTR)0xFFF;
    Span  = (MaxV - MinV + 0xFFF) & ~(SIZE_T)0xFFF;

    if (MinV >= 0x10000UL && MinV + Span <= 0x80000000UL)
    {
        Base = (PVOID)MinV;
        Status = VzAllocateVirtualMemory(NtCurrentProcess(), &Base, 0, &Span,
                                         MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }
    else
    {
        Status = STATUS_CONFLICTING_ADDRESSES;
    }

    if (!VEA_SUCCESS(Status))
    {
        Base = NULL;
        Status = VzAllocateVirtualMemory(NtCurrentProcess(), &Base, 0, &Span,
                                         MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!VEA_SUCCESS(Status)) goto fail;
    }

    /* Salin tiap segmen; BSS (memsz > filesz) otomatis nol dari alokasi baru */
    for (i = 0; i < Eh->e_phnum; i++)
    {
        if (Ph[i].p_type != PT_LOAD) continue;
        if (Ph[i].p_offset + Ph[i].p_filesz > FileSize) { Status = STATUS_INVALID_IMAGE_FORMAT; goto fail; }
        RtlCopyMemory((PUCHAR)Base + (Ph[i].p_vaddr - MinV),
                      (PUCHAR)File + Ph[i].p_offset,
                      Ph[i].p_filesz);
    }

    /* Buat LDR_MODULE */
    Mod = RtlAllocateHeap(NtCurrentPeb()->ProcessHeap, HEAP_ZERO_MEMORY, sizeof(LDR_MODULE));
    if (!Mod) { Status = STATUS_NO_MEMORY; goto fail; }

    Mod->Base      = Base;
    Mod->LoadCount = 1;
    LdrpSetModuleName(Mod, Name);
    LdrpInsertModule(Mod);

    RtlFreeHeap(NtCurrentPeb()->ProcessHeap, 0, File);
    *OutModule = Mod;
    return STATUS_SUCCESS;

fail:
    if (File) RtlFreeHeap(NtCurrentPeb()->ProcessHeap, 0, File);
    if (Base) VzFreeVirtualMemory(NtCurrentProcess(), &Base, &Span, MEM_RELEASE);
    return Status;
}

VOID
LdrpRunInitRoutines(VOID)
{
    PLIST_ENTRY E;
    ULONG i;

    for (E = LdrpPebLdr.InInitializationOrderModuleList.Flink;
         E != &LdrpPebLdr.InInitializationOrderModuleList; E = E->Flink)
    {
        PLDR_MODULE M = CONTAINING_RECORD(E, LDR_MODULE, InInitializationOrderLinks);
        if (M->Flags & LDRP_FLAG_INITED) continue;
        M->Flags |= LDRP_FLAG_INITED;

        if (M->InitFunction)
            ((VOID (*)(VOID))M->InitFunction)();

        for (i = 0; i < M->InitArrayCount; i++)
            ((VOID (*)(VOID))((PVOID *)M->InitArray)[i])();
    }
}

VOID
VEAPI
LdrpRunFiniRoutines(VOID)
{
    PLIST_ENTRY E;

    /* Telusuri InInitializationOrderModuleList secara MUNDUR (Blink) */
    for (E = LdrpPebLdr.InInitializationOrderModuleList.Blink;
         E != &LdrpPebLdr.InInitializationOrderModuleList;
         E = E->Blink)
    {
        PLDR_MODULE M = CONTAINING_RECORD(E, LDR_MODULE, InInitializationOrderLinks);

        if (!(M->Flags & LDRP_FLAG_INITED) || (M->Flags & LDRP_FLAG_DEINITED))
            continue;

        M->Flags |= LDRP_FLAG_DEINITED;

        /* 1. Jalankan .fini_array dari elemen terakhir ke pertama */
        if (M->FiniArray && M->FiniArrayCount > 0)
        {
            PVOID *Arr = (PVOID *)M->FiniArray;
            for (ULONG i = M->FiniArrayCount; i > 0; i--)
            {
                if (Arr[i - 1])
                    ((VOID (*)(VOID))Arr[i - 1])();
            }
        }

        /* 2. Jalankan .fini setelah .fini_array selesai */
        if (M->FiniFunction)
            ((VOID (*)(VOID))M->FiniFunction)();
    }
}

/* EOF */