/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : queue.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Kernel Queue
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 15-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file queue.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
KsInitializeQueue(
    IN PKQUEUE Queue,
    IN ULONG Count OPTIONAL
)
{
    Queue->Header.Type = QueueObject;
    Queue->Header.Abandoned = 0;
    Queue->Header.Size = sizeof(KQUEUE) / sizeof(ULONG);
    Queue->Header.SignalState = 0;

    InitializeListHead(&Queue->Header.WaitListHead);
    InitializeListHead(&Queue->EntryListHead);
    InitializeListHead(&Queue->ThreadListHead);
    KsInitializeSpinlock((PKSPIN_LOCK)&Queue->Header.Lock);

    Queue->CurrentCount = 0;
    Queue->MaximumCount = Count;
}

PLIST_ENTRY
VEAPI
KsInsertQueue(
    IN PKQUEUE Queue,
    IN PLIST_ENTRY ListEntry
)
{
    KIRQL OldIrql;

    KsAcquireSpinLock((PKSPIN_LOCK)&Queue->Header.Lock, &OldIrql);

    if (!IsListEmpty(&Queue->Header.WaitListHead))
    {
        PLIST_ENTRY WaitEntry = RemoveHeadList(&Queue->Header.WaitListHead);
        PKWAIT_BLOCK WaitBlock = CONTAINING_RECORD(WaitEntry, KWAIT_BLOCK, WaitListEntry);
        PKTHREAD WaitThread = WaitBlock->Thread;

        if ((Queue->MaximumCount == 0) ||
            (Queue->CurrentCount < Queue->MaximumCount))
        {
            InitializeListHead(&WaitBlock->WaitListEntry);
            WaitBlock->Object = NULL;
            WaitBlock->WaitResult = ListEntry;
            WaitBlock->WaitKey = 1;
            WaitThread->WaitStatus = (UCHAR)STATUS_SUCCESS;
            Queue->CurrentCount++;

            KsReleaseSpinLock((PKSPIN_LOCK)&Queue->Header.Lock, OldIrql);
            PtWakeThread(WaitThread);
        }
        else
        {
            InsertHeadList(&Queue->Header.WaitListHead, WaitEntry);
            InsertTailList(&Queue->EntryListHead, ListEntry);
            Queue->Header.SignalState++;
            KsReleaseSpinLock((PKSPIN_LOCK)&Queue->Header.Lock, OldIrql);
        }
    }
    else
    {
        InsertTailList(&Queue->EntryListHead, ListEntry);
        Queue->Header.SignalState++;
        
        KsReleaseSpinLock((PKSPIN_LOCK)&Queue->Header.Lock, OldIrql);
    }

    return ListEntry;
}

PLIST_ENTRY
VEAPI
KsRemoveQueue(
    IN PKQUEUE Queue,
    IN KPROCESSOR_MODE WaitMode,
    IN PLARGE_INTEGER Timeout OPTIONAL
)
{
    PKTHREAD CurrentThread = PtGetCurrentThread();
    PKQUEUE PreviousQueue;
    PKWAIT_BLOCK WaitBlock;
    PKWAIT_BLOCK TimerBlock;
    PKTIMER Timer;

    (VOID)WaitMode;

    if (Queue == NULL || CurrentThread == NULL)
        return NULL;

    /* Get PreviousQueue */
    PreviousQueue = CurrentThread->Queue;

    /* There were still Queue from last operation */
    if (PreviousQueue != NULL)
    {
        KIRQL PreviousIrql;

        /* Lock and remove Queue */
        KsAcquireSpinLock((PKSPIN_LOCK)&PreviousQueue->Header.Lock, &PreviousIrql);
        if (PreviousQueue->CurrentCount > 0)
            PreviousQueue->CurrentCount--;
        CurrentThread->Queue = NULL;
        KsReleaseSpinLock((PKSPIN_LOCK)&PreviousQueue->Header.Lock, PreviousIrql);
    }

    WaitBlock = &CurrentThread->WaitBlock[0];
    TimerBlock = &CurrentThread->WaitBlock[1];
    Timer = &CurrentThread->Timer;

    /* Looping it */
    while (TRUE)
    {
        KIRQL OldIrql;
        KIRQL WaitIrql;
        PLIST_ENTRY Entry;

        WaitIrql = KeGetCurrentIrql();
        KsAcquireSpinLock((PKSPIN_LOCK)&Queue->Header.Lock, &OldIrql);

        /* If List not empty and Queue current count still enough */
           if ((Queue->MaximumCount == 0 ||
               Queue->CurrentCount < Queue->MaximumCount) &&
            !IsListEmpty(&Queue->EntryListHead))
        {
            /* Get Entry */
            Entry = RemoveHeadList(&Queue->EntryListHead);

            /* Decrement SignalState and Increment CurrentCount */
            Queue->Header.SignalState--;
            Queue->CurrentCount++;
            CurrentThread->Queue = Queue;

            /* Return the Entry */
            KsReleaseSpinLock((PKSPIN_LOCK)&Queue->Header.Lock, OldIrql);
            KsCancelTimer(Timer);
            return Entry;
        }

        /* Check for Timeout */
        if (Timeout != NULL && Timeout->QuadPart == 0)
        {
            KsReleaseSpinLock((PKSPIN_LOCK)&Queue->Header.Lock, OldIrql);
            return (PLIST_ENTRY)(ULONG_PTR)STATUS_TIMEOUT;
        }

        /* Initialize WaitBlock */
        InitializeListHead(&WaitBlock->WaitListEntry);
        WaitBlock->Thread = CurrentThread;
        WaitBlock->Object = Queue;
        WaitBlock->WaitResult = NULL;
        WaitBlock->WaitKey = 0;

        /* Initialize TimerBlock */
        InitializeListHead(&TimerBlock->WaitListEntry);
        TimerBlock->Thread = CurrentThread;
        TimerBlock->Object = NULL;
        TimerBlock->WaitResult = NULL;

        /* Set it to CurrentThread*/
        CurrentThread->WaitBlockList = WaitBlock;
        CurrentThread->WaitType = (UCHAR)WaitAny;
        CurrentThread->WaitStatus = (UCHAR)STATUS_SUCCESS;
        CurrentThread->State = Waiting;

        /* Insert Waitblock to queue */
        InsertTailList(
            &Queue->Header.WaitListHead,
            &WaitBlock->WaitListEntry
        );

        /* If timeout specified */
        if (Timeout != NULL)
        {
            KIRQL TimerIrql;

            /* Assign the Timer with TimerBlock */
            KsAcquireSpinLock((PKSPIN_LOCK)&Timer->Header.Lock, &TimerIrql);
            TimerBlock->Object = Timer;
            InsertTailList(
                &Timer->Header.WaitListHead,
                &TimerBlock->WaitListEntry
            );
            KsReleaseSpinLock((PKSPIN_LOCK)&Timer->Header.Lock, TimerIrql);
        }

        KsReleaseSpinLock((PKSPIN_LOCK)&Queue->Header.Lock, OldIrql);

        /* Set the timer here */
        if (Timeout != NULL)
            KsSetTimer(Timer, *Timeout, 0, NULL);

        /* Yield it */
        PtYieldThread();

        /* Get the result of another work */
        Entry = (PLIST_ENTRY)WaitBlock->WaitResult;

        /* There's timer here */
        if (TimerBlock->Object != NULL)
        {
            KIRQL TimerIrql;

            /* Remove TimerBlock's Object */
            KsAcquireSpinLock((PKSPIN_LOCK)&Timer->Header.Lock, &TimerIrql);
            if (TimerBlock->Object != NULL)
            {
                RemoveEntryList(&TimerBlock->WaitListEntry);
                InitializeListHead(&TimerBlock->WaitListEntry);
                TimerBlock->Object = NULL;
            }
            KsReleaseSpinLock((PKSPIN_LOCK)&Timer->Header.Lock, TimerIrql);
        }

        KsAcquireSpinLock((PKSPIN_LOCK)&Queue->Header.Lock, &OldIrql);
        if (WaitBlock->Object != NULL)
        {
            RemoveEntryList(&WaitBlock->WaitListEntry);
            InitializeListHead(&WaitBlock->WaitListEntry);
        }

        if (Entry != NULL && WaitBlock->WaitKey == 0)
            Queue->CurrentCount++;

        if (Entry != NULL)
            CurrentThread->Queue = Queue;
        KsReleaseSpinLock((PKSPIN_LOCK)&Queue->Header.Lock, OldIrql);

        /* Cancel timer */
        KsCancelTimer(Timer);

        /* Remove anything */
        WaitBlock->WaitResult = NULL;
        WaitBlock->Object = NULL;
        WaitBlock->WaitKey = 0;
        TimerBlock->WaitResult = NULL;
        TimerBlock->Object = NULL;
        CurrentThread->WaitBlockList = NULL;
        InitializeListHead(&WaitBlock->WaitListEntry);
        InitializeListHead(&TimerBlock->WaitListEntry);

        if (KeGetCurrentIrql() > WaitIrql)
        {
            KeLowerIrql(WaitIrql);
        }
        else if (KeGetCurrentIrql() < WaitIrql)
        {
            KeRaiseIrql(WaitIrql);
        }

        /* There is entry result */
        if (Entry != NULL)
            return Entry;

        /* If Timeout, return Timeout*/
        if (CurrentThread->WaitStatus == (UCHAR)STATUS_TIMEOUT)
            return (PLIST_ENTRY)(ULONG_PTR)STATUS_TIMEOUT;
    }

}