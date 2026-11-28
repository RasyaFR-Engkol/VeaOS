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
/*++

Routine Description:

    This routine retrieves the specified system metric or display dimension,
    such as primary screen size (defaulting to 800x600 if uninitialized),
    window border width, or title bar height.

Arguments:

    nIndex - Supplies the system metric index (SM_*) to retrieve.

Return Value:

    INT - Returns the requested system metric value, or 0 if nIndex is not
          supported.

--*/
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
/*++

Routine Description:

    This routine retrieves a 32-bit value from the specified window's
    attributes (such as window procedure, instance handle, parent handle,
    menu/ID, style, extended style, or user data).

Arguments:

    hWnd - Supplies the handle to the window.

    nIndex - Supplies the zero-based offset or GWL_* index specifying the
             value to retrieve.

Return Value:

    LONG - Returns the requested attribute value, or 0 if hWnd is invalid or
           nIndex is unsupported.

--*/
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
/*++

Routine Description:

    This routine modifies an attribute of the specified window under the
    global window list lock, verifying that the window belongs to the
    calling process. Changing GWL_STYLE or GWL_EXSTYLE marks the window as
    needing a repaint.

Arguments:

    hWnd - Supplies the handle to the window to modify.

    nIndex - Supplies the GWL_* index specifying the attribute to update.

    dwNewLong - Supplies the replacement value.

Return Value:

    LONG - Returns the previous value of the specified attribute, or 0 if
           hWnd is invalid, owned by another process, or nIndex is
           unsupported.

--*/
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
/*++

Routine Description:

    This routine retrieves the specified class-related value (background
    brush, module instance, or window procedure) associated with the given
    window.

Arguments:

    hWnd - Supplies the handle to the window.

    nIndex - Supplies the GCL_* index specifying the value to retrieve.

Return Value:

    ULONG - Returns the requested class attribute value, or 0 if hWnd is
            invalid or nIndex is unsupported.

--*/
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
/*++

Routine Description:

    This routine modifies a class-related attribute (background brush,
    module instance, or window procedure) on the specified window under the
    global window list lock, verifying that the window belongs to the
    calling process. Updating GCL_HBRBACKGROUND marks the window for
    repainting.

Arguments:

    hWnd - Supplies the handle to the window.

    nIndex - Supplies the GCL_* index specifying the attribute to update.

    dwNewLong - Supplies the replacement value.

Return Value:

    ULONG - Returns the previous value of the specified attribute, or 0 if
            hWnd is invalid, owned by another process, or nIndex is
            unsupported.

--*/
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
/*++

Routine Description:

    This routine changes the position, size, visibility, and Z-order of the
    specified window owned by the calling process. For visible top-level
    windows on the active desktop, it restores/redraws bottom shadows,
    performs fast back-buffer blitting with dirty-strip clearing on pure
    moves (or background fills on resizes/moves), invalidates child and
    desktop windows, and posts WM_MOVE and/or WM_SIZE notifications.

Arguments:

    hWnd - Supplies the handle to the window to reposition or resize.

    hWndInsertAfter - Supplies the Z-order placement handle (e.g., HWND_TOP
                      or HWND_TOPMOST) when SWP_NOZORDER is not set.

    X - Supplies the new horizontal position when SWP_NOMOVE is not set.

    Y - Supplies the new vertical position when SWP_NOMOVE is not set.

    cx - Supplies the new width (clamped to at least 1) when SWP_NOSIZE is
         not set.

    cy - Supplies the new height (clamped to at least 1) when SWP_NOSIZE is
         not set.

    uFlags - Supplies a combination of SWP_* flags controlling sizing,
             positioning, Z-order, visibility, and redraw behavior.

Return Value:

    BOOLEAN - Returns TRUE on success; otherwise returns FALSE if hWnd is
              invalid or not owned by the calling process.

--*/
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
        WndRestoreWindowBottomShadow(Wnd, FALSE);
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
        PVEAGDI_SURFACE Back = GpPrimaryDisplay.BackBuffer;
        ULONG BgColor = WndGetDesktopBgColor(GpActiveDesktop);

        BOOLEAN FullyCovered = (NewX <= OldX &&
                                NewY <= OldY &&
                                (NewX + (LONG)NewW) >= (OldX + (LONG)OldW) &&
                                (NewY + (LONG)NewH) >= (OldY + (LONG)OldH));

        if (!IsChild && WasVisible && Moved && !Resized && Back && Back->Scan0)
        {
            /* Copy visual window lama ke posisi baru di BackBuffer agar langsung ikut bergeser */
            WndBlitMoveWindow(Back, OldX, OldY, NewX, NewY, (LONG)NewW, (LONG)NewH);

            /* Hapus hanya sisa strip area lama di BackBuffer (tanpa present dulu) */
            LONG OldR = OldX + (LONG)OldW;
            LONG OldB = OldY + (LONG)OldH;
            LONG NewR = NewX + (LONG)NewW;
            LONG NewB = NewY + (LONG)NewH;

            /* Strip horizontal (atas/bawah) */
            if (NewY > OldY)
            {
                LONG StripH = (NewY < OldB) ? (NewY - OldY) : (LONG)OldH;
                if (StripH > 0)
                    EngFillRect32(Back, (ULONG)OldX, (ULONG)OldY, OldW, (ULONG)StripH, BgColor);
            }
            else if (NewB < OldB)
            {
                LONG TopY   = (NewB > OldY) ? NewB : OldY;
                LONG StripH = OldB - TopY;
                if (StripH > 0)
                    EngFillRect32(Back, (ULONG)OldX, (ULONG)TopY, OldW, (ULONG)StripH, BgColor);
            }

            /* Strip vertikal (kiri/kanan) pada irisan Y */
            LONG MidT = (OldY > NewY) ? OldY : NewY;
            LONG MidB = (OldB < NewB) ? OldB : NewB;
            if (MidT < MidB)
            {
                if (NewX > OldX)
                {
                    LONG StripW = (NewX < OldR) ? (NewX - OldX) : (LONG)OldW;
                    if (StripW > 0)
                        EngFillRect32(Back, (ULONG)OldX, (ULONG)MidT, (ULONG)StripW, (ULONG)(MidB - MidT), BgColor);
                }
                else if (NewR < OldR)
                {
                    LONG LeftX  = (NewR > OldX) ? NewR : OldX;
                    LONG StripW = OldR - LeftX;
                    if (StripW > 0)
                        EngFillRect32(Back, (ULONG)LeftX, (ULONG)MidT, (ULONG)StripW, (ULONG)(MidB - MidT), BgColor);
                }
            }

            /* Gambar shadow baru di BackBuffer (tanpa present), lalu 1x present UnionRect */
            WndDrawWindowBottomShadow(Wnd, FALSE);

            LONG ShadowPad = 8; /* Sesuaikan dengan tinggi shadow bawah window kamu */
            LONG UnionL = (OldX < NewX) ? OldX : NewX;
            LONG UnionT = (OldY < NewY) ? OldY : NewY;
            LONG UnionR = ((OldR > NewR) ? OldR : NewR) + ShadowPad;
            LONG UnionB = ((OldB > NewB) ? OldB : NewB) + ShadowPad;

            if (UnionL < 0) UnionL = 0;
            if (UnionT < 0) UnionT = 0;
            if (UnionR > (LONG)Back->Width)  UnionR = (LONG)Back->Width;
            if (UnionB > (LONG)Back->Height) UnionB = (LONG)Back->Height;

            if (UnionL < UnionR && UnionT < UnionB)
            {
                EngPresentRect(UnionL, UnionT, (ULONG)(UnionR - UnionL), (ULONG)(UnionB - UnionT));
            }
        }
        else if (!IsChild && WasVisible && (Moved || Resized) && !FullyCovered)
        {
            if (Back && Back->Scan0)
            {
                EngFillRect32(Back, (ULONG)OldX, (ULONG)OldY, OldW, OldH, BgColor);
                EngPresentRect(OldX, OldY, OldW, OldH);
            }
            else
            {
                EngFillDesktopRect(OldX, OldY, OldW, OldH, BgColor);
            }
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

VOID
VEAPI
VeaUserSetCaptionButtonState(
    IN HWND  hWnd,
    IN ULONG HotBtn,
    IN ULONG PressedBtn
)
/*++

Routine Description:

    This routine updates the global hot (hovered) and pressed caption button
    tracking state for the specified window and triggers a redraw of its
    caption buttons.

Arguments:

    hWnd - Supplies the handle to the window whose caption button state is
           updated.

    HotBtn - Supplies the identifier of the currently hovered caption
             button, or CAPTION_BTN_NONE if none.

    PressedBtn - Supplies the identifier of the currently pressed caption
                 button, or CAPTION_BTN_NONE if none.

Return Value:

    None.

--*/
{
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return;
    }

    gHotCaptionWnd     = (HotBtn != CAPTION_BTN_NONE) ? hWnd : NULL;
    gHotCaptionBtn     = HotBtn;
    gPressedCaptionWnd = (PressedBtn != CAPTION_BTN_NONE) ? hWnd : NULL;
    gPressedCaptionBtn = PressedBtn;

    WndRedrawCaptionButtons(Wnd);
    WndDereference(Wnd);
}

BOOLEAN
VEAPI
VeaUserSetTaskmanWindow(
    IN HWND hWnd
)
/*++

Routine Description:

    This routine assigns the taskbar/task-manager window handle for the
    current thread's desktop (or the active desktop if none is bound).

Arguments:

    hWnd - Supplies the handle to the taskman window, or NULL to clear it.

Return Value:

    BOOLEAN - Returns TRUE if the taskman window was updated; otherwise
              returns FALSE if no desktop is available or a non-NULL hWnd is
              invalid.

--*/
{
    PTHREADINFO pti = PtGetCurrentThreadVeapiThread();
    PDESKTOP_OBJECT DskObj = (pti && pti->rpdesk) ? pti->rpdesk : GpActiveDesktop;

    if (!DskObj)
    {
        return FALSE;
    }

    if (hWnd != NULL && WndLookupHandleLocked(hWnd) == NULL)
    {
        return FALSE;
    }

    DskObj->TaskmanWindow = hWnd;
    return TRUE;
}

BOOLEAN
VEAPI
VeaUserRegisterShellHookWindow(
    IN HWND hWnd
)
/*++

Routine Description:

    This routine registers the specified window as the shell hook listener
    on the current thread's or active desktop. Upon registration, it
    enumerates up to 64 existing taskbar-eligible windows on the desktop
    and sends initial HSHELL_WINDOWCREATED notifications, followed by
    HSHELL_WINDOWACTIVATED if a taskbar-eligible foreground window exists.

Arguments:

    hWnd - Supplies the handle to the window to receive shell hook
           notifications.

Return Value:

    BOOLEAN - Returns TRUE if the window was successfully registered;
              otherwise returns FALSE if the desktop or hWnd is invalid.

--*/
{
    PTHREADINFO pti = (PTHREADINFO)PtGetCurrentThreadVeapiThread();
    PDESKTOP_OBJECT Desk = (pti && pti->rpdesk) ? pti->rpdesk : GpActiveDesktop;
    if (!Desk || !hWnd)
    {
        return FALSE;
    }

    PWND_OBJECT HookWnd = WndLookupHandle(hWnd);
    if (!HookWnd)
    {
        return FALSE;
    }
    WndDereference(HookWnd);

    HWND InitialHwnds[64];
    ULONG InitialCount = 0;
    HWND ActiveTaskHwnd = NULL;
    KIRQL ListIrql, DeskIrql;

    KsAcquireSpinLock(&gWindowListLock, &ListIrql);
    KsAcquireSpinLock(&Desk->Lock, &DeskIrql);

    Desk->ShellHookWindow = hWnd;

    for (PLIST_ENTRY Entry = Desk->WindowListHead.Flink;
         Entry != &Desk->WindowListHead && InitialCount < 64;
         Entry = Entry->Flink)
    {
        PWND_OBJECT Window = CONTAINING_RECORD(Entry, WND_OBJECT, DesktopListEntry);
        if (Window->Magic == WND_OBJECT_MAGIC &&
            Window->Used &&
            Window->hWnd != hWnd &&
            WndIsTaskbarEligible(Desk, Window))
        {
            InitialHwnds[InitialCount++] = Window->hWnd;
        }
    }

    PWND_OBJECT FgWnd = (PWND_OBJECT)Desk->ForegroundWindow;
    if (FgWnd != NULL &&
        FgWnd->Magic == WND_OBJECT_MAGIC &&
        FgWnd->Used &&
        WndIsTaskbarEligible(Desk, FgWnd))
    {
        ActiveTaskHwnd = FgWnd->hWnd;
    }

    KsReleaseSpinLock(&Desk->Lock, DeskIrql);
    KsReleaseSpinLock(&gWindowListLock, ListIrql);

    for (ULONG i = 0; i < InitialCount; i++)
    {
        WndNotifyShellHook(Desk, HSHELL_WINDOWCREATED, InitialHwnds[i]);
    }

    if (ActiveTaskHwnd != NULL)
    {
        WndNotifyShellHook(Desk, HSHELL_WINDOWACTIVATED, ActiveTaskHwnd);
    }

    return TRUE;
}

BOOLEAN
VEAPI
VeaUserDeregisterShellHookWindow(
    IN HWND hWnd
)
/*++

Routine Description:

    This routine unregisters the specified window as the shell hook listener
    on the current thread's or active desktop if it is currently registered.

Arguments:

    hWnd - Supplies the handle to the shell hook window to deregister.

Return Value:

    BOOLEAN - Returns TRUE if a valid desktop was found; otherwise returns
              FALSE.

--*/
{
    PTHREADINFO pti = (PTHREADINFO)PtGetCurrentThreadVeapiThread();
    PDESKTOP_OBJECT Desk = (pti && pti->rpdesk) ? pti->rpdesk : GpActiveDesktop;
    if (!Desk)
    {
        return FALSE;
    }

    if (Desk->ShellHookWindow == hWnd)
    {
        Desk->ShellHookWindow = NULL;
    }

    return TRUE;
}

/* EOF */

