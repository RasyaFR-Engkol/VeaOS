/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : init.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PURPOSE
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 08-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file init.c
 * --------------------------------------------------------------------- */

#define LDRP_DLL_DIR   "/D1P1:/VeaOS/SystemReserved/"

LONG LdrpProcessInitialized = 0;
PVOID VeaSoBase;
PEB_LDR_DATA LdrpPebLdr;
LDR_MODULE   LdrpExeModule;
LDR_MODULE   LdrpVeaSoModule;

VEASTATUS LdrpLoadDependencies(IN PLDR_MODULE Mod);
VEASTATUS LdrpParseDynamic(IN PLDR_MODULE Mod);
VEASTATUS
VEAPI
LdrpMapModule(
    IN  PCSTR Name,
    IN  PCSTR FullPath,
    OUT PLDR_MODULE *OutModule
);
VOID
LdrpRunInitRoutines(VOID);

VOID
LdrpSetModuleName(PLDR_MODULE Mod, PCSTR Name)
{
    ULONG i;
    for (i = 0; i < sizeof(Mod->Name) - 1 && Name[i]; i++)
        Mod->Name[i] = Name[i];
    Mod->Name[i] = '\0';
}

VOID
LdrpInsertModule(PLDR_MODULE Mod)
{
    InsertTailList(&LdrpPebLdr.InLoadOrderModuleList, &Mod->InLoadOrderLinks);
}

PLDR_MODULE
VEAPI
LdrpFindModuleByName(IN PCSTR Name)
{
    PLIST_ENTRY Head = &LdrpPebLdr.InLoadOrderModuleList;
    PLIST_ENTRY E;

    for (E = Head->Flink; E != Head; E = E->Flink)
    {
        PLDR_MODULE M = CONTAINING_RECORD(E, LDR_MODULE, InLoadOrderLinks);
        if (RtlRawStringCompare(M->Name, Name) == 0)
            return M;
    }
    return NULL;
}

static VOID
LdrpInitializeLoaderData(IN PPEB Peb, IN PVOID VeaSo)
{
    RtlZeroMemory(&LdrpPebLdr, sizeof(LdrpPebLdr));
    LdrpPebLdr.Length = sizeof(PEB_LDR_DATA);
    InitializeListHead(&LdrpPebLdr.InLoadOrderModuleList);
    InitializeListHead(&LdrpPebLdr.InMemoryOrderModuleList);
    InitializeListHead(&LdrpPebLdr.InInitializationOrderModuleList);

    /* EXE utama */
    LdrpExeModule.Base      = Peb->ImageBaseAddress;
    LdrpExeModule.LoadCount = -1;
    LdrpExeModule.Flags     = LDRP_FLAG_EXE;
    LdrpSetModuleName(&LdrpExeModule, "<exe>");
    LdrpInsertModule(&LdrpExeModule);

    /* veaso.so (sudah direlokasi di awal LdrpInitializeProcessMinimal) */
    LdrpVeaSoModule.Base      = VeaSo;
    LdrpVeaSoModule.LoadCount = -1;
    LdrpVeaSoModule.Flags     = LDRP_FLAG_PROCESSED;
    LdrpSetModuleName(&LdrpVeaSoModule, "veaso.so");
    LdrpInsertModule(&LdrpVeaSoModule);

    LdrpPebLdr.Initialized = TRUE;
    Peb->Ldr = &LdrpPebLdr;
}

static ULONG_PTR
VEAPI
LdrpResolveGlobalSymbol(IN PCSTR SymbolName)
{
    PLIST_ENTRY Head = &LdrpPebLdr.InLoadOrderModuleList;
    PLIST_ENTRY E;

    for (E = Head->Flink; E != Head; E = E->Flink)
    {
        PLDR_MODULE M = CONTAINING_RECORD(E, LDR_MODULE, InLoadOrderLinks);
        ULONG_PTR Addr;

        if (M->Flags & LDRP_FLAG_EXE) continue;

        Addr = RtlGetElfSymbolAddress((PELF32_EHDR)M->Base, (ULONG_PTR)M->Base, SymbolName);
        if (Addr) return Addr;
    }

    KdPrintf("[LDR/SYM] unresolved '%s'\n", SymbolName);
    return 0;
}

VEASTATUS
VEAPI
LdrpInitializeProcessMinimal(
    IN PKTRAP_FRAME ContextFrame,
    IN PVOID SysArg1
)
{
    PPEB Peb;
    VEASTATUS Status;

    /* 1. Relokasi internal veaso.so DULUAN sebelum sentuh global var / panggil fungsi apa pun */
    if (SysArg1)
    {
        Status = RtlApplyElfRelocations((PELF32_EHDR)SysArg1, (ULONG_PTR)SysArg1, NULL);
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }
    }

    /* Aman akses global & panggil KdPrintf mulai titik ini */
    VeaSoBase = SysArg1;
    Peb = NtCurrentPeb();
    LdrpInitializeLoaderData(Peb, SysArg1);
    LdrpExeModule.EntryPoint = ContextFrame ? (PVOID)ContextFrame->Eax : NULL;

    /* 2. Init ProcessHeap SEBELUM LdrpLoadDependencies (karena LdrpReadFileImage butuh heap!) */
    if (Peb && !Peb->ProcessHeap)
    {
        Peb->ProcessHeap = RtlCreateHeap(
            HEAP_GROWABLE,
            NULL,
            0x100000,
            0x10000,
            NULL,
            NULL
        );
        if (!Peb->ProcessHeap)
        {
            return STATUS_NO_MEMORY;
        }
    }

    /* 3. Parse .dynamic & muat semua dependensi DT_NEEDED secara rekursif */
    Status = LdrpParseDynamic(&LdrpExeModule);
    if (VEA_SUCCESS(Status))
    {
        Status = LdrpLoadDependencies(&LdrpExeModule);
    }
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* 4. Relokasi semua modul dependensi (leaf-first), lalu relokasi EXE utama sekali saja */
    {
        PLIST_ENTRY E;
        for (E = LdrpPebLdr.InInitializationOrderModuleList.Flink;
             E != &LdrpPebLdr.InInitializationOrderModuleList;
             E = E->Flink)
        {
            PLDR_MODULE M = CONTAINING_RECORD(E, LDR_MODULE, InInitializationOrderLinks);
            if (M->Flags & LDRP_FLAG_PROCESSED) continue;

            Status = RtlApplyElfRelocations((PELF32_EHDR)M->Base,
                                            (ULONG_PTR)M->Base,
                                            LdrpResolveGlobalSymbol);
            if (!VEA_SUCCESS(Status)) return Status;
            M->Flags |= LDRP_FLAG_PROCESSED;
        }

        if (LdrpExeModule.Base)
        {
            Status = RtlApplyElfRelocations((PELF32_EHDR)LdrpExeModule.Base,
                                            (ULONG_PTR)LdrpExeModule.Base,
                                            LdrpResolveGlobalSymbol);
            if (!VEA_SUCCESS(Status)) return Status;
        }
    }

    /* 5. Jalankan .init / .init_array semua .so setelah semua simbol & GOT ter-resolve */
    LdrpRunInitRoutines();

    if (ContextFrame && ContextFrame->Eax != 0)
    {
        ContextFrame->Eip = ContextFrame->Eax;
    }

    return STATUS_SUCCESS;
}

VOID
VEAPI
LdrpInitializeThreadMinimal(
    IN PKTRAP_FRAME Context
)
{
    PTEB Teb = NtCurrentTeb();

    // Reset LastStatus khusus milik thread baru ini
    if (Teb)
    {
        Teb->LastStatusValue = STATUS_SUCCESS;
    }

    // Untuk thread tambahan (bukan main thread), start routine ada di Context->Eax
    if (Context && Context->Eax != 0)
    {
        Context->Eip = Context->Eax;
    }
}

VEASTATUS
VEAPI
LdrpLoadDependency(IN PCSTR Name, OUT PLDR_MODULE *OutMod)
{
    CHAR Path[128];
    PLDR_MODULE Mod;
    ULONG Dl = sizeof(LDRP_DLL_DIR) - 1, Nl = 0;
    VEASTATUS Status;

    /* Sudah termuat? Kalau sedang LOADING berarti siklus: terima saja */
    Mod = LdrpFindModuleByName(Name);
    if (Mod)
    {
        if (Mod->LoadCount != -1) Mod->LoadCount++;
        *OutMod = Mod;
        return STATUS_SUCCESS;
    }

    while (Name[Nl]) Nl++;
    if (Dl + Nl + 1 > sizeof(Path)) return STATUS_NAME_TOO_LONG;
    RtlCopyMemory(Path, LDRP_DLL_DIR, Dl);
    RtlCopyMemory(Path + Dl, (PSTR)Name, Nl + 1);

    Status = LdrpMapModule(Name, Path, &Mod);
    if (!VEA_SUCCESS(Status)) return Status;

    Mod->Flags |= LDRP_FLAG_LOADING;

    Status = LdrpParseDynamic(Mod);
    if (VEA_SUCCESS(Status))
        Status = LdrpLoadDependencies(Mod); 

    Mod->Flags &= ~LDRP_FLAG_LOADING;

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[LDR] gagal memuat '%s': 0x%08X\n", Name, Status);
        return Status;      /* cleanup modul gagal: TODO, lihat catatan */
    }

    /* Post-order: dependensi sudah masuk duluan, jadi daftar init berurutan leaf-first */
    InsertTailList(&LdrpPebLdr.InInitializationOrderModuleList,
                   &Mod->InInitializationOrderLinks);

    *OutMod = Mod;
    return STATUS_SUCCESS;
}

VEASTATUS
LdrpLoadDependencies(IN PLDR_MODULE Mod)
{
    PELF32_DYN D;
    PLDR_MODULE Dep;
    VEASTATUS Status;

    for (D = Mod->Dynamic; D->d_tag != DT_NULL; D++)
    {
        if (D->d_tag != DT_NEEDED) continue;

        Status = LdrpLoadDependency(Mod->StrTab + D->d_un.d_val, &Dep);
        if (!VEA_SUCCESS(Status)) return Status;
    }
    return STATUS_SUCCESS;
}

VOID
VEAPI
LdrpInit(
    IN PKTRAP_FRAME Context,
    IN PVOID SysArg1,
    IN PVOID SysArg2
)
{
    PPEB Peb = NtCurrentPeb();
    LONG Prev = _InterlockedCompareExchange(&LdrpProcessInitialized, 1, 0);

    /* Proses dianggap belum di-init jika Prev == 0 ATAU ProcessHeap di PEB per-proses masih NULL */
    if (Prev == 0 || (Peb && Peb->ProcessHeap == NULL))
    {
        VEASTATUS Status = LdrpInitializeProcessMinimal(Context, SysArg1);
        if (!VEA_SUCCESS(Status))
        {
            KdPrintf("[LDR/INIT] FAILED Status=0x%08X, terminating...\n", Status);
            VzTerminateProcess(NtCurrentProcess(), Status);
        }
    }
    else
    {
        LdrpInitializeThreadMinimal(Context);
    }

    VzContinue(Context, FALSE);

    VzTerminateProcess(NtCurrentProcess(), STATUS_APP_INIT_FAILURE);
}

/* EOF */