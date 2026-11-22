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

MESSAGE_QUEUE MsgMessageQueue[MAX_INPUT_QUEUE];
KEVENT MsgInputEvent;
ULONG       MsgQueueHead = 0;
ULONG       MsgQueueTail = 0;
KSPIN_LOCK  MsgQueueLock;
BOOLEAN MsgQueueReady = FALSE;

static VOID
MsgEnqueueMessage(
    IN HWND      hWnd,
    IN ULONG     Message,
    IN ULONG_PTR wParam,
    IN LONG_PTR  lParam,
    IN BOOLEAN   ForceIfFull
)
{
    KIRQL OldIrql;
    ULONG NextTail;

    if (!MsgQueueReady)
    {
        return;
    }

    KsAcquireSpinLock(&MsgQueueLock, &OldIrql);
    NextTail = (MsgQueueTail + 1) % MAX_INPUT_QUEUE;
    if (NextTail != MsgQueueHead)
    {
        MsgMessageQueue[MsgQueueTail].hwnd    = hWnd;
        MsgMessageQueue[MsgQueueTail].message = Message;
        MsgMessageQueue[MsgQueueTail].wParam  = wParam;
        MsgMessageQueue[MsgQueueTail].lParam  = lParam;
        MsgMessageQueue[MsgQueueTail].time    = 0;
        MsgQueueTail = NextTail;
    }
    else if (ForceIfFull)
    {
        /* Jangan pernah drop WM_QUIT walaupun ring buffer penuh */
        ULONG LastSlot = (MsgQueueTail + MAX_INPUT_QUEUE - 1) % MAX_INPUT_QUEUE;
        MsgMessageQueue[LastSlot].hwnd    = hWnd;
        MsgMessageQueue[LastSlot].message = Message;
        MsgMessageQueue[LastSlot].wParam  = wParam;
        MsgMessageQueue[LastSlot].lParam  = lParam;
        MsgMessageQueue[LastSlot].time    = 0;
    }
    KsReleaseSpinLock(&MsgQueueLock, OldIrql);

    KsSetEvent(&MsgInputEvent, FALSE);
}

VOID
VEAPI
MsgInitializeInputQueue(VOID)
{
    KsInitializeSpinlock(&MsgQueueLock);
    KsInitializeEvent(&MsgInputEvent, SynchronizationEvent, FALSE);
    MsgQueueReady = TRUE;
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

    if (!MsgQueueReady)
    {
        return;
    }

    /* Tentukan target HWND dari Desktop yang sedang aktif */
    if (GpActiveDesktop != NULL)
    {
        KsAcquireSpinLock(&GpActiveDesktop->Lock, &OldIrql);
        if (GpActiveDesktop->FocusWindow != NULL)
        {
            PWND_OBJECT FocusWnd = (PWND_OBJECT)GpActiveDesktop->FocusWindow;
            TargetHwnd = FocusWnd->hWnd;
        }
        KsReleaseSpinLock(&GpActiveDesktop->Lock, OldIrql);
    }
    else
    {
        /* Fallback hanya saat boot awal sebelum WindowStation/Desktop dibuat */
        TargetHwnd = IptFocusWindow;
    }

    /* Jangan enqueue input hardware kalau desktop aktif tidak punya window fokus */
    if (!TargetHwnd)
    {
        return;
    }

    MsgEnqueueMessage(TargetHwnd, Message, wParam, lParam, FALSE);
}

VEASTATUS
VEAPI
VeaUserGetMessage(
    OUT PMESSAGE_QUEUE UserMsg,
    IN PVOID hWnd,
    IN ULONG MsgMin,
    IN ULONG MsgMax
)
{
    MESSAGE_QUEUE LocalMsg;
    KIRQL OldIrql;
    BOOLEAN HasMsg = FALSE;
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);

    UNREFERENCED_PARAMETER(hWnd);
    UNREFERENCED_PARAMETER(MsgMin);
    UNREFERENCED_PARAMETER(MsgMax);

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

    while (!HasMsg)
    {
        KsAcquireSpinLock(&MsgQueueLock, &OldIrql);
        if (MsgQueueHead != MsgQueueTail)
        {
            LocalMsg = MsgMessageQueue[MsgQueueHead];
            MsgQueueHead = (MsgQueueHead + 1) % MAX_INPUT_QUEUE;
            HasMsg = TRUE;
        }
        KsReleaseSpinLock(&MsgQueueLock, OldIrql);

        if (!HasMsg)
        {
            HWND PaintHwnd = WndFindPendingPaintWindow((PETHREAD)PtGetCurrentThread());
            if (PaintHwnd != NULL)
            {
                RtlZeroMemory(&LocalMsg, sizeof(MESSAGE_QUEUE));
                LocalMsg.hwnd    = PaintHwnd;
                LocalMsg.message = WM_PAINT;
                HasMsg = TRUE;
            }
        }

        if (!HasMsg)
        {
            KsWaitForSingleObject(&MsgInputEvent);
        }
    }

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

VOID
VEAPI
VeaUserPostQuitMessage(
    IN INT ExitCode
)
{
    /* WM_QUIT selalu punya hwnd = NULL dan wParam = ExitCode */
    MsgEnqueueMessage(NULL, WM_QUIT, (ULONG_PTR)ExitCode, 0, TRUE);
}

/* EOF */