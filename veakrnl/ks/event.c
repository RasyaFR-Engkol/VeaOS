/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : event.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : KEVENT related file
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 16-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file event.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
KsClearEvent(PKEVENT Event)
{
    Event->Header.SignalState = FALSE;
}

VOID
VEAPI
KsInitializeEvent(
    IN OUT PKEVENT Event,
    IN EVENT_TYPE Type,
    IN BOOLEAN State
)
{
    ASSERTATION((Type == NotificationEvent) || (Type == SynchronizationEvent));
    Event->Header.Type = EventNotificationObject + Type;
    Event->Header.Signaling = FALSE;
    Event->Header.Size = sizeof(KEVENT) / sizeof(ULONG);
    Event->Header.SignalState = State;
    InitializeListHead(&(Event->Header.WaitListHead));
}

LONG
VEAPI
KsResetEvent(
    IN PKEVENT Event
)
{
    KIRQL OldIrql;
    LONG PreviousState;
    
    OldIrql = KsAcquireDispatcherLock();

    PreviousState = Event->Header.SignalState;

    Event->Header.SignalState = 0;

    KsReleaseDispatcherLock(OldIrql);

    return PreviousState;
}

LONG
VEAPI
KsSetEvent(
    IN PKEVENT Event,
    IN BOOLEAN Wait
)
{
    KIRQL OldIrql;
    LONG PreviousState;
    PKTHREAD Thread;

    /* if Type is event notification and is singaled and we dont need to wait */
    if ((Event->Header.Type == EventNotificationObject) &&
        (Event->Header.SignalState == 1) &&
        !(Wait))
    {
        /* It's been signaled (RACE) */
        return TRUE;
    }

    /* Lock */
    OldIrql = KsAcquireDispatcherLock();

    /* Save previous state */
    PreviousState = Event->Header.SignalState;

    /* Signaled */
    Event->Header.SignalState = TRUE;

    /* Is there any waiter? */
    if (!(PreviousState) && !(IsListEmpty(&Event->Header.WaitListHead)))
    {
        KsWaitTest(&Event->Header);
    }

    if (!Wait)
    {
        KsReleaseDispatcherLock(OldIrql);
    }
    else
    {
        Thread = PtGetCurrentThread();
        Thread->WaitNext = TRUE;
        Thread->WaitIrql = OldIrql;
    }

    /* Return it */
    return PreviousState;
}

