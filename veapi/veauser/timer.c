/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : timer.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Timer Implementation for VEAPI
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 25-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file timer.c
 * --------------------------------------------------------------------- */

/* VARIABLE ***************************************************************/

#define TMR_POOL_ALLOCATE(_Size) UlAllocatePoolWithTag(NonPagedPool, _Size, 'rmTU')
#define TMR_POOL_FREE(_Pointer)  UlFreePoolWithTag(_Pointer, 'rmTU')

static LIST_ENTRY gTimerListHead;
static KSPIN_LOCK gTimerListLock;
static BOOLEAN    gTimerSubsystemReady = FALSE;
static UINT_PTR   gNextThreadTimerId   = 0x1000;

/* PRIVATE ****************************************************************/

static VOID
TimerReference(
    IN PUSER_TIMER Timer
)
{
    if (Timer)
    {
        InterlockedIncrement(&Timer->RefCount);
    }
}

static VOID
TimerDereference(
    IN PUSER_TIMER Timer
)
{
    if (Timer && InterlockedDecrement(&Timer->RefCount) == 0)
    {
        TMR_POOL_FREE(Timer);
    }
}

static BOOLEAN
TimerThreadHasExpiredLocked(
    IN PETHREAD Thread
)
{
    PLIST_ENTRY Entry;

    for (Entry = gTimerListHead.Flink;
         Entry != &gTimerListHead;
         Entry = Entry->Flink)
    {
        PUSER_TIMER Tmr = CONTAINING_RECORD(Entry, USER_TIMER, ListEntry);
        if (!Tmr->Cancelled && Tmr->Thread == Thread && Tmr->Expired)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static VOID
TimerUpdateQueueWakeBitLocked(
    IN PETHREAD            Thread,
    IN PUSER_MESSAGE_QUEUE ExistingQueue OPTIONAL
)
{
    KIRQL QueueIrql;
    BOOLEAN HasExpired = TimerThreadHasExpiredLocked(Thread);
    PUSER_MESSAGE_QUEUE Queue = ExistingQueue
                              ? ExistingQueue
                              : MsgGetThreadQueue(Thread, FALSE);

    if (!Queue)
    {
        return;
    }

    KsAcquireSpinLock(&Queue->Lock, &QueueIrql);
    if (!Queue->Destroyed)
    {
        if (HasExpired)
        {
            Queue->WakeBits |= QS_TIMER;
            KsSetEvent(&Queue->WakeEvent, FALSE);
        }
        else
        {
            Queue->WakeBits &= ~QS_TIMER;
        }
    }
    KsReleaseSpinLock(&Queue->Lock, QueueIrql);

    if (!ExistingQueue)
    {
        MsgDereferenceQueue(Queue);
    }
}

static VOID
VEAPI
TimerDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
    PUSER_TIMER Timer = (PUSER_TIMER)DeferredContext;
    KIRQL OldIrql;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    if (!Timer)
    {
        return;
    }

    KsAcquireSpinLock(&gTimerListLock, &OldIrql);

    /* Abaikan DPC sisa dari arming sebelumnya yang sudah terlanjur masuk DPC queue */
    if (Timer->IgnoreDpcCount > 0)
    {
        Timer->IgnoreDpcCount--;
        KsReleaseSpinLock(&gTimerListLock, OldIrql);
        TimerDereference(Timer);
        return;
    }

    Timer->Armed = FALSE;

    if (!Timer->Cancelled && Timer->Thread != NULL)
    {
        Timer->Expired = TRUE;
        TimerUpdateQueueWakeBitLocked(Timer->Thread, NULL);
    }

    KsReleaseSpinLock(&gTimerListLock, OldIrql);

    TimerDereference(Timer);
}

static VOID
TimerArmLocked(
    IN PUSER_TIMER Timer
)
{
    LARGE_INTEGER DueTime;
    DueTime.QuadPart = -(LONGLONG)Timer->ElapseMs * 10000LL;

    /* Jika timer masih terpasang, batalkan dulu arming sebelumnya */
    if (Timer->Armed)
    {
        if (KsCancelTimer(&Timer->KeTimer))
        {
            TimerDereference(Timer);
        }
        else
        {
            /* DPC lama sudah masuk antrean eksekusi; tandai agar diabaikan */
            Timer->IgnoreDpcCount++;
        }
    }

    Timer->Expired   = FALSE;
    Timer->Cancelled = FALSE;
    Timer->Armed     = TRUE;

    /* Tambah ref untuk DPC baru; kurangi lagi jika mengganti arming lama di queue */
    TimerReference(Timer);
    if (KsSetTimer(&Timer->KeTimer, DueTime, 0, &Timer->Dpc))
    {
        TimerDereference(Timer);
    }
}

/* PUBLIC *****************************************************************/

VOID
VEAPI
TimerInitialize(VOID)
{
    if (!gTimerSubsystemReady)
    {
        InitializeListHead(&gTimerListHead);
        KsInitializeSpinlock(&gTimerListLock);
        gTimerSubsystemReady = TRUE;
    }
}

UINT_PTR
VEAPI
VeaUserSetTimer(
    IN HWND      hWnd,
    IN UINT_PTR  nIDEvent,
    IN UINT      uElapse,
    IN TIMERPROC lpTimerFunc
)
{
    PETHREAD TargetThread = NULL;
    KIRQL OldIrql;
    PLIST_ENTRY Entry;

    if (!gTimerSubsystemReady)
    {
        return 0;
    }

    //
    // Limit the Timer Minimum and Maximum
    //

    if (uElapse < USER_TIMER_MINIMUM)
    {
        uElapse = USER_TIMER_MINIMUM;
    }
    else if (uElapse > USER_TIMER_MAXIMUM)
    {
        uElapse = USER_TIMER_MAXIMUM;
    }

    if (hWnd != NULL)
    {
        //
        // Standard VEAPI: Process should same as the WND Process
        //

        PWND_OBJECT Wnd = WndLookupHandle(hWnd);
        if (!Wnd)
        {
            return 0;
        }

        if (Wnd->OwnerProcess != (PEPROCESS)PtGetCurrentProcess())
        {
            WndDereference(Wnd);
            return 0;
        }

        TargetThread = Wnd->OwnerThread;
        WndDereference(Wnd);

        if (!TargetThread)
        {
            return 0;
        }
    }
    else
    {
        TargetThread = (PETHREAD)PtGetCurrentThread();
    }

    PUSER_MESSAGE_QUEUE Queue = MsgGetThreadQueue(TargetThread, TRUE);
    if (!Queue)
    {
        return 0;
    }
    MsgDereferenceQueue(Queue);

    UINT_PTR NormalizedId = nIDEvent;
    if (hWnd != NULL && NormalizedId == 0)
    {
        NormalizedId = 1;
    }

    KsAcquireSpinLock(&gTimerListLock, &OldIrql);

    if (hWnd != NULL || NormalizedId != 0)
    {
        for (Entry = gTimerListHead.Flink;
             Entry != &gTimerListHead;
             Entry = Entry->Flink)
        {
            PUSER_TIMER Existing = CONTAINING_RECORD(Entry, USER_TIMER, ListEntry);
            if (!Existing->Cancelled &&
                Existing->Thread == TargetThread &&
                Existing->hWnd == hWnd &&
                Existing->nIDEvent == NormalizedId)
            {
                Existing->ElapseMs  = uElapse;
                Existing->TimerProc = lpTimerFunc;
                TimerArmLocked(Existing);
                TimerUpdateQueueWakeBitLocked(TargetThread, NULL);

                KsReleaseSpinLock(&gTimerListLock, OldIrql);
                return NormalizedId;
            }
        }
    }

    UINT_PTR AssignedId = NormalizedId;
    if (hWnd == NULL && AssignedId == 0)
    {
        AssignedId = gNextThreadTimerId++;
        if (gNextThreadTimerId == 0)
        {
            gNextThreadTimerId = 0x1000;
        }
    }

    KsReleaseSpinLock(&gTimerListLock, OldIrql);

    PUSER_TIMER NewTimer = (PUSER_TIMER)TMR_POOL_ALLOCATE(sizeof(USER_TIMER));
    if (!NewTimer)
    {
        return 0;
    }

    RtlZeroMemory(NewTimer, sizeof(USER_TIMER));
    KsInitializeTimer(&NewTimer->KeTimer);
    KsInitializeDpc(&NewTimer->Dpc, TimerDpcRoutine, NewTimer);

    NewTimer->Thread    = TargetThread;
    NewTimer->hWnd      = hWnd;
    NewTimer->nIDEvent  = AssignedId;
    NewTimer->ElapseMs  = uElapse;
    NewTimer->TimerProc = lpTimerFunc;
    NewTimer->RefCount  = 1; 

    KsAcquireSpinLock(&gTimerListLock, &OldIrql);
    InsertTailList(&gTimerListHead, &NewTimer->ListEntry);
    TimerArmLocked(NewTimer);
    KsReleaseSpinLock(&gTimerListLock, OldIrql);

    return AssignedId;
}

BOOLEAN
VEAPI
VeaUserKillTimer(
    IN HWND     hWnd,
    IN UINT_PTR uIDEvent
)
{
    PETHREAD TargetThread = NULL;
    PUSER_TIMER Victim = NULL;
    PLIST_ENTRY Entry;
    KIRQL OldIrql;

    if (!gTimerSubsystemReady)
    {
        return FALSE;
    }

    if (hWnd != NULL)
    {
        PWND_OBJECT Wnd = WndLookupHandle(hWnd);
        if (!Wnd)
        {
            return FALSE;
        }

        if (Wnd->OwnerProcess != (PEPROCESS)PtGetCurrentProcess())
        {
            WndDereference(Wnd);
            return FALSE;
        }

        TargetThread = Wnd->OwnerThread;
        WndDereference(Wnd);
    }
    else
    {
        TargetThread = (PETHREAD)PtGetCurrentThread();
    }

    KsAcquireSpinLock(&gTimerListLock, &OldIrql);

    for (Entry = gTimerListHead.Flink;
         Entry != &gTimerListHead;
         Entry = Entry->Flink)
    {
        PUSER_TIMER Tmr = CONTAINING_RECORD(Entry, USER_TIMER, ListEntry);
        if (!Tmr->Cancelled &&
            Tmr->Thread == TargetThread &&
            Tmr->hWnd == hWnd &&
            Tmr->nIDEvent == uIDEvent)
        {
            RemoveEntryList(&Tmr->ListEntry);
            Tmr->Cancelled = TRUE;
            Tmr->Expired   = FALSE;
            Victim = Tmr;
            break;
        }
    }

    if (Victim != NULL && TargetThread != NULL)
    {
        TimerUpdateQueueWakeBitLocked(TargetThread, NULL);
    }

    KsReleaseSpinLock(&gTimerListLock, OldIrql);

    if (!Victim)
    {
        return FALSE;
    }

    if (KsCancelTimer(&Victim->KeTimer))
    {
        TimerDereference(Victim);
    }

    TimerDereference(Victim);
    return TRUE;
}

BOOLEAN
VEAPI
TimerCheckThreadTimers(
    IN  PETHREAD            Thread,
    IN  PUSER_MESSAGE_QUEUE Queue,
    IN  HWND                FilterHwnd,
    IN  ULONG               MsgMin,
    IN  ULONG               MsgMax,
    IN  BOOLEAN             RemoveMsg,
    OUT PMESSAGE_QUEUE      OutMsg
)
{
    PLIST_ENTRY Entry;
    KIRQL OldIrql;
    BOOLEAN Found = FALSE;

    if (!gTimerSubsystemReady || !Thread || !OutMsg)
    {
        return FALSE;
    }

    if (MsgMin != 0 || MsgMax != 0)
    {
        if (WM_TIMER < MsgMin || WM_TIMER > MsgMax)
        {
            return FALSE;
        }
    }

    KsAcquireSpinLock(&gTimerListLock, &OldIrql);

    for (Entry = gTimerListHead.Flink;
         Entry != &gTimerListHead;
         Entry = Entry->Flink)
    {
        PUSER_TIMER Tmr = CONTAINING_RECORD(Entry, USER_TIMER, ListEntry);
        if (Tmr->Cancelled || Tmr->Thread != Thread || !Tmr->Expired)
        {
            continue;
        }

        if (FilterHwnd == (HWND)-1)
        {
            if (Tmr->hWnd != NULL) continue;
        }
        else if (FilterHwnd != NULL)
        {
            if (Tmr->hWnd != FilterHwnd) continue;
        }

        RtlZeroMemory(OutMsg, sizeof(MESSAGE_QUEUE));
        OutMsg->hwnd    = Tmr->hWnd;
        OutMsg->message = WM_TIMER;
        OutMsg->wParam  = Tmr->nIDEvent;
        OutMsg->lParam  = (LPARAM)Tmr->TimerProc;
        OutMsg->time    = (ULONG)(KsQuerySystemTime() / 10000ULL);

        if (RemoveMsg)
        {
            /* Re-arm otomatis untuk siklus interval berikutnya */
            TimerArmLocked(Tmr);
            TimerUpdateQueueWakeBitLocked(Thread, Queue);
        }

        Found = TRUE;
        break;
    }

    KsReleaseSpinLock(&gTimerListLock, OldIrql);

    return Found;
}

VOID
VEAPI
TimerCleanupWindowTimers(
    IN HWND hWnd
)
{
    LIST_ENTRY VictimsHead;
    PLIST_ENTRY Entry;
    PETHREAD OwnerThread = NULL;
    KIRQL OldIrql;

    if (!gTimerSubsystemReady || !hWnd)
    {
        return;
    }

    InitializeListHead(&VictimsHead);

    KsAcquireSpinLock(&gTimerListLock, &OldIrql);
    Entry = gTimerListHead.Flink;
    while (Entry != &gTimerListHead)
    {
        PLIST_ENTRY Next = Entry->Flink;
        PUSER_TIMER Tmr = CONTAINING_RECORD(Entry, USER_TIMER, ListEntry);

        if (Tmr->hWnd == hWnd)
        {
            RemoveEntryList(&Tmr->ListEntry);
            Tmr->Cancelled = TRUE;
            Tmr->Expired   = FALSE;
            OwnerThread    = Tmr->Thread;
            InsertTailList(&VictimsHead, &Tmr->ListEntry);
        }

        Entry = Next;
    }

    if (OwnerThread != NULL)
    {
        TimerUpdateQueueWakeBitLocked(OwnerThread, NULL);
    }
    KsReleaseSpinLock(&gTimerListLock, OldIrql);

    while (!IsListEmpty(&VictimsHead))
    {
        Entry = RemoveHeadList(&VictimsHead);
        PUSER_TIMER Tmr = CONTAINING_RECORD(Entry, USER_TIMER, ListEntry);

        if (KsCancelTimer(&Tmr->KeTimer))
        {
            TimerDereference(Tmr);
        }
        TimerDereference(Tmr);
    }
}

VOID
VEAPI
TimerCleanupThreadTimers(
    IN PETHREAD Thread
)
{
    LIST_ENTRY VictimsHead;
    PLIST_ENTRY Entry;
    KIRQL OldIrql;

    if (!gTimerSubsystemReady || !Thread)
    {
        return;
    }

    InitializeListHead(&VictimsHead);

    KsAcquireSpinLock(&gTimerListLock, &OldIrql);
    Entry = gTimerListHead.Flink;
    while (Entry != &gTimerListHead)
    {
        PLIST_ENTRY Next = Entry->Flink;
        PUSER_TIMER Tmr = CONTAINING_RECORD(Entry, USER_TIMER, ListEntry);

        if (Tmr->Thread == Thread)
        {
            RemoveEntryList(&Tmr->ListEntry);
            Tmr->Cancelled = TRUE;
            Tmr->Expired   = FALSE;
            InsertTailList(&VictimsHead, &Tmr->ListEntry);
        }

        Entry = Next;
    }
    KsReleaseSpinLock(&gTimerListLock, OldIrql);

    while (!IsListEmpty(&VictimsHead))
    {
        Entry = RemoveHeadList(&VictimsHead);
        PUSER_TIMER Tmr = CONTAINING_RECORD(Entry, USER_TIMER, ListEntry);

        if (KsCancelTimer(&Tmr->KeTimer))
        {
            TimerDereference(Tmr);
        }
        TimerDereference(Tmr);
    }
}

/* EOF */