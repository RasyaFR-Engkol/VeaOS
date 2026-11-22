/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : critsect.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : RTL Critical Section
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <rtl.h>
#include "rtl2.h"
#include <ptfunc.h>

/* Revision History ------------------------------------------------------
 * DATE       : 15-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file critsect.c
 * --------------------------------------------------------------------- */

/* SHIM: sesuaikan dengan VeaOS ------------------------------------------ */

#define RTLP_CS_CURRENT_THREAD()    ((HANDLE)NtCurrentTeb()->ClientId.UniqueThread)
#define RTLP_CS_YIELD()             VeaYieldExecution()
#define RTLP_CS_PAUSE()             __asm__ __volatile__("pause" ::: "memory")
#define RTLP_CS_READ_LOCKCOUNT(Cs)  (*(volatile LONG *)&(Cs)->LockCount)

#ifndef InterlockedIncrement
#define InterlockedIncrement(p)                 __sync_add_and_fetch((p), 1)
#define InterlockedDecrement(p)                 __sync_sub_and_fetch((p), 1)
#define InterlockedCompareExchange(p, x, c)     __sync_val_compare_and_swap((p), (c), (x))
#define InterlockedExchange(p, v)               __sync_lock_test_and_set((p), (v))
#endif

/* Wait object: event auto-reset (seperti NT) disimpan di LockSemaphore.
 * Definisikan RTL_CS_NO_EVENT kalau syscall event belum ada: LockSemaphore
 * dipakai sebagai flag handoff, dan waiter menunggu dengan yield-loop. */

#define RTL_CS_NO_EVENT 1

#ifndef RTL_CS_NO_EVENT

static
VEASTATUS
RtlpCsGetEvent(IN OUT PRTL_CRITICAL_SECTION CriticalSection, OUT PHANDLE Event)
{
    HANDLE NewEvent;
    HANDLE OldEvent;
    VEASTATUS Status;

    if (CriticalSection->LockSemaphore != NULL)
    {
        *Event = CriticalSection->LockSemaphore;
        return STATUS_SUCCESS;
    }

    Status = VeaCreateEvent(&NewEvent, EVENT_ALL_ACCESS, NULL, SynchronizationEvent, FALSE);
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* Publish once; the loser of the race closes its own event */
    OldEvent = (HANDLE)InterlockedCompareExchange((PLONG)&CriticalSection->LockSemaphore,
                                                  (LONG)NewEvent, 0);
    if (OldEvent != NULL)
    {
        VeaClose(NewEvent);
        NewEvent = OldEvent;
    }
    *Event = NewEvent;
    return STATUS_SUCCESS;
}

static
VEASTATUS
RtlpCsPrepare(IN OUT PRTL_CRITICAL_SECTION CriticalSection)
{
    HANDLE Event;

    return RtlpCsGetEvent(CriticalSection, &Event);
}

static
VOID
RtlpCsWaitForOwnership(IN OUT PRTL_CRITICAL_SECTION CriticalSection)
{
    HANDLE Event;
    VEASTATUS Status;

    Status = RtlpCsGetEvent(CriticalSection, &Event);
    if (VEA_SUCCESS(Status))
    {
        Status = VeaWaitForSingleObject(Event, FALSE, NULL);
    }
    if (!VEA_SUCCESS(Status))
    {
        RtlRaiseStatus(Status);
    }
}

static
VOID
RtlpCsSignalWaiter(IN OUT PRTL_CRITICAL_SECTION CriticalSection)
{
    HANDLE Event;
    VEASTATUS Status;

    Status = RtlpCsGetEvent(CriticalSection, &Event);
    if (VEA_SUCCESS(Status))
    {
        Status = VeaSetEvent(Event, NULL);
    }
    if (!VEA_SUCCESS(Status))
    {
        RtlRaiseStatus(Status);
    }
}

static
VOID
RtlpCsDestroy(IN OUT PRTL_CRITICAL_SECTION CriticalSection)
{
    if (CriticalSection->LockSemaphore != NULL)
    {
        VeaClose(CriticalSection->LockSemaphore);
    }
}

#else /* RTL_CS_NO_EVENT */

static
VEASTATUS
RtlpCsPrepare(IN OUT PRTL_CRITICAL_SECTION CriticalSection)
{
    return STATUS_SUCCESS;
}

static
VOID
RtlpCsWaitForOwnership(IN OUT PRTL_CRITICAL_SECTION CriticalSection)
{
    while (InterlockedCompareExchange((PLONG)&CriticalSection->LockSemaphore, 0, 1) != 1)
    {
        RTLP_CS_YIELD();
    }
}

static
VOID
RtlpCsSignalWaiter(IN OUT PRTL_CRITICAL_SECTION CriticalSection)
{
    InterlockedExchange((PLONG)&CriticalSection->LockSemaphore, 1);
}

static
VOID
RtlpCsDestroy(IN OUT PRTL_CRITICAL_SECTION CriticalSection)
{
}

#endif /* RTL_CS_NO_EVENT */

/* Spin up to SpinCount times; TRUE = lock acquired */
static
BOOLEAN
RtlpCsSpinAcquire(IN OUT PRTL_CRITICAL_SECTION CriticalSection)
{
    ULONG_PTR Count;
    LONG Current;

    for (Count = CriticalSection->SpinCount; Count > 0; Count--)
    {
        Current = RTLP_CS_READ_LOCKCOUNT(CriticalSection);
        if (Current > 0)
        {
            break;                      /* ada waiter lain, jangan spin */
        }
        if (Current == -1 &&
            InterlockedCompareExchange(&CriticalSection->LockCount, 0, -1) == -1)
        {
            CriticalSection->OwningThread = RTLP_CS_CURRENT_THREAD();
            CriticalSection->RecursionCount = 1;
            return TRUE;
        }
        RTLP_CS_PAUSE();
    }
    return FALSE;
}

/* PUBLIC ---------------------------------------------------------------- */

VEASTATUS
VEAPI
RtlInitializeCriticalSectionAndSpinCount(IN OUT PRTL_CRITICAL_SECTION CriticalSection,
                                         IN ULONG SpinCount)
{
    CriticalSection->DebugInfo = NULL;      /* belum ada tracking */
    CriticalSection->LockCount = -1;
    CriticalSection->RecursionCount = 0;
    CriticalSection->OwningThread = NULL;
    CriticalSection->LockSemaphore = NULL;
    CriticalSection->SpinCount = SpinCount & RTL_CS_SPIN_MASK;

    if (SpinCount & RTL_CS_PREALLOCATE_EVENT)
    {
        return RtlpCsPrepare(CriticalSection);
    }
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
RtlInitializeCriticalSection(IN OUT PRTL_CRITICAL_SECTION CriticalSection)
{
    return RtlInitializeCriticalSectionAndSpinCount(CriticalSection, 0);
}

VEASTATUS
VEAPI
RtlDeleteCriticalSection(IN OUT PRTL_CRITICAL_SECTION CriticalSection)
{
    RtlpCsDestroy(CriticalSection);

    CriticalSection->DebugInfo = NULL;
    CriticalSection->LockCount = -1;
    CriticalSection->RecursionCount = -1;
    CriticalSection->OwningThread = NULL;
    CriticalSection->LockSemaphore = NULL;
    CriticalSection->SpinCount = 0;
    return STATUS_SUCCESS;
}

BOOLEAN
VEAPI
RtlTryEnterCriticalSection(IN OUT PRTL_CRITICAL_SECTION CriticalSection)
{
    HANDLE Thread = RTLP_CS_CURRENT_THREAD();

    if (InterlockedCompareExchange(&CriticalSection->LockCount, 0, -1) == -1)
    {
        CriticalSection->OwningThread = Thread;
        CriticalSection->RecursionCount = 1;
        return TRUE;
    }

    if (CriticalSection->OwningThread == Thread)
    {
        InterlockedIncrement(&CriticalSection->LockCount);
        CriticalSection->RecursionCount++;
        return TRUE;
    }
    return FALSE;
}

VEASTATUS
VEAPI
RtlEnterCriticalSection(IN OUT PRTL_CRITICAL_SECTION CriticalSection)
{
    HANDLE Thread = RTLP_CS_CURRENT_THREAD();

    if (CriticalSection->SpinCount != 0)
    {
        if (RtlTryEnterCriticalSection(CriticalSection) ||
            RtlpCsSpinAcquire(CriticalSection))
        {
            return STATUS_SUCCESS;
        }
    }

    if (InterlockedIncrement(&CriticalSection->LockCount) != 0)
    {
        if (CriticalSection->OwningThread == Thread)
        {
            CriticalSection->RecursionCount++;      /* recursive entry */
            return STATUS_SUCCESS;
        }

        /* Returns only when ownership has been handed to us */
        RtlpCsWaitForOwnership(CriticalSection);
    }

    CriticalSection->OwningThread = Thread;
    CriticalSection->RecursionCount = 1;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
RtlLeaveCriticalSection(IN OUT PRTL_CRITICAL_SECTION CriticalSection)
{
    CriticalSection->RecursionCount--;

    if (CriticalSection->RecursionCount > 0)
    {
        InterlockedDecrement(&CriticalSection->LockCount);
        return STATUS_SUCCESS;
    }

    if (CriticalSection->RecursionCount < 0)
    {
        CriticalSection->RecursionCount = 0;        /* Leave tanpa Enter */
        return STATUS_UNSUCCESSFUL;
    }

    CriticalSection->OwningThread = NULL;
    if (InterlockedDecrement(&CriticalSection->LockCount) >= 0)
    {
        RtlpCsSignalWaiter(CriticalSection);        /* ada waiter: serahkan ownership */
    }
    return STATUS_SUCCESS;
}

/* EOF */