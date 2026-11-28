/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : procsup.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : CSR Process Support
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <csr.h>

/* Revision History ------------------------------------------------------
 * DATE       : 28-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file procsup.c
 * --------------------------------------------------------------------- */

/* VARIABLE ***************************************************************/

LIST_ENTRY gProcessListHead;
static volatile int gProcTableLock = 0;

/* PRIVATE ****************************************************************/

static VOID CsrpLockProcesses(VOID)
{
    while (__sync_lock_test_and_set(&gProcTableLock, 1))
    {
        VzYieldExecution();
    }
}

static VOID CsrpUnlockProcesses(VOID)
{
    __sync_lock_release(&gProcTableLock);
}

/* PUBLIC *****************************************************************/

VOID
VEAPI
CsrpInitializeProcessList(VOID)
{
    InitializeListHead(&gProcessListHead);
}

PCSR_PROCESS
VEAPI
CsrpAllocateProcess(
    IN HANDLE ProcessId,
    IN HANDLE ClientCommPort
)
{
    PCSR_PROCESS Process = RtlAllocateHeap(RtlGetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CSR_PROCESS));
    if (!Process)
    {
        return NULL;
    }

    Process->ProcessId      = ProcessId;
    Process->ClientCommPort = ClientCommPort;
    Process->ShutdownLevel  = 0x280;
    Process->ReferenceCount = 1;
    InitializeListHead(&Process->ThreadList);

    CsrpLockProcesses();
    InsertTailList(&gProcessListHead, &Process->ListLink);
    CsrpUnlockProcesses();

    return Process;
}

VOID
VEAPI
CsrpRemoveProcess(
    IN HANDLE ProcessId
)
{
    PLIST_ENTRY NextEntry;
    PCSR_PROCESS Target = NULL;

    CsrpLockProcesses();
    for (NextEntry = gProcessListHead.Flink;
         NextEntry != &gProcessListHead;
         NextEntry = NextEntry->Flink)
    {
        PCSR_PROCESS Current = CONTAINING_RECORD(NextEntry, CSR_PROCESS, ListLink);
        if (Current->ProcessId == ProcessId)
        {
            RemoveEntryList(&Current->ListLink);
            Target = Current;
            break;
        }
    }
    CsrpUnlockProcesses();

    if (Target)
    {
        for (ULONG s = 0; s < gServerDllCapacity; s++)
        {
            if (gServerDlls[s] != NULL && gServerDlls[s]->DisconnectRoutine != NULL)
            {
                gServerDlls[s]->DisconnectRoutine(Target);
            }
        }

        if (Target->ClientCommPort)
        {
            VzClose(Target->ClientCommPort);
        }

        if (Target->ProcessHandle)
        {
            VzClose(Target->ProcessHandle);
            Target->ProcessHandle = NULL;
        }

        RtlFreeHeap(RtlGetProcessHeap(), 0, Target);
        KdPrintf("[CSRSRV] Client with PID=0x%X removed from list.\n\r", ProcessId);
    }
}

PCSR_PROCESS
VEAPI
CsrLocateProcessById(HANDLE ProcessId)
{
    PLIST_ENTRY NextEntry;
    PCSR_PROCESS Found = NULL;

    CsrpLockProcesses();
    for (NextEntry = gProcessListHead.Flink;
         NextEntry != &gProcessListHead;
         NextEntry = NextEntry->Flink)
    {
        PCSR_PROCESS Current = CONTAINING_RECORD(NextEntry, CSR_PROCESS, ListLink);
        if (Current->ProcessId == ProcessId)
        {
            Found = Current;
            break;
        }
    }
    CsrpUnlockProcesses();

    return Found;
}

/* EOF */