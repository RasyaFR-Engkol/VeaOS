/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : wait.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Wait Purpose
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 15-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file wait.c
 * --------------------------------------------------------------------- */

#define KsiIsNotificationObject(Header) \
    ((Header)->Type == EventNotificationObject || \
     (Header)->Type == ProcessObject || \
     (Header)->Type == ThreadObject)

static VOID
KsiRemoveThreadWaitBlocks(
    IN PKTHREAD Thread
)
{
    ULONG Index;
    PKWAIT_BLOCK WaitBlock;

    for (Index = 0; Index < THREAD_WAIT_OBJECTS; Index++)
    {
        WaitBlock = &Thread->WaitBlock[Index];

        if (WaitBlock->Object != NULL)
        {
        RemoveEntryList(&WaitBlock->WaitListEntry);
        InitializeListHead(&WaitBlock->WaitListEntry);
        WaitBlock->Object = NULL;
        }
    }

    Thread->WaitBlockList = NULL;
}

static BOOLEAN
KsiAreObjectsSignaled(
    IN ULONG ObjectCount,
    IN PVOID *Objects,
    IN KWAIT_TYPE WaitType,
    OUT PULONG SignaledIndex
)
{
    ULONG Index;
    PDISPATCHER_HEADER Header;

    if (WaitType == WaitAny)
    {
        for (Index = 0; Index < ObjectCount; Index++)
        {
        Header = (PDISPATCHER_HEADER)Objects[Index];
        if (Header != NULL && Header->SignalState > 0)
        {
            *SignaledIndex = Index;
            return TRUE;
        }
        }

        return FALSE;
    }

    for (Index = 0; Index < ObjectCount; Index++)
    {
        Header = (PDISPATCHER_HEADER)Objects[Index];
        if (Header == NULL || Header->SignalState <= 0)
        return FALSE;
    }

    *SignaledIndex = 0;
    return TRUE;
}

static VOID
KsiConsumeObjectSignals(
    IN ULONG ObjectCount,
    IN PVOID *Objects,
    IN KWAIT_TYPE WaitType,
    IN ULONG SignaledIndex
)
{
    ULONG Index;
    PDISPATCHER_HEADER Header;

    if (WaitType == WaitAny)
    {
        Header = (PDISPATCHER_HEADER)Objects[SignaledIndex];
        if (!KsiIsNotificationObject(Header))
        {
            Header->SignalState--;
        }
        return;
    }

    for (Index = 0; Index < ObjectCount; Index++)
    {
        Header = (PDISPATCHER_HEADER)Objects[Index];
        if (!KsiIsNotificationObject(Header))
        {
            Header->SignalState--;
        }
    }
}

static VOID
KsiAssignMutantOwner(
    IN PKTHREAD Thread,
    IN PDISPATCHER_HEADER Header
)
{
    PKMUTANT Mutant;

    if (Header->Type != MutantObject)
        return;

    Mutant = (PKMUTANT)Header;
    Mutant->OwnerThread = Thread;
    InsertTailList(&Thread->MutantListHead, &Mutant->MutantListEntry);
}

static BOOLEAN
KsiThreadWaitIsSatisfied(
    IN PKTHREAD Thread,
    IN PDISPATCHER_HEADER SignaledObject,
    OUT PULONG SignaledIndex
)
{
    ULONG Index;
    PKWAIT_BLOCK WaitBlock;
    PDISPATCHER_HEADER Header;

    if (Thread->WaitType == (UCHAR)WaitAny)
    {
        for (Index = 0; Index < THREAD_WAIT_OBJECTS; Index++)
        {
            WaitBlock = &Thread->WaitBlock[Index];
            if (WaitBlock->Object == (PVOID)SignaledObject)
            {
                *SignaledIndex = Index;
                return TRUE;
            }
        }

        return FALSE;
    }

    for (Index = 0; Index < THREAD_WAIT_OBJECTS; Index++)
    {
        WaitBlock = &Thread->WaitBlock[Index];
        if (WaitBlock->Object == NULL)
            break;

        Header = (PDISPATCHER_HEADER)WaitBlock->Object;
        if (Header->SignalState <= 0)
            return FALSE;
    }

    if (Index == 0)
        return FALSE;

    *SignaledIndex = 0;
    return TRUE;
}

static VOID
KsiSatisfyThreadWait(
    IN PKTHREAD Thread,
    IN ULONG SignaledIndex
)
{
    ULONG Index;
    ULONG ObjectCount;
    PVOID Objects[THREAD_WAIT_OBJECTS];
    PKWAIT_BLOCK WaitBlock;

    ObjectCount = 0;
    for (Index = 0; Index < THREAD_WAIT_OBJECTS; Index++)
    {
        WaitBlock = &Thread->WaitBlock[Index];
        if (WaitBlock->Object == NULL)
            break;

        Objects[ObjectCount++] = WaitBlock->Object;
    }

    KsiConsumeObjectSignals(
        ObjectCount,
        Objects,
        (KWAIT_TYPE)Thread->WaitType,
        SignaledIndex
    );

    for (Index = 0; Index < ObjectCount; Index++)
        KsiAssignMutantOwner(Thread, (PDISPATCHER_HEADER)Objects[Index]);

    Thread->WaitStatus = STATUS_SUCCESS;
    Thread->WaitBlock[SignaledIndex].WaitKey = (USHORT)SignaledIndex;
    KsiRemoveThreadWaitBlocks(Thread);
    PtWakeThread(Thread);
}

VOID
VEAPI
KsWaitTest(
    IN PDISPATCHER_HEADER SignaledObject
)
{
    PLIST_ENTRY Entry;
    PLIST_ENTRY NextEntry;
    PKWAIT_BLOCK WaitBlock;
    PKTHREAD Thread;
    ULONG SignaledIndex;
    BOOLEAN NotificationObject;

    if (SignaledObject == NULL)
        return;

    NotificationObject = KsiIsNotificationObject(SignaledObject);
    Entry = SignaledObject->WaitListHead.Flink;

    while (Entry != &SignaledObject->WaitListHead)
    {
        NextEntry = Entry->Flink;
        WaitBlock = CONTAINING_RECORD(Entry, KWAIT_BLOCK, WaitListEntry);
        Thread = WaitBlock->Thread;

        if (Thread != NULL &&
            Thread->State == Waiting &&
            KsiThreadWaitIsSatisfied(Thread, SignaledObject, &SignaledIndex))
        {
            KsiSatisfyThreadWait(Thread, SignaledIndex);

            if (!NotificationObject && SignaledObject->SignalState <= 0)
                break;
        }

        Entry = NextEntry;
    }
}

VEASTATUS
VEAPI
KsWaitForSingleObject(
  IN PVOID Object
)
{
    PVOID Objects[1];

    if (Object == NULL)
        return STATUS_INVALID_PARAMETER;

    Objects[0] = Object;
    return KsWaitForMultipleObjects(1, Objects, WaitAny);
}

VEASTATUS
VEAPI
KsWaitForMultipleObjects(
    IN ULONG ObjectCount,
    IN PVOID *Objects,
    IN KWAIT_TYPE WaitType
)
{
    PKTHREAD CurrentThread;
    PKWAIT_BLOCK WaitBlock;
    ULONG Index;
    ULONG SignaledIndex;
    KIRQL OldIrql;
    KIRQL WaitIrql;
    BOOLEAN WaitNext;

    if (ObjectCount == 0 || ObjectCount > THREAD_WAIT_OBJECTS || Objects == NULL)
        return STATUS_INVALID_PARAMETER;

    if (WaitType != WaitAny && WaitType != WaitAll)
        return STATUS_INVALID_PARAMETER;

    CurrentThread = PtGetCurrentThread();
    if (CurrentThread == NULL)
        return STATUS_INVALID_PARAMETER;

    WaitIrql = KeGetCurrentIrql();

    for (Index = 0; Index < ObjectCount; Index++)
    {
        if (Objects[Index] == NULL)
            return STATUS_INVALID_PARAMETER;
    }

    WaitNext = CurrentThread->WaitNext;
    if (WaitNext)
    {
        OldIrql = CurrentThread->WaitIrql;
        CurrentThread->WaitNext = FALSE;
        CurrentThread->WaitIrql = PASSIVE_LEVEL;
    }
    else
    {
        OldIrql = KsAcquireDispatcherLock();
    }

    if (KsiAreObjectsSignaled(ObjectCount, Objects, WaitType, &SignaledIndex))
    {
        KsiConsumeObjectSignals(ObjectCount, Objects, WaitType, SignaledIndex);
        if (WaitType == WaitAny)
        {
            KsiAssignMutantOwner(
                CurrentThread,
                (PDISPATCHER_HEADER)Objects[SignaledIndex]
            );
        }
        else
        {
            for (Index = 0; Index < ObjectCount; Index++)
                KsiAssignMutantOwner(
                    CurrentThread,
                    (PDISPATCHER_HEADER)Objects[Index]
                );
        }
        KsReleaseDispatcherLock(OldIrql);
        return STATUS_SUCCESS;
    }

    for (Index = 0; Index < ObjectCount; Index++)
    {
        WaitBlock = &CurrentThread->WaitBlock[Index];
        InitializeListHead(&WaitBlock->WaitListEntry);
        WaitBlock->Thread = CurrentThread;
        WaitBlock->Object = Objects[Index];
        WaitBlock->WaitResult = NULL;
        WaitBlock->WaitType = (UCHAR)WaitType;
        InsertTailList(
        &((PDISPATCHER_HEADER)Objects[Index])->WaitListHead,
        &WaitBlock->WaitListEntry
        );
    }

    CurrentThread->WaitBlockList = &CurrentThread->WaitBlock[0];
    CurrentThread->WaitType = (UCHAR)WaitType;
    CurrentThread->WaitStatus = STATUS_PENDING;
    CurrentThread->State = Waiting;

    KsReleaseDispatcherLock(OldIrql);

    PtYieldThread();

    OldIrql = KsAcquireDispatcherLock();
    KsiRemoveThreadWaitBlocks(CurrentThread);

    KsReleaseDispatcherLock(OldIrql);

    /*
     * A thread resumed by the scheduler from a timer/DPC path can inherit
     * DISPATCH_LEVEL. Wait returns at the IRQL where the caller began waiting.
     */
    if (KeGetCurrentIrql() > WaitIrql)
    {
        KeLowerIrql(WaitIrql);
    }
    else if (KeGetCurrentIrql() < WaitIrql)
    {
        KeRaiseIrql(WaitIrql);
    }

    return (VEASTATUS)CurrentThread->WaitStatus;
}
