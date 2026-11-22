/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : paint.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Vea GDI Painting 
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file paint.c
 * --------------------------------------------------------------------- */

#define WND_BORDER_WIDTH    2
#define WND_TITLEBAR_HEIGHT 24

static VOID
WndGetClientOriginOnScreen(
    IN PWND_OBJECT Wnd,
    OUT PLONG ScreenX,
    OUT PLONG ScreenY
)
{
    LONG X = 0;
    LONG Y = 0;
    PWND_OBJECT Curr = Wnd;

    while (Curr)
    {
        X += Curr->X;
        Y += Curr->Y;

        if (!(Curr->Style & WS_CHILD))
        {
            /* Top-level window punya border & titlebar */
            X += WND_BORDER_WIDTH;
            Y += WND_TITLEBAR_HEIGHT;
            break;
        }

        Curr = WndLookupHandle(Curr->hWndParent);
    }

    *ScreenX = X;
    *ScreenY = Y;
}

HWND
VEAPI
WndFindPendingPaintWindow(
    IN PETHREAD Thread
)
{
    PLIST_ENTRY Entry;
    KIRQL OldIrql;
    HWND FoundHwnd = NULL;

    WndEnsureListsInitialized();

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    for (Entry = gWindowListHead.Flink; Entry != &gWindowListHead; Entry = Entry->Flink)
    {
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
        if (Wnd->Magic == WND_OBJECT_MAGIC &&
            Wnd->Used &&
            Wnd->NeedsPaint &&
            (Wnd->Style & WS_VISIBLE) &&
            Wnd->OwnerThread == Thread)
        {
            FoundHwnd = Wnd->hWnd;
            break;
        }
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    return FoundHwnd;
}

HDC
VEAPI
VeaUserBeginPaint(
    IN HWND hWnd,
    OUT PPAINTSTRUCT lpPaint
)
{
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    PAINTSTRUCT LocalPs;
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    BOOLEAN IsChild;
    LONG ClientX;
    LONG ClientY;
    ULONG ClientW;
    ULONG ClientH;

    if (!Wnd || !lpPaint)
    {
        return NULL;
    }

    if (IsUser)
    {
        if ((ULONG_PTR)lpPaint >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)lpPaint + sizeof(PAINTSTRUCT)) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return NULL;
        }
    }

    Wnd->NeedsPaint = FALSE;
    IsChild = ((Wnd->Style & WS_CHILD) != 0);

    if (!IsChild)
    {
        /* Non-client area hanya untuk top-level window */
        EngFillRect32(&GpPrimaryDisplay.PrimarySurface, Wnd->X, Wnd->Y, Wnd->Width, Wnd->Height, 0xFF404040);
        EngFillRect32(
            &GpPrimaryDisplay.PrimarySurface,
            Wnd->X + WND_BORDER_WIDTH,
            Wnd->Y + WND_BORDER_WIDTH,
            Wnd->Width - (2 * WND_BORDER_WIDTH),
            WND_TITLEBAR_HEIGHT - WND_BORDER_WIDTH,
            0xFF000080
        );

        ClientX = Wnd->X + WND_BORDER_WIDTH;
        ClientY = Wnd->Y + WND_TITLEBAR_HEIGHT;
        ClientW = (Wnd->Width > 2 * WND_BORDER_WIDTH) ? (Wnd->Width - 2 * WND_BORDER_WIDTH) : 0;
        ClientH = (Wnd->Height > WND_TITLEBAR_HEIGHT + WND_BORDER_WIDTH)
                  ? (Wnd->Height - WND_TITLEBAR_HEIGHT - WND_BORDER_WIDTH) : 0;

        EngFillRect32(&GpPrimaryDisplay.PrimarySurface, ClientX, ClientY, ClientW, ClientH, 0xFFC0C0C0);

        WndInvalidateChildWindows(hWnd);
    }
    else
    {
        /* WS_CHILD: hitung posisi layar dari parent, tanpa titlebar */
        WndGetClientOriginOnScreen(Wnd, &ClientX, &ClientY);
        ClientW = Wnd->Width;
        ClientH = Wnd->Height;
    }

    HDC hdc = GdiCreateWindowDC(hWnd, ClientX, ClientY, ClientW, ClientH);
    if (!hdc)
    {
        return NULL;
    }

    RtlZeroMemory(&LocalPs, sizeof(PAINTSTRUCT));
    LocalPs.hdc            = hdc;
    LocalPs.fErase         = TRUE;
    LocalPs.rcPaint.left   = 0;
    LocalPs.rcPaint.top    = 0;
    LocalPs.rcPaint.right  = (LONG)ClientW;
    LocalPs.rcPaint.bottom = (LONG)ClientH;

    if (IsUser)
    {
        _VEA_TRY
        {
            RtlCopyMemory(lpPaint, &LocalPs, sizeof(PAINTSTRUCT));
        }
        _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
        {
            GdiReleaseWindowDC(hdc);
            return NULL;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(lpPaint, &LocalPs, sizeof(PAINTSTRUCT));
    }

    return hdc;
}

BOOLEAN
VEAPI
VeaUserEndPaint(
    IN HWND hWnd,
    IN CONST PAINTSTRUCT *lpPaint
)
{
    PAINTSTRUCT LocalPs;
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);

    UNREFERENCED_PARAMETER(hWnd);

    if (!lpPaint)
    {
        return FALSE;
    }

    if (IsUser)
    {
        if ((ULONG_PTR)lpPaint >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)lpPaint + sizeof(PAINTSTRUCT)) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return FALSE;
        }

        _VEA_TRY
        {
            RtlCopyMemory(&LocalPs, (PVOID)lpPaint, sizeof(PAINTSTRUCT));
        }
        _VEA_EXCEPT(1)
        {
            return FALSE;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(&LocalPs, (PVOID)lpPaint, sizeof(PAINTSTRUCT));
    }

    GdiReleaseWindowDC(LocalPs.hdc);
    return TRUE;
}

BOOLEAN
VEAPI
VeaUserInvalidateRect(
    IN HWND hWnd,
    IN CONST PRECT lpRect OPTIONAL,
    IN BOOLEAN bErase
)
{
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    UNREFERENCED_PARAMETER(lpRect);
    UNREFERENCED_PARAMETER(bErase);

    if (!Wnd)
    {
        return FALSE;
    }

    Wnd->NeedsPaint = TRUE;
    KsSetEvent(&MsgInputEvent, FALSE);
    return TRUE;
}

/* EOF */