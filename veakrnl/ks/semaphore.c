/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : semaphore.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Semaphore Object
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file semaphore.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
KsInitializeSemaphore(
    IN PKSEMAPHORE Semaphore,
    IN LONG Count,
    IN LONG Limit
)
{
    Semaphore->Header.Type = SemaphoreObject;
    Semaphore->Header.Size = sizeof(KSEMAPHORE) / sizeof(ULONG);
    Semaphore->Header.SignalState = Count;
    InitializeListHead(&(Semaphore->Header.WaitListHead));
    Semaphore->Limit = Limit;
}

LONG
VEAPI
KsReadStateSemaphore(
    IN PKSEMAPHORE Semaphore
)
{
    ASSERTATION(Semaphore->Header.Type == SemaphoreObject);
    
    return Semaphore->Header.SignalState;
}

LONG
VEAPI
KsReleaseSemaphore(
    IN PKSEMAPHORE Semaphore,
    IN LONG Adjustment,
    IN BOOLEAN Wait
)
{
    LONG InitialState, State;
    KIRQL OldIrql;
    PKTHREAD CurrentThread;

    OldIrql = KsAcquireDispatcherLock();

    InitialState = Semaphore->Header.SignalState;
    State = InitialState + Adjustment;

    if ((Semaphore->Limit < State) || (InitialState > State))
    {
        KsReleaseDispatcherLock(OldIrql);
        /* Todo: RAISE STATUS ERROR */
        KsBugCheckEx(
            INVALID_OBJECT_STATE,
            (ULONG_PTR)SemaphoreObject, 
            (ULONG_PTR)Semaphore, 
            (ULONG_PTR)Semaphore->Limit, 
            (ULONG_PTR)State
        );
    }

    Semaphore->Header.SignalState = State;

    if (!(InitialState) && !(IsListEmpty(&Semaphore->Header.WaitListHead)))
    {
        KsWaitTest(&Semaphore->Header);
    }

    if (Wait == FALSE)
    {
        KsReleaseDispatcherLock(OldIrql);
    }
    else
    {
        CurrentThread = PtGetCurrentThread();
        CurrentThread->WaitNext = TRUE;
        CurrentThread->WaitIrql = OldIrql;
    }

    return InitialState;
}