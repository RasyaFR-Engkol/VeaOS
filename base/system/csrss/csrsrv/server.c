/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : server.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : CSR Server
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <csr.h>

/* Revision History ------------------------------------------------------
 * DATE       : 28-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file server.c
 * --------------------------------------------------------------------- */

/* VARIABLE ***************************************************************/

PCSR_SERVER_DLL *gServerDlls = NULL;
ULONG gServerDllCapacity = 0;

/* PRIVATE ****************************************************************/

static VEASTATUS
CsrpEnsureServerDllCapacity(
    IN ULONG RequiredCount
)
{
    ULONG NewCap;
    PCSR_SERVER_DLL *NewTable;
    PVOID Heap = RtlGetProcessHeap();

    if (RequiredCount <= gServerDllCapacity)
    {
        return STATUS_SUCCESS;
    }

    NewCap = (gServerDllCapacity == 0) ? 4 : gServerDllCapacity;
    while (NewCap < RequiredCount)
    {
        NewCap *= 2;
    }

    NewTable = (PCSR_SERVER_DLL *)RtlAllocateHeap(
        Heap,
        HEAP_ZERO_MEMORY,
        NewCap * sizeof(PCSR_SERVER_DLL)
    );
    if (!NewTable)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (gServerDlls)
    {
        RtlCopyMemory(
            NewTable,
            gServerDlls,
            gServerDllCapacity * sizeof(PCSR_SERVER_DLL)
        );
        RtlFreeHeap(Heap, 0, gServerDlls);
    }

    gServerDlls = NewTable;
    gServerDllCapacity = NewCap;
    return STATUS_SUCCESS;
}

/* PUBLIC *****************************************************************/

VEASTATUS
VEAPI
CsrRegisterServerDll(
    IN PCSR_SERVER_DLL ServerDll
)
{
    VEASTATUS Status;
    PCSR_SERVER_DLL Entry;
    PVOID Heap = RtlGetProcessHeap();

    if (!ServerDll)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = CsrpEnsureServerDllCapacity((ULONG)ServerDll->ServerId + 1);
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    Entry = gServerDlls[ServerDll->ServerId];
    if (!Entry)
    {
        Entry = (PCSR_SERVER_DLL)RtlAllocateHeap(Heap, HEAP_ZERO_MEMORY, sizeof(CSR_SERVER_DLL));
        if (!Entry)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        gServerDlls[ServerDll->ServerId] = Entry;
    }

    *Entry = *ServerDll;
    InitializeListHead(&Entry->ListLink);

    KdPrintf("[CSRSRV] Registered on Server DLL [%u]: %s (%lu API)\n\r",
             ServerDll->ServerId, ServerDll->Name, ServerDll->ApiCount);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
CsrLoadServerDll(
    IN PCHAR DllPath,
    IN PCHAR EntryPointName
)
{
    ANSI_STRING DllName;
    ANSI_STRING ProcName;
    PVOID ModuleHandle = NULL;
    PCSR_SERVER_DLL_INIT_ROUTINE InitRoutine = NULL;
    VEASTATUS Status;

    if (!DllPath || !EntryPointName)
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlInitAnsiString(&DllName, DllPath);

    Status = LdrLoadDll(NULL, NULL, &DllName, &ModuleHandle);
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[CSRSRV] LdrLoadDll('%s') gagal: 0x%08X\n\r", DllPath, Status);
        return Status;
    }

    RtlInitAnsiString(&ProcName, EntryPointName);
    Status = LdrGetProcedureAddress(ModuleHandle, &ProcName, 0, (PVOID *)&InitRoutine);
    if (!VEA_SUCCESS(Status) || !InitRoutine)
    {
        KdPrintf("[CSRSRV] EntryPoint '%s' tidak ditemukan di '%s': 0x%08X\n\r",
                 EntryPointName, DllPath, Status);
        return Status;
    }

    Status = InitRoutine();
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[CSRSRV] %s() gagal: 0x%08X\n\r", EntryPointName, Status);
        return Status;
    }

    return STATUS_SUCCESS;
}

/* EOF */