/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : winapi.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Window API
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 24-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file winapi.c
 * --------------------------------------------------------------------- */

/* PUBLIC *****************************************************************/

INT
VEAPI
VeaUserGetSystemMetrics(
    IN INT nIndex
)
{
    ULONG ScrW = GpPrimaryDisplay.PrimarySurface.Width;
    ULONG ScrH = GpPrimaryDisplay.PrimarySurface.Height;
    if (ScrW == 0) ScrW = 800;
    if (ScrH == 0) ScrH = 600;

    switch (nIndex)
    {
        case SM_CXSCREEN:
        case SM_CXFULLSCREEN:
            return (INT)ScrW;

        case SM_CYSCREEN:
        case SM_CYFULLSCREEN:
            return (INT)ScrH;

        case SM_CXBORDER:
        case SM_CYBORDER:
            return WND_BORDER_WIDTH;

        case SM_CYCAPTION:
            return WND_TITLEBAR_HEIGHT;

        default:
            return 0;
    }
}

LONG
VEAPI
VeaUserGetWindowLong(
    IN HWND hWnd,
    IN INT  nIndex
)
{
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return 0;
    }

    LONG Result = 0;
    switch (nIndex)
    {
        case GWL_WNDPROC:
            Result = (LONG)(ULONG_PTR)Wnd->lpfnWndProc;
            break;

        case GWL_HINSTANCE:
            Result = (LONG)(ULONG_PTR)Wnd->hInstance;
            break;

        case GWL_HWNDPARENT:
            Result = (LONG)(ULONG_PTR)Wnd->hWndParent;
            break;

        case GWL_ID:
            Result = (LONG)(ULONG_PTR)Wnd->hMenu;
            break;

        case GWL_STYLE:
            Result = (LONG)Wnd->Style;
            break;

        case GWL_EXSTYLE:
            Result = (LONG)Wnd->ExStyle;
            break;

        case GWL_USERDATA:
            Result = (LONG)Wnd->UserData;
            break;

        default:
            break;
    }

    WndDereference(Wnd);
    return Result;
}

LONG
VEAPI
VeaUserSetWindowLong(
    IN HWND hWnd,
    IN INT  nIndex,
    IN LONG dwNewLong
)
{
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    KIRQL ListIrql;
    if (!Wnd)
    {
        return 0;
    }

    if (Wnd->OwnerProcess != (PEPROCESS)PtGetCurrentProcess())
    {
        WndDereference(Wnd);
        return 0;
    }

    LONG OldVal = 0;
    BOOLEAN VisualChanged = FALSE;

    KsAcquireSpinLock(&gWindowListLock, &ListIrql);
    switch (nIndex)
    {
        case GWL_WNDPROC:
            OldVal = (LONG)(ULONG_PTR)Wnd->lpfnWndProc;
            Wnd->lpfnWndProc = (PVOID)(ULONG_PTR)dwNewLong;
            break;

        case GWL_HINSTANCE:
            OldVal = (LONG)(ULONG_PTR)Wnd->hInstance;
            Wnd->hInstance = (HINSTANCE)(ULONG_PTR)dwNewLong;
            break;

        case GWL_HWNDPARENT:
            OldVal = (LONG)(ULONG_PTR)Wnd->hWndParent;
            Wnd->hWndParent = (HWND)(ULONG_PTR)dwNewLong;
            break;

        case GWL_ID:
            OldVal = (LONG)(ULONG_PTR)Wnd->hMenu;
            Wnd->hMenu = (HMENU)(ULONG_PTR)dwNewLong;
            break;

        case GWL_STYLE:
            OldVal = (LONG)Wnd->Style;
            Wnd->Style = (ULONG)dwNewLong;
            VisualChanged = TRUE;
            break;

        case GWL_EXSTYLE:
            OldVal = (LONG)Wnd->ExStyle;
            Wnd->ExStyle = (ULONG)dwNewLong;
            VisualChanged = TRUE;
            break;

        case GWL_USERDATA:
            OldVal = (LONG)Wnd->UserData;
            Wnd->UserData = (ULONG_PTR)dwNewLong;
            break;

        default:
            break;
    }

    if (VisualChanged)
    {
        Wnd->NeedsPaint = TRUE;
    }
    KsReleaseSpinLock(&gWindowListLock, ListIrql);

    WndDereference(Wnd);
    return OldVal;
}

ULONG
VEAPI
VeaUserGetClassLong(
    IN HWND hWnd,
    IN INT  nIndex
)
{
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return 0;
    }

    ULONG Result = 0;
    switch (nIndex)
    {
        case GCL_HBRBACKGROUND:
            Result = (ULONG)(ULONG_PTR)Wnd->hbrBackground;
            break;

        case GCL_HMODULE:
            Result = (ULONG)(ULONG_PTR)Wnd->hInstance;
            break;

        case GCL_WNDPROC:
            Result = (ULONG)(ULONG_PTR)Wnd->lpfnWndProc;
            break;

        default:
            break;
    }

    WndDereference(Wnd);
    return Result;
}

ULONG
VEAPI
VeaUserSetClassLong(
    IN HWND  hWnd,
    IN INT   nIndex,
    IN LONG  dwNewLong
)
{
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    KIRQL ListIrql;
    if (!Wnd)
    {
        return 0;
    }

    if (Wnd->OwnerProcess != (PEPROCESS)PtGetCurrentProcess())
    {
        WndDereference(Wnd);
        return 0;
    }

    ULONG OldVal = 0;

    KsAcquireSpinLock(&gWindowListLock, &ListIrql);
    switch (nIndex)
    {
        case GCL_HBRBACKGROUND:
            OldVal = (ULONG)(ULONG_PTR)Wnd->hbrBackground;
            Wnd->hbrBackground = (HBRUSH)(ULONG_PTR)dwNewLong;
            Wnd->NeedsPaint = TRUE;
            break;

        case GCL_HMODULE:
            OldVal = (ULONG)(ULONG_PTR)Wnd->hInstance;
            Wnd->hInstance = (HINSTANCE)(ULONG_PTR)dwNewLong;
            break;

        case GCL_WNDPROC:
            OldVal = (ULONG)(ULONG_PTR)Wnd->lpfnWndProc;
            Wnd->lpfnWndProc = (PVOID)(ULONG_PTR)dwNewLong;
            break;

        default:
            break;
    }
    KsReleaseSpinLock(&gWindowListLock, ListIrql);

    WndDereference(Wnd);
    return OldVal;
}

BOOLEAN
VEAPI
VeaUserSetWindowPos(
    IN HWND hWnd,
    IN HWND hWndInsertAfter,
    IN INT  X,
    IN INT  Y,
    IN INT  cx,
    IN INT  cy,
    IN UINT uFlags
)
{
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    KIRQL ListIrql;
    if (!Wnd)
    {
        return FALSE;
    }

    if (Wnd->OwnerProcess != (PEPROCESS)PtGetCurrentProcess())
    {
        WndDereference(Wnd);
        return FALSE;
    }

    BOOLEAN IsChild    = ((Wnd->Style & WS_CHILD) != 0);
    BOOLEAN WasVisible = ((Wnd->Style & WS_VISIBLE) != 0);

    LONG  OldX = Wnd->X;
    LONG  OldY = Wnd->Y;
    ULONG OldW = Wnd->Width;
    ULONG OldH = Wnd->Height;

    if (!IsChild && WasVisible && (GpActiveDesktop == NULL || Wnd->Desktop == GpActiveDesktop))
    {
        WndRestoreWindowBottomShadow(Wnd, TRUE);
    }

    KsAcquireSpinLock(&gWindowListLock, &ListIrql);

    if (!(uFlags & SWP_NOMOVE))
    {
        Wnd->X = (LONG)X;
        Wnd->Y = (LONG)Y;
    }

    if (!(uFlags & SWP_NOSIZE))
    {
        Wnd->Width  = (cx > 0) ? (ULONG)cx : 1;
        Wnd->Height = (cy > 0) ? (ULONG)cy : 1;
    }

    if (uFlags & SWP_SHOWWINDOW)
    {
        Wnd->Style |= WS_VISIBLE;
    }
    else if (uFlags & SWP_HIDEWINDOW)
    {
        Wnd->Style &= ~WS_VISIBLE;
    }

    LONG  NewX = Wnd->X;
    LONG  NewY = Wnd->Y;
    ULONG NewW = Wnd->Width;
    ULONG NewH = Wnd->Height;
    BOOLEAN NowVisible = ((Wnd->Style & WS_VISIBLE) != 0);

    if (NowVisible && !(uFlags & SWP_NOREDRAW))
    {
        Wnd->NeedsPaint = TRUE;
    }

    KsReleaseSpinLock(&gWindowListLock, ListIrql);

    if (!IsChild && !(uFlags & SWP_NOZORDER))
    {
        if (hWndInsertAfter == HWND_TOP || hWndInsertAfter == HWND_TOPMOST)
        {
            WndBringToFront(Wnd);
        }
    }

    BOOLEAN Moved   = (NewX != OldX || NewY != OldY);
    BOOLEAN Resized = (NewW != OldW || NewH != OldH);

    if (NowVisible && !(uFlags & SWP_NOREDRAW) &&
        (GpActiveDesktop == NULL || Wnd->Desktop == GpActiveDesktop))
    {
        /* Hapus sisa area lama hanya jika tidak tertutup penuh oleh posisi/ukuran baru */
        BOOLEAN FullyCovered = (NewX <= OldX &&
                                NewY <= OldY &&
                                (NewX + (LONG)NewW) >= (OldX + (LONG)OldW) &&
                                (NewY + (LONG)NewH) >= (OldY + (LONG)OldH));

        if (!IsChild && WasVisible && (Moved || Resized) && !FullyCovered)
        {
            EngHidePointer();
            EngFillDesktopRect(OldX, OldY, OldW, OldH,
                               WndGetDesktopBgColor(GpActiveDesktop));
            EngShowPointer();
        }

        WndInvalidateChildWindows(Wnd->hWnd);
        UserInvalidateAllWindows();
    }

    /* Kirim notifikasi WM_MOVE & WM_SIZE ke WndProc aplikasi */
    if (Moved)
    {
        LPARAM lParamMove = (LPARAM)(((ULONG)(USHORT)NewX) | (((ULONG)(USHORT)NewY) << 16));
        MsgPostToWindow(hWnd, WM_MOVE, 0, lParamMove, FALSE);
    }

    if (Resized)
    {
        WPARAM SizeType = SIZE_RESTORED;
        if (Wnd->Style & WS_MAXIMIZE)
        {
            SizeType = SIZE_MAXIMIZED;
        }
        else if (Wnd->Style & WS_MINIMIZE)
        {
            SizeType = SIZE_MINIMIZED;
        }

        LPARAM lParamSize = (LPARAM)(((ULONG)(USHORT)NewW) | (((ULONG)(USHORT)NewH) << 16));
        MsgPostToWindow(hWnd, WM_SIZE, SizeType, lParamSize, FALSE);
    }

    WndDereference(Wnd);
    return TRUE;
}

/* EOF */

