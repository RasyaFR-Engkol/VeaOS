/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : thread.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : OSI386 Section
   PURPOSE     : Making initial thread and process to put in block boot
 ----- Effective since 07-2026 til FOREVER ------------------------------- */

#include <osi386.h>

/* Revision History ------------------------------------------------------
 * DATE       : 26-07-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file thread.c
 * --------------------------------------------------------------------- */

BOOLEAN
VEAPI
LdrCreateInitialThreadAndProcess(IN OUT BLOCK_BOOT_2 *BlockBoot2)
{
    /* Make Process*/
    PKPROCESS Process = (PKPROCESS)LmAllocatePool(LdrUnreclaimablePool, sizeof(KPROCESS));
    if(!Process)
    {
        return FALSE;
    }

    // Initialize Process
    LdrInitializeSpinLock(&Process->ProcessLock);
    Process->BasePriority = 0;
    Process->DirectoryTableBase = (ULONG_PTR)GET_PAGE_DIR();
    Process->State = Running;
    InitializeListHead(&Process->ThreadListHead);
    Process->Token = 0; // No Token

    // We can safely attach this to BLOCK_BOOT_2
    BlockBoot2->Process = (ULONG_PTR)Process;

    /* Now make thread, attach to the process */
    PKTHREAD Thread = (PKTHREAD)LmAllocatePool(LdrUnreclaimablePool, sizeof(KTHREAD));
    if(!Thread)
    {
        return FALSE;
    }

    Thread->ApcState.Process = Process;
    Thread->State = Running;
    Thread->Priority = 0;
    Thread->Peb = NULL;
    Thread->VRuntime = 0;
    Thread->Weight = 1024;
    InitializeListHead(&Thread->WaitListEntry);

    /* Make kernel stack now*/
    ULONG StackSize = 8192; // 8KB
    PVOID StackBase = LmAllocatePool(LdrUnreclaimablePool, StackSize);
    if (!StackBase)
    {
        return FALSE;
    }

    Thread->StackLimit = StackBase;
    Thread->InitialStack = (PVOID)((ULONG_PTR)StackBase + StackSize);

    // Save this data
    Thread->TrapFrame = NULL;
    Thread->KernelStack = Thread->InitialStack;

    // Connect Thread to Process
    InsertTailList(&Process->ThreadListHead, &Thread->WaitListEntry);

    BlockBoot2->Thread = (ULONG_PTR)Thread;
    BlockBoot2->KernelStack = (ULONG_PTR)Thread->KernelStack;

    return TRUE;
}