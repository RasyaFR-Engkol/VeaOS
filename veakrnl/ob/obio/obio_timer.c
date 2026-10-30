/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_timer.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : IO Timer
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 28-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_timer.c
 * --------------------------------------------------------------------- */

/* VARIABLE */

KSPIN_LOCK ObioTimerLock;
LIST_ENTRY ObioTimerQueueHead;
KDPC ObioTimerDpc;
KTIMER ObioTimer;
ULONG ObioTimerCount = 0;

VOID
VEAPI
ObioTimerDispatch(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
    KIRQL OldIrql;
    PLIST_ENTRY TimerEntry;
    PIO_TIMER Timer;
    ULONG i;

    if (ObioTimerCount)
    {
        KsAcquireSpinLock(&ObioTimerLock, &OldIrql);

        for (TimerEntry = ObioTimerQueueHead.Flink, i = ObioTimerCount;
            (TimerEntry != &ObioTimerQueueHead) && i;
            TimerEntry = TimerEntry->Flink)
        {
            /* Get the timer and check if it's enabled */
            Timer = CONTAINING_RECORD(TimerEntry, IO_TIMER, IoTimerList);
            if (Timer->TimerEnabled)
            {
                /* Call the timer routine */
                Timer->TimerRoutine(Timer->DeviceObject, Timer->Context);
                i--;
            }
        }

        KsReleaseSpinLock(&ObioTimerLock, OldIrql);
    }
}

VOID
VEAPI
ObioRemoveTimerFromTimerList(IN PIO_TIMER Timer)
{
    KIRQL OldIrql;

    KsAcquireSpinLock(&ObioTimerLock, &OldIrql);

    RemoveEntryList(&Timer->IoTimerList);
    if (Timer->TimerEnabled) ObioTimerCount--;

    KsReleaseSpinLock(&ObioTimerLock, OldIrql);
}

VEASTATUS
VEAPI
ObioInitializeIoTimer(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIO_TIMER_ROUTINE TimerRoutine,
    IN PVOID Context
)
{
    PIO_TIMER IoTimer = DeviceObject->Timer;

    if (!IoTimer)
    {
        IoTimer = UlAllocatePoolWithTag(
            NonPagedPool,
            sizeof(IO_TIMER),
            TAG_IO_TIMER
        );
        if (!IoTimer) return STATUS_INSUFFICIENT_RESOURCES;

        RtlZeroMemory(IoTimer, sizeof(IO_TIMER));
        IoTimer->Type = IO_TYPE_TIMER;
        IoTimer->DeviceObject = DeviceObject;
        DeviceObject->Timer = IoTimer;
    }

    IoTimer->TimerRoutine = TimerRoutine;
    IoTimer->Context = Context;

    UlInterlockedInsertTailList(
        &ObioTimerQueueHead,
        &IoTimer->IoTimerList,
        &ObioTimerLock
    );

    return STATUS_SUCCESS;
}

VOID
VEAPI
ObioStartTimer(IN PDEVICE_OBJECT DeviceObject)
{
    KIRQL OldIrql;
    PIO_TIMER IoTimer = DeviceObject->Timer;

    if (!IoTimer->TimerEnabled)
    {
        KsAcquireSpinLock(&ObioTimerLock, &OldIrql);

        IoTimer->TimerEnabled = TRUE;
        ObioTimerCount++;
        DPRINT("Driver %p started timer\n");

        KsReleaseSpinLock(&ObioTimerLock, OldIrql);
    }
}
 
VOID
VEAPI
ObioStopTimer(IN PDEVICE_OBJECT DeviceObject)
{
    KIRQL OldIrql;
    PIO_TIMER IoTimer = DeviceObject->Timer;

    KsAcquireSpinLock(&ObioTimerLock, &OldIrql);

    if (IoTimer->TimerEnabled)
    {
        IoTimer->TimerEnabled = FALSE;
        ObioTimerCount--;
    }

    KsReleaseSpinLock(&ObioTimerLock, OldIrql);
}

/* EOF */