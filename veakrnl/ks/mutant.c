/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : mutant.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : KMUTANT
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file mutant.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
KsInitializeMutant
(
    IN PKMUTANT Mutant,
    IN BOOLEAN InitialOwner
)
{
    PKTHREAD Thread;
    KIRQL OldIrql;

    InitializeListHead(&Mutant->MutantListEntry);
    InitializeListHead(&Mutant->Header.WaitListHead);

    if (InitialOwner)
    {
        Thread = PtGetCurrentThread();
        Mutant->OwnerThread = Thread;

        OldIrql = KsAcquireDispatcherLock();

        InsertTailList(&Thread->MutantListHead,
                       &Mutant->MutantListEntry);

        KsReleaseDispatcherLock(OldIrql);
    }
    else
    {
        Mutant->OwnerThread = NULL;
    }
    
    Mutant->Header.Type = MutantObject;
    Mutant->Header.Size = sizeof(KMUTANT) / sizeof(ULONG);
    Mutant->Header.SignalState = InitialOwner ? 0 : 1;
    Mutant->Abandoned = FALSE;
    Mutant->ApcDisable = 0;
}

VOID
VEAPI
KsInitializeMutex(
    IN PKMUTEX Mutex, 
    IN ULONG Level
)
{
    InitializeListHead(&Mutex->MutantListEntry);
    InitializeListHead(&Mutex->Header.WaitListHead);
    Mutex->Header.Type = MutantObject;
    Mutex->Header.Size = sizeof(KMUTEX) / sizeof(ULONG);
    Mutex->Header.SignalState = 1;
    Mutex->OwnerThread = NULL;
    Mutex->Abandoned = FALSE;
    Mutex->ApcDisable = 1;
}

LONG
VEAPI
KeReadStateMutant(IN PKMUTANT Mutant)
{
    return Mutant->Header.SignalState;
}

static LONG
KsiReleaseMutant(
    IN PKMUTANT Mutant,
    IN BOOLEAN Wait
)
{
    PKTHREAD CurrentThread;
    KIRQL OldIrql;
    LONG PreviousState;

    if (Mutant == NULL || Mutant->Header.Type != MutantObject)
        return STATUS_INVALID_PARAMETER;

    CurrentThread = PtGetCurrentThread();
    if (CurrentThread == NULL)
        return STATUS_INVALID_PARAMETER;

    OldIrql = KsAcquireDispatcherLock();

    if (Mutant->OwnerThread != CurrentThread)
    {
        KsReleaseDispatcherLock(OldIrql);
        return STATUS_ACCESS_DENIED;
    }

    PreviousState = Mutant->Header.SignalState;
    RemoveEntryList(&Mutant->MutantListEntry);
    InitializeListHead(&Mutant->MutantListEntry);
    Mutant->OwnerThread = NULL;
    Mutant->Header.SignalState = 1;

    if (!IsListEmpty(&Mutant->Header.WaitListHead))
        KsWaitTest(&Mutant->Header);

    if (Wait)
    {
        CurrentThread->WaitNext = TRUE;
        CurrentThread->WaitIrql = OldIrql;
    }
    else
    {
        KsReleaseDispatcherLock(OldIrql);
    }

    return PreviousState;
}

LONG
VEAPI
KsReleaseMutant(
    IN PKMUTANT Mutant,
    IN BOOLEAN Wait
)
{
    return KsiReleaseMutant(Mutant, Wait);
}

LONG
VEAPI
KsReleaseMutex(
    IN PKMUTEX Mutex,
    IN BOOLEAN Wait
)
{
    return KsiReleaseMutant((PKMUTANT)Mutex, Wait);
}

