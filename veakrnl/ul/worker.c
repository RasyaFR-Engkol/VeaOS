/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : worker.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Upper Layer Worker
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 16-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file worker.c
 * --------------------------------------------------------------------- */

/* DEFINE */
#define EX_DELAYED_WORK_THREADS                     12
#define EX_CRITICAL_WORK_THREADS                    8

/* Ul Worker dynamic flag */
#define EX_DYNAMIC_WORK_THREAD                      0x80000000
#define EX_DELAYED_WORK_PRIORITY                    8
#define EX_CRITICAL_WORK_PRIORITY                   16
#define EX_HYPERCRITICAL_WORK_PRIORITY              24

/* VARIABLE */
UL_WORK_QUEUE UlWorkerQueue[MaximumWorkQueue];

VOID
VEAPI
UlpWorkerEntryPoint(PVOID Context)
{
    PUL_WORK_QUEUE_ITEM WorkItem;
    WORK_QUEUE_TYPE WorkType;
    PUL_WORK_QUEUE WorkQueue;
    PLIST_ENTRY QueueEntry;
    PLARGE_INTEGER TimeoutPointer;
    PETHREAD Ethread = (PETHREAD)PtGetCurrentThread();

    DPRINT("UlpWorkerEntryPoint\n");

    KeLowerIrql(PASSIVE_LEVEL);

    /* Check if context is Dynamic */
    if ((ULONG)Context & EX_DYNAMIC_WORK_THREAD)
    {
        /* TODO: Set Timeout pointer */
    }

    /* Get the queue */
    WorkType = (WORK_QUEUE_TYPE)((ULONG)Context & ~EX_DYNAMIC_WORK_THREAD);
    WorkQueue = &UlWorkerQueue[WorkType];

    while (1)
    {
        /* Get the queue */
        QueueEntry = KsRemoveQueue(
            &WorkQueue->WorkerQueue,
            KernelMode,
            NULL
        );

        /* If timeout, break */
        if ((VEASTATUS)(ULONG_PTR)QueueEntry == STATUS_TIMEOUT) break;

        /* Get WorkItem */
        WorkItem = CONTAINING_RECORD(QueueEntry, UL_WORK_QUEUE_ITEM, List);

        /* Call the worker routine */
        WorkItem->WorkerRoutine(WorkItem->Parameter);

        /* Worker must return with PASSIVE_LEVEL */
        if (KeGetCurrentIrql() != PASSIVE_LEVEL)
        {
            /* Stupid people */
            KsBugCheckEx(
                WORKER_BACK_AT_NON_PASSIVE_IRQL,
                (ULONG_PTR)WorkItem->WorkerRoutine,
                KeGetCurrentIrql(),
                (ULONG_PTR)WorkItem->Parameter,
                (ULONG_PTR)WorkItem
            );
        }

        /* TODO: Check if the Thread back while impersonating 
           (We dont have impersonating so not to worry) */
    }

    /* If we're going here, then it means Dynamic Worker Thread */
    return;
}

VOID
VEAPI
UlpCreateWorkerThread(IN WORK_QUEUE_TYPE Type,
                      IN BOOLEAN Dynamic)
{
    PETHREAD Thread;
    HANDLE hThread;
    ULONG Context;
    VEASTATUS Status;

    Context = Type;

    if (Dynamic)
    {
        Context |= EX_DYNAMIC_WORK_THREAD;
    }

    Thread = PtCreateSystemThread(
        (PEPROCESS)PtGetCurrentProcess(),
        UlpWorkerEntryPoint,
        (PVOID)Context,
        NonPagedPool
    );

    if (Thread == NULL)
    {
        DPRINT("ERR: UlWorker failed to initialize.\n");
        return;
    }

    if (Dynamic)
    {
        UlWorkerQueue[Type].DynamicThreadCount++;
    }

    if (Type == CriticalWorkQueue)
    {
        PtSetThreadPriority(&Thread->Tcb, EX_CRITICAL_WORK_PRIORITY);
    }
    else if (Type == HyperCriticalWorkQueue)
    {
        PtSetThreadPriority(&Thread->Tcb, EX_HYPERCRITICAL_WORK_PRIORITY);
    }
    else if (Type == DelayedWorkQueue)
    {
        PtSetThreadPriority(&Thread->Tcb, EX_DELAYED_WORK_PRIORITY);
    }
}

/*
    Reference from ReactOS
*/
VOID
VEAPI
UlpInitializeWorkerThreads(VOID)
{
    ULONG DelayedThreads, CriticalThreads;
    ULONG WorkQueueType;
    /* TODO: StackSwap */

    DelayedThreads = EX_DELAYED_WORK_THREADS;
    CriticalThreads = EX_CRITICAL_WORK_THREADS;

    /* Initialize WorkQueueType */
    for (WorkQueueType = 0;
         WorkQueueType < MaximumWorkQueue;
         WorkQueueType++)
    {
        RtlZeroMemory(&UlWorkerQueue[WorkQueueType], sizeof(UL_WORK_QUEUE));
        KsInitializeQueue(&UlWorkerQueue[WorkQueueType].WorkerQueue, 0);
    }

    /* Now create the worker */
    for (ULONG i = 0; i < CriticalThreads; i++)
    {
        UlpCreateWorkerThread(CriticalWorkQueue, FALSE);
    }

    for (ULONG i = 0; i < DelayedThreads; i++)
    {
        UlpCreateWorkerThread(DelayedWorkQueue, FALSE);
    }

    UlpCreateWorkerThread(HyperCriticalWorkQueue, FALSE);

    /* TODO:
    
        In ReactOS/Windows, Ex Worker has SystemThreadBalancer. The purpose
        itself is detecting deadlock, create dynamic thread, and shutdown
        process of the worker itself. But atp, it doesn't needed
    */
}

VOID
VEAPI
UlQueueWorkItem(
    IN PUL_WORK_QUEUE_ITEM WorkItem,
    IN WORK_QUEUE_TYPE WorkType
)
{
    PUL_WORK_QUEUE WorkQueue = &UlWorkerQueue[WorkType];

    /* TODO: Check for exploit like for example the given WorkItem 
       WorkerRoutine is above MmUserProbeAddress */

    KsInsertQueue(&WorkQueue->WorkerQueue, &WorkItem->List);

    if ((WorkQueue->Info.MakeThreadsAsNecessary) &&
        (!IsListEmpty(&WorkQueue->WorkerQueue.EntryListHead)) &&
        (WorkQueue->WorkerQueue.CurrentCount <
         WorkQueue->WorkerQueue.MaximumCount) &&
        (WorkQueue->DynamicThreadCount < 16))
    {
        /* TODO: Notify with KsSetEvent */
    }
}