/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : gmutex.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Guarded Mutex
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 31-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file gmutex.c
 * --------------------------------------------------------------------- */

VOID
VEAFAST
KsInitializeGuardedMutex(OUT PKGUARDED_MUTEX GuardedMutex)
{
    GuardedMutex->Count = GM_LOCK_BIT;
    GuardedMutex->Owner = NULL;
    GuardedMutex->Contention = 0;

    KsInitializeGate(&GuardedMutex->Gate);
}

VOID
VEAFAST
KsAcquireGuardedMutex(IN PKGUARDED_MUTEX GuardedMutex)
{
    PKTHREAD Thread = PtGetCurrentThread();

    KsEnterGuardedRegionThread(Thread);

    if (!__InterlockedBitTestAndReset(&GuardedMutex->Count, GM_LOCK_BIT_V))
    {
        KsiAcquireGuardedMutex(GuardedMutex);
    }

    GuardedMutex->Owner = Thread;
    GuardedMutex->SpecialApcDisable = Thread->SpecialApcDisable;
}

VOID
VEAFAST
KsReleaseGuardedMutex(IN OUT PKGUARDED_MUTEX GuardedMutex)
{
    PKTHREAD Thread = PtGetCurrentThread();
    LONG OldValue, NewValue;

    GuardedMutex->Owner = NULL;

    OldValue = __InterlockedExchangeAdd(&GuardedMutex->Count, GM_LOCK_BIT);

    if ((OldValue) && !(OldValue & GM_LOCK_WAITER_WOKEN))
    {
        OldValue += GM_LOCK_BIT;

        NewValue = OldValue + GM_LOCK_WAITER_WOKEN -
            GM_LOCK_WAITER_INC;

        if (__InterlockedCompareExchange(&GuardedMutex->Count, NewValue, OldValue))
        {
            KsSignalGate(&GuardedMutex->Gate);
        }
    }

    KsLeaveGuardedRegionThread(Thread);
}

VOID
VEAFAST
KsAcquireGuardedMutexUnsafe(IN OUT PKGUARDED_MUTEX GuardedMutex)
{
    PKTHREAD Thread = PtGetCurrentThread();

    if (!__InterlockedBitTestAndReset(&GuardedMutex->Count, GM_LOCK_BIT_V))
    {
        KsiAcquireGuardedMutex(GuardedMutex);
    }

    GuardedMutex->Owner = Thread;
}

VOID
VEAFAST
KsReleaseGuardedMutexUnsafe(IN OUT PKGUARDED_MUTEX GuardedMutex)
{
    LONG OldValue, NewValue;

    GuardedMutex->Owner = NULL;

    OldValue = __InterlockedExchangeAdd(&GuardedMutex->Count, GM_LOCK_BIT);

    if ((OldValue) && !(OldValue & GM_LOCK_WAITER_WOKEN))
    {
        OldValue += GM_LOCK_BIT;

        NewValue = OldValue + GM_LOCK_WAITER_WOKEN -
            GM_LOCK_WAITER_INC;

        if (__InterlockedCompareExchange(&GuardedMutex->Count,
                                       NewValue,
                                       OldValue) == OldValue)
        {
            KsSignalGate(&GuardedMutex->Gate);
        }
    }
}

BOOLEAN
VEAFAST
KsTryToAcquireGuardedMutex(IN OUT PKGUARDED_MUTEX GuardedMutex)
{
    PKTHREAD Thread = PtGetCurrentThread();

    KsEnterGuardedRegionThread(Thread);

    if (!__InterlockedBitTestAndReset(&GuardedMutex->Count, GM_LOCK_BIT_V))
    {
        KsLeaveGuardedRegionThread(Thread);
        __YieldProcessor();

        return FALSE;
    }

    GuardedMutex->Owner = Thread;
    GuardedMutex->SpecialApcDisable = Thread->SpecialApcDisable;
    return TRUE;
}

/* EOF */