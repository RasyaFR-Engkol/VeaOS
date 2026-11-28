/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : msg.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Message Queue mechanism
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 20-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file msg.c
 * --------------------------------------------------------------------- */

#define MSG_POOL_ALLOCATE(_Size) UlAllocatePoolWithTag(NonPagedPool, _Size, 'gsMQ')
#define MSG_POOL_FREE(_Pointer) UlFreePoolWithTag(_Pointer, 'gsMQ')

LIST_ENTRY gThreadQueueListHead;
BOOLEAN    gMsgSubsystemReady = FALSE;
KSPIN_LOCK gThreadQueueListLock;

static BOOLEAN
MsgIsRawMouseMessage(
    IN ULONG Message
)
{
    return (Message == WM_MOUSEMOVE ||
            Message == WM_LBUTTONDOWN ||
            Message == WM_LBUTTONUP ||
            Message == WM_RBUTTONDOWN ||
            Message == WM_RBUTTONUP ||
            Message == WM_MBUTTONDOWN ||
            Message == WM_MBUTTONUP);
}

static VOID
MsgTranslateMouseForWindow(
    IN     PWND_OBJECT    TargetWnd,
    IN OUT PMESSAGE_QUEUE Msg
)
{
    LONG ScreenX = (LONG)(SHORT)(Msg->lParam & 0xFFFF);
    LONG ScreenY = (LONG)(SHORT)((Msg->lParam >> 16) & 0xFFFF);

    /* Jika window sedang memegang capture, semua pesan mouse langsung masuk sebagai client */
    if (gCaptureWindow == TargetWnd->hWnd)
    {
        POINT Pt = { ScreenX, ScreenY };
        WndScreenToClient(TargetWnd, &Pt);
        Msg->lParam = ((LONG_PTR)(SHORT)Pt.y << 16) | ((ULONG_PTR)(USHORT)Pt.x);
        return;
    }

    /* Tanya ke WndProc / DefWindowProcA titik kursor kena area apa */
    LRESULT Hit = WndInvokeUserProc(TargetWnd, WM_NCHITTEST, 0, Msg->lParam);

    /* Beri kesempatan WndProc / DefWindowProcA mengganti bentuk kursor */
    LONG_PTR CursorLParam = ((LONG_PTR)(USHORT)Msg->message << 16) | ((ULONG_PTR)(USHORT)Hit);
    WndInvokeUserProc(TargetWnd, WM_SETCURSOR, (WPARAM)TargetWnd->hWnd, CursorLParam);

    /* Jika di dalam Client Area, ubah lParam jadi koordinat relatif client */
    if (Hit == HTCLIENT)
    {
        POINT Pt = { ScreenX, ScreenY };
        WndScreenToClient(TargetWnd, &Pt);
        Msg->lParam = ((LONG_PTR)(SHORT)Pt.y << 16) | ((ULONG_PTR)(USHORT)Pt.x);
        return;
    }

    /* Jika di Non-Client Area, konversi ke WM_NC* dengan wParam = Hit dan lParam = ScreenXY */
    Msg->wParam = (ULONG_PTR)Hit;

    switch (Msg->message)
    {
        case WM_MOUSEMOVE:   Msg->message = WM_NCMOUSEMOVE;   break;
        case WM_LBUTTONDOWN: Msg->message = WM_NCLBUTTONDOWN; break;
        case WM_LBUTTONUP:   Msg->message = WM_NCLBUTTONUP;   break;
        case WM_RBUTTONDOWN: Msg->message = WM_NCRBUTTONDOWN; break;
        case WM_RBUTTONUP:   Msg->message = WM_NCRBUTTONUP;   break;
        case WM_MBUTTONDOWN: Msg->message = WM_NCMBUTTONDOWN; break;
        case WM_MBUTTONUP:   Msg->message = WM_NCMBUTTONUP;   break;
        default: break;
    }
}

static VOID
MsgQueryCurrentCursorPos(
    OUT POINT *Pt
)
{
    if (!Pt)
    {
        return;
    }

    Pt->x = gCursorX;
    Pt->y = gCursorY;
}

static VOID
MsgDereferenceSms(IN PSENT_MESSAGE Sms)
{
    if (Sms && InterlockedDecrement(&Sms->RefCount) == 0)
    {
        MSG_POOL_FREE(Sms);
    }
}

static ULONG
MsgGetWakeBitForMessage(
    IN ULONG   Message,
    IN BOOLEAN IsInput
)
{
    if (!IsInput)
    {
        return QS_POSTMESSAGE;
    }

    if (Message == WM_MOUSEMOVE)
    {
        return QS_MOUSEMOVE;
    }

    if (Message >= WM_KEYFIRST && Message <= WM_KEYLAST)
    {
        return QS_KEY;
    }

    return QS_MOUSEBUTTON;
}

static BOOLEAN
MsgMatchesFilter(
    IN CONST MESSAGE_QUEUE *Msg,
    IN HWND                 FilterHwnd,
    IN ULONG                MsgMin,
    IN ULONG                MsgMax
)
{
    if (FilterHwnd == (HWND)-1)
    {
        /* Filter khusus thread message (hwnd == NULL) */
        if (Msg->hwnd != NULL)
        {
            return FALSE;
        }
    }
    else if (FilterHwnd != NULL)
    {
        if (Msg->hwnd != FilterHwnd)
        {
            return FALSE;
        }
    }

    if (MsgMin != 0 || MsgMax != 0)
    {
        if (Msg->message < MsgMin || Msg->message > MsgMax)
        {
            return FALSE;
        }
    }

    return TRUE;
}

static VOID
MsgRecomputeWakeBitsLocked(
    IN PUSER_MESSAGE_QUEUE Queue
)
{
    PLIST_ENTRY Entry;
    ULONG NewBits = 0;

    if (!IsListEmpty(&Queue->SentListHead))
    {
        NewBits |= QS_SENDMESSAGE;
    }

    if (!IsListEmpty(&Queue->PostListHead))
    {
        NewBits |= QS_POSTMESSAGE;
    }

    for (Entry = Queue->InputListHead.Flink;
         Entry != &Queue->InputListHead;
         Entry = Entry->Flink)
    {
        PQUEUED_MESSAGE Node = CONTAINING_RECORD(Entry, QUEUED_MESSAGE, ListEntry);
        NewBits |= MsgGetWakeBitForMessage(Node->Msg.message, TRUE);
    }

    if (Queue->QuitPosted)
    {
        NewBits |= QS_POSTQUIT;
    }

    /* Pertahankan QS_PAINT jika masih set; dibersihkan saat WndFindPendingPaintWindow kosong */
    NewBits |= (Queue->WakeBits & (QS_PAINT | QS_TIMER));

    Queue->WakeBits = NewBits;
}

static PQUEUED_MESSAGE
MsgPopMatchingFromListLocked(
    IN PUSER_MESSAGE_QUEUE Queue,
    IN PLIST_ENTRY         ListHead,
    IN HWND                FilterHwnd,
    IN ULONG               MsgMin,
    IN ULONG               MsgMax,
    IN BOOLEAN             RemoveMsg
)
{
    PLIST_ENTRY Entry;

    for (Entry = ListHead->Flink; Entry != ListHead; Entry = Entry->Flink)
    {
        PQUEUED_MESSAGE Node = CONTAINING_RECORD(Entry, QUEUED_MESSAGE, ListEntry);
        if (MsgMatchesFilter(&Node->Msg, FilterHwnd, MsgMin, MsgMax))
        {
            if (RemoveMsg)
            {
                RemoveEntryList(&Node->ListEntry);
                if (Queue->MessageCount > 0)
                {
                    Queue->MessageCount--;
                }
                MsgRecomputeWakeBitsLocked(Queue);
            }
            return Node;
        }
    }

    return NULL;
}

static VOID
MsgProcessPendingSentMessages(IN PUSER_MESSAGE_QUEUE Queue)
{
    KIRQL OldIrql;

    while (TRUE)
    {
        KsAcquireSpinLock(&Queue->Lock, &OldIrql);
        if (Queue->Destroyed || IsListEmpty(&Queue->SentListHead))
        {
            MsgRecomputeWakeBitsLocked(Queue);
            KsReleaseSpinLock(&Queue->Lock, OldIrql);
            break;
        }

        PLIST_ENTRY Entry = RemoveHeadList(&Queue->SentListHead);
        PSENT_MESSAGE Sms = CONTAINING_RECORD(Entry, SENT_MESSAGE, ListEntry);
        MsgRecomputeWakeBitsLocked(Queue);
        KsReleaseSpinLock(&Queue->Lock, OldIrql);

        /* Eksekusi WndProc di Ring 3 pada konteks thread target ini */
        LRESULT Res = 0;
        PWND_OBJECT Wnd = WndLookupHandle(Sms->hWnd);
        if (Wnd)
        {
            Res = WndInvokeUserProc(Wnd, Sms->Message, Sms->wParam, Sms->lParam);
            WndDereference(Wnd);
        }

        /* Simpan hasil & bangunkan thread pengirim */
        Sms->Result    = Res;
        Sms->Completed = TRUE;
        KsSetEvent(&Sms->ReplyEvent, FALSE);

        /* Lepas referensi milik queue (pengirim akan melepas referensi miliknya) */
        MsgDereferenceSms(Sms);
    }
}

VOID
MsgReferenceQueue(
    IN PUSER_MESSAGE_QUEUE Queue
)
{
    if (Queue)
    {
        InterlockedIncrement((PLONG)&Queue->RefCount);
    }
}

VOID
MsgDereferenceQueue(
    IN PUSER_MESSAGE_QUEUE Queue
)
{
    if (Queue && InterlockedDecrement((PLONG)&Queue->RefCount) == 0)
    {
        /* Pastikan sisa node di kedua list sudah dibersihkan sebelum free */
        while (!IsListEmpty(&Queue->PostListHead))
        {
            PLIST_ENTRY Entry = RemoveHeadList(&Queue->PostListHead);
            PQUEUED_MESSAGE Node = CONTAINING_RECORD(Entry, QUEUED_MESSAGE, ListEntry);
            MSG_POOL_FREE(Node);
        }

        while (!IsListEmpty(&Queue->InputListHead))
        {
            PLIST_ENTRY Entry = RemoveHeadList(&Queue->InputListHead);
            PQUEUED_MESSAGE Node = CONTAINING_RECORD(Entry, QUEUED_MESSAGE, ListEntry);
            MSG_POOL_FREE(Node);
        }

        MSG_POOL_FREE(Queue);
    }
}

static VEASTATUS
MsgInternalReadMessage(
    OUT PMESSAGE_QUEUE UserMsg,
    IN  PVOID          hWnd,
    IN  ULONG          MsgMin,
    IN  ULONG          MsgMax,
    IN  UINT           RemoveFlags,
    IN  BOOLEAN        Blocking
)
{
    MESSAGE_QUEUE LocalMsg;
    KIRQL OldIrql;
    BOOLEAN HasMsg = FALSE;
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    BOOLEAN DoRemove = ((RemoveFlags & PM_REMOVE) != 0);
    HWND FilterHwnd = (HWND)hWnd;
    PETHREAD CurrentThread = (PETHREAD)PtGetCurrentThread();

    if (IsUser)
    {
        if (!UserMsg ||
            (ULONG_PTR)UserMsg >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)UserMsg + sizeof(MESSAGE_QUEUE)) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return STATUS_ACCESS_VIOLATION;
        }
    }
    else if (!UserMsg)
    {
        return STATUS_INVALID_PARAMETER;
    }

    /* Validasi FilterHwnd jika bukan NULL dan bukan (HWND)-1 */
    if (FilterHwnd != NULL && FilterHwnd != (HWND)-1)
    {
        PWND_OBJECT FilterWnd = WndLookupHandle(FilterHwnd);
        if (!FilterWnd)
        {
            return STATUS_INVALID_HANDLE;
        }

        if (FilterWnd->OwnerThread != CurrentThread)
        {
            WndDereference(FilterWnd);
            return STATUS_INVALID_HANDLE;
        }

        WndDereference(FilterWnd);
    }

    PUSER_MESSAGE_QUEUE Queue = MsgGetThreadQueue(CurrentThread, TRUE);
    if (!Queue)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    while (!HasMsg)
    {
        PQUEUED_MESSAGE PoppedNode = NULL;

        MsgProcessPendingSentMessages(Queue);

        KsAcquireSpinLock(&Queue->Lock, &OldIrql);

        if (Queue->Destroyed)
        {
            KsReleaseSpinLock(&Queue->Lock, OldIrql);
            MsgDereferenceQueue(Queue);
            return STATUS_UNSUCCESSFUL;
        }

        //
        // List of the Priority
        // 1. Posted Message
        // 2. Hardware Input
        // 3. WM_QUIT
        // 4. WM_PAINT
        // 5. WM_TIMER
        //

        //
        // Posted Message
        // 
        PoppedNode = MsgPopMatchingFromListLocked(
            Queue,
            &Queue->PostListHead,
            FilterHwnd,
            MsgMin,
            MsgMax,
            DoRemove
        );

        if (!PoppedNode)
        {
            PoppedNode = MsgPopMatchingFromListLocked(
                Queue,
                &Queue->InputListHead,
                FilterHwnd,
                MsgMin,
                MsgMax,
                DoRemove
            );
        }

        //
        // Posted Message + HW Input
        // 

        if (PoppedNode)
        {
            LocalMsg = PoppedNode->Msg;
            KsReleaseSpinLock(&Queue->Lock, OldIrql);

            if (DoRemove)
            {
                MSG_POOL_FREE(PoppedNode);
            }

            /* Buang pesan stale jika window target sudah di-destroy sebelum sempat diproses */
            if (LocalMsg.hwnd != NULL)
            {
                PWND_OBJECT TargetWnd = WndLookupHandle(LocalMsg.hwnd);
                if (!TargetWnd)
                {
                    if (!DoRemove)
                    {
                        /* Bersihkan node stale dari antrean meski mode PM_NOREMOVE */
                        KsAcquireSpinLock(&Queue->Lock, &OldIrql);
                        PQUEUED_MESSAGE Stale = MsgPopMatchingFromListLocked(
                            Queue, &Queue->PostListHead, LocalMsg.hwnd,
                            LocalMsg.message, LocalMsg.message, TRUE
                        );
                        if (!Stale)
                        {
                            Stale = MsgPopMatchingFromListLocked(
                                Queue, &Queue->InputListHead, LocalMsg.hwnd,
                                LocalMsg.message, LocalMsg.message, TRUE
                            );
                        }
                        KsReleaseSpinLock(&Queue->Lock, OldIrql);
                        if (Stale)
                        {
                            MSG_POOL_FREE(Stale);
                        }
                    }
                    
                    continue;
                }

                if (DoRemove && MsgIsRawMouseMessage(LocalMsg.message))
                {
                    MsgTranslateMouseForWindow(TargetWnd, &LocalMsg);
                }
                
                WndDereference(TargetWnd);
            }

            HasMsg = TRUE;
            break;
        }

        //
        // WM_QUIT
        // 

        if (Queue->QuitPosted)
        {
            MESSAGE_QUEUE QuitMsg;
            RtlZeroMemory(&QuitMsg, sizeof(MESSAGE_QUEUE));
            QuitMsg.hwnd    = NULL;
            QuitMsg.message = WM_QUIT;
            QuitMsg.wParam  = (ULONG_PTR)Queue->QuitExitCode;
            QuitMsg.time    = (ULONG)(KsQuerySystemTime() / 10000ULL);
            MsgQueryCurrentCursorPos(&QuitMsg.pt);

            if (MsgMatchesFilter(&QuitMsg, FilterHwnd, MsgMin, MsgMax))
            {
                LocalMsg = QuitMsg;
                if (DoRemove)
                {
                    Queue->QuitPosted = FALSE;
                    MsgRecomputeWakeBitsLocked(Queue);
                }
                KsReleaseSpinLock(&Queue->Lock, OldIrql);
                HasMsg = TRUE;
                break;
            }
        }

        KsReleaseSpinLock(&Queue->Lock, OldIrql);

        //
        // WM_PAINT
        //

        if (FilterHwnd != (HWND)-1 &&
            ((MsgMin == 0 && MsgMax == 0) || (WM_PAINT >= MsgMin && WM_PAINT <= MsgMax)))
        {
            HWND PaintHwnd = WndFindPendingPaintWindow(CurrentThread);
            if (PaintHwnd != NULL && (FilterHwnd == NULL || FilterHwnd == PaintHwnd))
            {
                RtlZeroMemory(&LocalMsg, sizeof(MESSAGE_QUEUE));
                LocalMsg.hwnd    = PaintHwnd;
                LocalMsg.message = WM_PAINT;
                LocalMsg.time    = (ULONG)(KsQuerySystemTime() / 10000ULL);
                MsgQueryCurrentCursorPos(&LocalMsg.pt);
                HasMsg = TRUE;
                break;
            }
            else if (PaintHwnd == NULL)
            {
                KsAcquireSpinLock(&Queue->Lock, &OldIrql);
                Queue->WakeBits &= ~QS_PAINT;
                KsReleaseSpinLock(&Queue->Lock, OldIrql);
            }
        }

        //
        // WM_TIMER 
        //

        if (TimerCheckThreadTimers(
                CurrentThread,
                Queue,
                FilterHwnd,
                MsgMin,
                MsgMax,
                DoRemove,
                &LocalMsg))
        {
            MsgQueryCurrentCursorPos(&LocalMsg.pt);
            HasMsg = TRUE;
            break;
        }

        if (!Blocking)
        {
            MsgDereferenceQueue(Queue);
            return STATUS_NO_MORE_ENTRIES;
        }

        KsAcquireSpinLock(&Queue->Lock, &OldIrql);
        if (Queue->Destroyed)
        {
            KsReleaseSpinLock(&Queue->Lock, OldIrql);
            MsgDereferenceQueue(Queue);
            return STATUS_UNSUCCESSFUL;
        }

        BOOLEAN StillEmpty = IsListEmpty(&Queue->SentListHead) &&
                     IsListEmpty(&Queue->PostListHead) &&
                     IsListEmpty(&Queue->InputListHead) &&
                     !Queue->QuitPosted &&
                     ((Queue->WakeBits & (QS_PAINT | QS_TIMER)) == 0);
        
        if (StillEmpty)
        {
            KsClearEvent(&Queue->WakeEvent);
            KsReleaseSpinLock(&Queue->Lock, OldIrql);
            KsWaitForSingleObject(&Queue->WakeEvent);
        }
        else
        {
            KsReleaseSpinLock(&Queue->Lock, OldIrql);
        }
    }

    MsgDereferenceQueue(Queue);

    if (IsUser)
    {
        _VEA_TRY
        {
            RtlCopyMemory(UserMsg, &LocalMsg, sizeof(MESSAGE_QUEUE));
        }
        _VEA_EXCEPT(1)
        {
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(UserMsg, &LocalMsg, sizeof(MESSAGE_QUEUE));
    }

    return STATUS_SUCCESS;
}

PUSER_MESSAGE_QUEUE
MsgGetThreadQueue(
    IN PETHREAD Thread,
    IN BOOLEAN  CreateIfMissing
)
{
    PLIST_ENTRY Entry;
    KIRQL OldIrql;
    PUSER_MESSAGE_QUEUE Found = NULL;

    if (!gMsgSubsystemReady || !Thread)
    {
        return NULL;
    }

    KsAcquireSpinLock(&gThreadQueueListLock, &OldIrql);
    for (Entry = gThreadQueueListHead.Flink;
         Entry != &gThreadQueueListHead;
         Entry = Entry->Flink)
    {
        PUSER_MESSAGE_QUEUE Q = CONTAINING_RECORD(Entry, USER_MESSAGE_QUEUE, ThreadListEntry);
        if (Q->Thread == Thread && !Q->Destroyed)
        {
            MsgReferenceQueue(Q);
            Found = Q;
            break;
        }
    }
    KsReleaseSpinLock(&gThreadQueueListLock, OldIrql);

    if (Found || !CreateIfMissing)
    {
        return Found;
    }

    PUSER_MESSAGE_QUEUE NewQ = (PUSER_MESSAGE_QUEUE)MSG_POOL_ALLOCATE(sizeof(USER_MESSAGE_QUEUE));
    if (!NewQ)
    {
        return NULL;
    }

    RtlZeroMemory(NewQ, sizeof(USER_MESSAGE_QUEUE));
    NewQ->Thread   = Thread;
    NewQ->RefCount = 2; /* 1 milik registry list + 1 untuk caller ini */
    KsInitializeSpinlock(&NewQ->Lock);
    KsInitializeEvent(&NewQ->WakeEvent, NotificationEvent, FALSE);
    InitializeListHead(&NewQ->PostListHead);
    InitializeListHead(&NewQ->InputListHead);
    InitializeListHead(&NewQ->SentListHead);

    KsAcquireSpinLock(&gThreadQueueListLock, &OldIrql);
    for (Entry = gThreadQueueListHead.Flink;
         Entry != &gThreadQueueListHead;
         Entry = Entry->Flink)
    {
        PUSER_MESSAGE_QUEUE Existing = CONTAINING_RECORD(Entry, USER_MESSAGE_QUEUE, ThreadListEntry);
        if (Existing->Thread == Thread && !Existing->Destroyed)
        {
            MsgReferenceQueue(Existing);
            KsReleaseSpinLock(&gThreadQueueListLock, OldIrql);
            MSG_POOL_FREE(NewQ);
            return Existing;
        }
    }

    InsertTailList(&gThreadQueueListHead, &NewQ->ThreadListEntry);
    KsReleaseSpinLock(&gThreadQueueListLock, OldIrql);

    return NewQ;
}

VOID
VEAPI
MsgDestroyThreadQueue(
    IN PETHREAD Thread
)
{
    PLIST_ENTRY Entry;
    KIRQL OldIrql;
    PUSER_MESSAGE_QUEUE Victim = NULL;

    if (!gMsgSubsystemReady || !Thread)
    {
        return;
    }

    KsAcquireSpinLock(&gThreadQueueListLock, &OldIrql);
    for (Entry = gThreadQueueListHead.Flink;
         Entry != &gThreadQueueListHead;
         Entry = Entry->Flink)
    {
        PUSER_MESSAGE_QUEUE Q = CONTAINING_RECORD(Entry, USER_MESSAGE_QUEUE, ThreadListEntry);
        if (Q->Thread == Thread && !Q->Destroyed)
        {
            RemoveEntryList(&Q->ThreadListEntry);
            Q->Destroyed = TRUE;
            Victim = Q;
            break;
        }
    }
    KsReleaseSpinLock(&gThreadQueueListLock, OldIrql);

    if (Victim)
    {
        KsAcquireSpinLock(&Victim->Lock, &OldIrql);
        while (!IsListEmpty(&Victim->SentListHead))
        {
            PLIST_ENTRY Entry = RemoveHeadList(&Victim->SentListHead);
            PSENT_MESSAGE Sms = CONTAINING_RECORD(Entry, SENT_MESSAGE, ListEntry);
            Sms->Result    = 0;
            Sms->Completed = TRUE;
            KsSetEvent(&Sms->ReplyEvent, FALSE);
            MsgDereferenceSms(Sms);
        }
        KsReleaseSpinLock(&Victim->Lock, OldIrql);

        MsgDereferenceQueue(Victim);
    }
}

BOOLEAN
MsgEnqueueThreadMessage(
    IN PETHREAD  TargetThread,
    IN HWND      hWnd,
    IN ULONG     Message,
    IN ULONG_PTR wParam,
    IN LONG_PTR  lParam,
    IN BOOLEAN   IsInput
)
{
    KIRQL OldIrql;
    PUSER_MESSAGE_QUEUE Queue;
    PQUEUED_MESSAGE Node;
    ULONG WakeBit;

    if (!gMsgSubsystemReady || !TargetThread)
    {
        return FALSE;
    }

    Queue = MsgGetThreadQueue(TargetThread, TRUE);
    if (!Queue)
    {
        return FALSE;
    }

    WakeBit = MsgGetWakeBitForMessage(Message, IsInput);

    /* Fast-path: coalesce WM_MOUSEMOVE tanpa alokasi pool */
    if (IsInput && Message == WM_MOUSEMOVE)
    {
        KsAcquireSpinLock(&Queue->Lock, &OldIrql);
        if (!Queue->Destroyed && !IsListEmpty(&Queue->InputListHead))
        {
            PQUEUED_MESSAGE TailNode = CONTAINING_RECORD(
                Queue->InputListHead.Blink,
                QUEUED_MESSAGE,
                ListEntry
            );

            if (TailNode->Msg.message == WM_MOUSEMOVE && TailNode->Msg.hwnd == hWnd)
            {
                TailNode->Msg.wParam = wParam;
                TailNode->Msg.lParam = lParam;
                MsgQueryCurrentCursorPos(&TailNode->Msg.pt);
                Queue->WakeBits |= WakeBit;
                KsSetEvent(&Queue->WakeEvent, FALSE);
                KsReleaseSpinLock(&Queue->Lock, OldIrql);

                MsgDereferenceQueue(Queue);
                return TRUE;
            }
        }
        KsReleaseSpinLock(&Queue->Lock, OldIrql);
    }

    /* Baru alokasi node jika memang perlu antrean baru */
    Node = (PQUEUED_MESSAGE)MSG_POOL_ALLOCATE(sizeof(QUEUED_MESSAGE));
    if (!Node)
    {
        MsgDereferenceQueue(Queue);
        return FALSE;
    }

    RtlZeroMemory(Node, sizeof(QUEUED_MESSAGE));
    Node->Msg.hwnd    = hWnd;
    Node->Msg.message = Message;
    Node->Msg.wParam  = wParam;
    Node->Msg.lParam  = lParam;
    Node->Msg.time    = (ULONG)(KsQuerySystemTime() / 10000ULL);
    MsgQueryCurrentCursorPos(&Node->Msg.pt);

    KsAcquireSpinLock(&Queue->Lock, &OldIrql);

    if (Queue->Destroyed)
    {
        KsReleaseSpinLock(&Queue->Lock, OldIrql);
        MSG_POOL_FREE(Node);
        MsgDereferenceQueue(Queue);
        return FALSE;
    }

    /* Batasi kuota per thread agar tidak menghabiskan pool kernel */
    if (Queue->MessageCount >= MAX_THREAD_MESSAGES)
    {
        KsReleaseSpinLock(&Queue->Lock, OldIrql);
        MSG_POOL_FREE(Node);
        MsgDereferenceQueue(Queue);
        return FALSE;
    }

    if (IsInput)
    {
        InsertTailList(&Queue->InputListHead, &Node->ListEntry);
    }
    else
    {
        InsertTailList(&Queue->PostListHead, &Node->ListEntry);
    }

    Queue->MessageCount++;
    Queue->WakeBits |= WakeBit;
    KsSetEvent(&Queue->WakeEvent, FALSE);

    KsReleaseSpinLock(&Queue->Lock, OldIrql);
    MsgDereferenceQueue(Queue);
    return TRUE;
}

VOID
VEAPI
MsgInitializeInputQueue(VOID)
{
    InitializeListHead(&gThreadQueueListHead);
    KsInitializeSpinlock(&gThreadQueueListLock);
    gMsgSubsystemReady = TRUE;
}

BOOLEAN
VEAPI
MsgPostToWindow(
    IN HWND      hWnd,
    IN ULONG     Message,
    IN ULONG_PTR wParam,
    IN LONG_PTR  lParam,
    IN BOOLEAN   IsInput
)
{
    PWND_OBJECT Wnd;
    PETHREAD TargetThread;

    if (!hWnd)
    {
        return FALSE;
    }

    Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return FALSE;
    }

    TargetThread = Wnd->OwnerThread;
    WndDereference(Wnd);

    if (!TargetThread)
    {
        return FALSE;
    }

    return MsgEnqueueThreadMessage(TargetThread, hWnd, Message, wParam, lParam, IsInput);
}

VOID
VEAPI
MsgPostInputMessage(
    IN ULONG     Message,
    IN ULONG_PTR wParam,
    IN LONG_PTR  lParam
)
{
    KIRQL OldIrql;
    HWND TargetHwnd = NULL;
    PDESKTOP_OBJECT Desktop = GpActiveDesktop;

    if (!gMsgSubsystemReady)
    {
        return;
    }

    if (Desktop != NULL)
    {
        KsAcquireSpinLock(&Desktop->Lock, &OldIrql);
        if (Desktop->FocusWindow != NULL)
        {
            PWND_OBJECT FocusWnd = (PWND_OBJECT)Desktop->FocusWindow;
            if (FocusWnd->Magic == WND_OBJECT_MAGIC && FocusWnd->Used)
            {
                TargetHwnd = FocusWnd->hWnd;
            }
        }
        KsReleaseSpinLock(&Desktop->Lock, OldIrql);
    }
    else
    {
        TargetHwnd = IptFocusWindow;
    }

    if (!TargetHwnd)
    {
        return;
    }

    (VOID)MsgPostToWindow(TargetHwnd, Message, wParam, lParam, TRUE);
}

VEASTATUS
VEAPI
VeaUserGetMessage(
    OUT PMESSAGE_QUEUE UserMsg,
    IN  PVOID          hWnd,
    IN  ULONG          MsgMin,
    IN  ULONG          MsgMax
)
{
    return MsgInternalReadMessage(UserMsg, hWnd, MsgMin, MsgMax, PM_REMOVE, TRUE);
}

VEASTATUS
VEAPI
VeaUserPeekMessage(
    OUT PMESSAGE_QUEUE UserMsg,
    IN  PVOID          hWnd,
    IN  ULONG          MsgMin,
    IN  ULONG          MsgMax,
    IN  UINT           wRemoveMsg
)
{
    BOOLEAN RemoveMsg = ((wRemoveMsg & PM_REMOVE) != 0) ? TRUE : FALSE;
    return MsgInternalReadMessage(UserMsg, hWnd, MsgMin, MsgMax, RemoveMsg, FALSE);
}

BOOLEAN
VEAPI
VeaUserPostMessage(
    IN HWND   hWnd,
    IN UINT   Msg,
    IN WPARAM wParam,
    IN LPARAM lParam
)
{
    /* Standar Win32: hWnd == NULL berarti post thread-message ke thread pemanggil */
    if (hWnd == NULL)
    {
        return MsgEnqueueThreadMessage(
            (PETHREAD)PtGetCurrentThread(),
            NULL,
            Msg,
            wParam,
            lParam,
            FALSE
        );
    }

    return MsgPostToWindow(hWnd, Msg, wParam, lParam, FALSE);
}

VOID
VEAPI
VeaUserPostQuitMessage(
    IN INT ExitCode
)
{
    KIRQL OldIrql;
    PUSER_MESSAGE_QUEUE Queue = MsgGetThreadQueue((PETHREAD)PtGetCurrentThread(), TRUE);

    if (!Queue)
    {
        return;
    }

    KsAcquireSpinLock(&Queue->Lock, &OldIrql);
    if (!Queue->Destroyed)
    {
        Queue->QuitPosted   = TRUE;
        Queue->QuitExitCode = ExitCode;
        Queue->WakeBits    |= QS_POSTQUIT;
        KsSetEvent(&Queue->WakeEvent, FALSE);
    }
    KsReleaseSpinLock(&Queue->Lock, OldIrql);

    MsgDereferenceQueue(Queue);
}

VOID
VEAPI
MsgNotifyPaint(
    IN PETHREAD TargetThread
)
{
    KIRQL OldIrql;
    PUSER_MESSAGE_QUEUE Queue = MsgGetThreadQueue(TargetThread, FALSE);

    if (!Queue)
    {
        return;
    }

    KsAcquireSpinLock(&Queue->Lock, &OldIrql);
    if (!Queue->Destroyed)
    {
        Queue->WakeBits |= QS_PAINT;
        KsSetEvent(&Queue->WakeEvent, FALSE);
    }
    KsReleaseSpinLock(&Queue->Lock, OldIrql);

    MsgDereferenceQueue(Queue);
}

VOID
VEAPI
MsgNotifyPaintAll(VOID)
{
    PLIST_ENTRY Entry;
    KIRQL ListIrql, QueueIrql;

    if (!gMsgSubsystemReady)
    {
        return;
    }

    KsAcquireSpinLock(&gThreadQueueListLock, &ListIrql);
    for (Entry = gThreadQueueListHead.Flink;
         Entry != &gThreadQueueListHead;
         Entry = Entry->Flink)
    {
        PUSER_MESSAGE_QUEUE Q = CONTAINING_RECORD(Entry, USER_MESSAGE_QUEUE, ThreadListEntry);
        if (!Q->Destroyed)
        {
            KsAcquireSpinLock(&Q->Lock, &QueueIrql);
            Q->WakeBits |= QS_PAINT;
            KsSetEvent(&Q->WakeEvent, FALSE);
            KsReleaseSpinLock(&Q->Lock, QueueIrql);
        }
    }
    KsReleaseSpinLock(&gThreadQueueListLock, ListIrql);
}

LRESULT
MsgSendMessageToThread(
    IN PETHREAD  TargetThread,
    IN HWND      hWnd,
    IN ULONG     Message,
    IN ULONG_PTR wParam,
    IN LONG_PTR  lParam
)
{
    KIRQL OldIrql;
    PUSER_MESSAGE_QUEUE TargetQueue = MsgGetThreadQueue(TargetThread, TRUE);
    if (!TargetQueue)
    {
        return 0;
    }

    PSENT_MESSAGE Sms = (PSENT_MESSAGE)MSG_POOL_ALLOCATE(sizeof(SENT_MESSAGE));
    if (!Sms)
    {
        MsgDereferenceQueue(TargetQueue);
        return 0;
    }

    RtlZeroMemory(Sms, sizeof(SENT_MESSAGE));
    Sms->hWnd      = hWnd;
    Sms->Message   = Message;
    Sms->wParam    = wParam;
    Sms->lParam    = lParam;
    Sms->RefCount  = 2; /* 1 milik caller ini, 1 milik TargetQueue */
    KsInitializeEvent(&Sms->ReplyEvent, NotificationEvent, FALSE);

    KsAcquireSpinLock(&TargetQueue->Lock, &OldIrql);
    if (TargetQueue->Destroyed)
    {
        KsReleaseSpinLock(&TargetQueue->Lock, OldIrql);
        MSG_POOL_FREE(Sms);
        MsgDereferenceQueue(TargetQueue);
        return 0;
    }

    InsertTailList(&TargetQueue->SentListHead, &Sms->ListEntry);
    TargetQueue->WakeBits |= QS_SENDMESSAGE;
    KsSetEvent(&TargetQueue->WakeEvent, FALSE);
    KsReleaseSpinLock(&TargetQueue->Lock, OldIrql);

    MsgDereferenceQueue(TargetQueue);

    /* Tidur sampai thread target selesai mengeksekusi WndProc (atau mati) */
    KsWaitForSingleObject(&Sms->ReplyEvent);

    LRESULT Result = Sms->Result;
    MsgDereferenceSms(Sms);
    return Result;
}

/* EOF */