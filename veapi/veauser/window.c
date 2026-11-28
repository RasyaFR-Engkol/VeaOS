/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : window.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Veapi Window Manager
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>
#include <psdk/w32thread.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file window.c
 * --------------------------------------------------------------------- */

/* VARIABLES *****************************************************************************/

static LIST_ENTRY   gClassListHead;
static KSPIN_LOCK   gClassListLock;
LIST_ENTRY   gWindowListHead;
KSPIN_LOCK   gWindowListLock;
static BOOLEAN      gWndListsInitialized = FALSE;
static ULONG        gWindowCascadeCount  = 0;
HWND                    IptFocusWindow = NULL;
HWND  gHotCaptionWnd     = NULL;
ULONG gHotCaptionBtn     = CAPTION_BTN_NONE;
HWND  gPressedCaptionWnd = NULL;
ULONG gPressedCaptionBtn = CAPTION_BTN_NONE;

/* PRIVATE *****************************************************************************/

BOOLEAN
WndIsTaskbarEligible(
    IN PDESKTOP_OBJECT Desktop,
    IN PWND_OBJECT     Wnd
)
/*++

Routine Description:

    This routine determines whether a window qualifies to be displayed as a
    taskbar button on the specified desktop based on its visibility,
    hierarchy, shell role, and extended window styles.

Arguments:

    Desktop - Supplies an optional pointer to the desktop object used to
              exclude the taskbar and shell hook windows themselves.

    Wnd - Supplies a pointer to the window object to evaluate.

Return Value:

    BOOLEAN - Returns TRUE if the window is eligible to appear on the
              taskbar; otherwise returns FALSE.

--*/
{
    if (!Wnd)
    {
        return FALSE;
    }

    /* Harus top-level window yang visible */
    if ((Wnd->Style & WS_CHILD) || !(Wnd->Style & WS_VISIBLE))
    {
        return FALSE;
    }

    /* Jangan masukkan taskbar/shell hook window itu sendiri */
    if (Desktop)
    {
        if (Wnd->hWnd == Desktop->TaskmanWindow ||
            Wnd->hWnd == Desktop->ShellHookWindow)
        {
            return FALSE;
        }
    }

    /* WS_EX_APPWINDOW memaksa window masuk taskbar meski punya owner/tool style */
    if (Wnd->ExStyle & WS_EX_APPWINDOW)
    {
        return TRUE;
    }

    /* Pada top-level window, hWndParent berperan sebagai owner window */
    if ((Wnd->ExStyle & WS_EX_TOOLWINDOW) || (Wnd->hWndParent != NULL))
    {
        return FALSE;
    }

    return TRUE;
}

VOID
WndNotifyShellHook(
    IN PDESKTOP_OBJECT Desktop,
    IN WPARAM          EventCode,
    IN HWND            TargetHwnd
)
/*++

Routine Description:

    This routine posts a shell hook notification message to the desktop's
    registered shell hook window. If the registered shell hook window is no
    longer valid, the registration is cleared automatically.

Arguments:

    Desktop - Supplies a pointer to the desktop object whose shell hook
              window is to be notified.

    EventCode - Supplies the shell hook event code passed as WPARAM.

    TargetHwnd - Supplies the handle of the window associated with the event,
                 passed as LPARAM.

Return Value:

    None.

--*/
{
    if (!Desktop)
    {
        return;
    }

    HWND HookHwnd = Desktop->ShellHookWindow;
    if (HookHwnd == NULL)
    {
        return;
    }

    /* Validasi aman di luar spinlock; auto-cleanup jika explorer.elf mati */
    PWND_OBJECT HookWnd = WndLookupHandle(HookHwnd);
    if (!HookWnd)
    {
        if (Desktop->ShellHookWindow == HookHwnd)
        {
            Desktop->ShellHookWindow = NULL;
        }
        return;
    }
    WndDereference(HookWnd);

    MsgPostToWindow(HookHwnd, WM_SHELLHOOKMESSAGE, EventCode, (LPARAM)TargetHwnd, FALSE);
}

static VOID
WndInvalidateWindowsInRect(
    IN CONST RECT *DirtyRect
)
/*++

Routine Description:

    This routine marks all effectively visible windows on the active desktop
    whose screen bounds intersect the specified dirty rectangle as needing a
    repaint, and notifies the message subsystem if any window was invalidated.

Arguments:

    DirtyRect - Supplies a pointer to the screen-space rectangle describing
                the invalidated region.

Return Value:

    None.

--*/
{
    PLIST_ENTRY Entry;
    KIRQL OldIrql;
    ULONG Count = 0;

    if (!DirtyRect || DirtyRect->left >= DirtyRect->right || DirtyRect->top >= DirtyRect->bottom)
    {
        return;
    }

    WndEnsureListsInitialized();

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    for (Entry = gWindowListHead.Flink; Entry != &gWindowListHead; Entry = Entry->Flink)
    {
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
        if (Wnd->Magic != WND_OBJECT_MAGIC ||
            !Wnd->Used ||
            !WndIsEffectivelyVisibleLocked(Wnd) ||
            (GpActiveDesktop != NULL && Wnd->Desktop != GpActiveDesktop))
        {
            continue;
        }

        LONG Left, Top, Right, Bottom;
        if (Wnd->Style & WS_CHILD)
        {
            LONG Ox = 0, Oy = 0;
            WndGetClientOriginOnScreen(Wnd, &Ox, &Oy);
            Left   = Ox;
            Top    = Oy;
            Right  = Ox + (LONG)Wnd->Width;
            Bottom = Oy + (LONG)Wnd->Height;
        }
        else
        {
            Left   = Wnd->X;
            Top    = Wnd->Y;
            Right  = Wnd->X + (LONG)Wnd->Width;
            Bottom = Wnd->Y + (LONG)Wnd->Height;
        }

        if (Left < DirtyRect->right && Right > DirtyRect->left &&
            Top < DirtyRect->bottom && Bottom > DirtyRect->top)
        {
            Wnd->NeedsPaint = TRUE;
            Count++;
        }
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    if (Count > 0)
    {
        MsgNotifyPaintAll();
    }
}

VOID
WndBlitMoveWindow(
    IN PVEAGDI_SURFACE Surface,
    IN LONG            OldX,
    IN LONG            OldY,
    IN LONG            NewX,
    IN LONG            NewY,
    IN LONG            Width,
    IN LONG            Height
)
/*++

Routine Description:

    This routine copies a rectangular window region from an old position to a
    new position within the same 32-bit-per-pixel surface, clipping both
    source and destination against the surface bounds and safely handling
    overlapping vertical moves.

Arguments:

    Surface - Supplies a pointer to the target GDI surface.

    OldX - Supplies the original horizontal coordinate, in pixels, of the
           window region.

    OldY - Supplies the original vertical coordinate, in pixels, of the
           window region.

    NewX - Supplies the new horizontal coordinate, in pixels, of the window
           region.

    NewY - Supplies the new vertical coordinate, in pixels, of the window
           region.

    Width - Supplies the width, in pixels, of the region to move.

    Height - Supplies the height, in pixels, of the region to move.

Return Value:

    None.

--*/
{
    if (!Surface || !Surface->Scan0 || Width <= 0 || Height <= 0)
    {
        return;
    }

    LONG ScreenW = (LONG)Surface->Width;
    LONG ScreenH = (LONG)Surface->Height;

    LONG OffX = 0;
    LONG OffY = 0;
    if (OldX < 0) OffX = -OldX;
    if (NewX + OffX < 0) OffX = -NewX;
    if (OldY < 0) OffY = -OldY;
    if (NewY + OffY < 0) OffY = -NewY;

    LONG CopyW = Width - OffX;
    if (ScreenW - (OldX + OffX) < CopyW) CopyW = ScreenW - (OldX + OffX);
    if (ScreenW - (NewX + OffX) < CopyW) CopyW = ScreenW - (NewX + OffX);

    LONG CopyH = Height - OffY;
    if (ScreenH - (OldY + OffY) < CopyH) CopyH = ScreenH - (OldY + OffY);
    if (ScreenH - (NewY + OffY) < CopyH) CopyH = ScreenH - (NewY + OffY);

    if (CopyW <= 0 || CopyH <= 0)
    {
        return;
    }

    LONG SrcX = OldX + OffX;
    LONG SrcY = OldY + OffY;
    LONG DstX = NewX + OffX;
    LONG DstY = NewY + OffY;
    SIZE_T RowBytes = (SIZE_T)CopyW * sizeof(ULONG);

    if (DstY > SrcY)
    {
        for (LONG Row = CopyH - 1; Row >= 0; Row--)
        {
            PUCHAR SrcRow = (PUCHAR)Surface->Scan0 + (SIZE_T)(SrcY + Row) * Surface->Pitch + (SIZE_T)SrcX * sizeof(ULONG);
            PUCHAR DstRow = (PUCHAR)Surface->Scan0 + (SIZE_T)(DstY + Row) * Surface->Pitch + (SIZE_T)DstX * sizeof(ULONG);
            RtlMoveMemory(DstRow, SrcRow, RowBytes);
        }
    }
    else
    {
        for (LONG Row = 0; Row < CopyH; Row++)
        {
            PUCHAR SrcRow = (PUCHAR)Surface->Scan0 + (SIZE_T)(SrcY + Row) * Surface->Pitch + (SIZE_T)SrcX * sizeof(ULONG);
            PUCHAR DstRow = (PUCHAR)Surface->Scan0 + (SIZE_T)(DstY + Row) * Surface->Pitch + (SIZE_T)DstX * sizeof(ULONG);
            RtlMoveMemory(DstRow, SrcRow, RowBytes);
        }
    }
}

static PWND_OBJECT
WndFindNextTopLevelLocked(
    IN PDESKTOP_OBJECT Desktop,
    IN PWND_OBJECT     ExcludeTop
)
/*++

Routine Description:

    This routine searches the global window list in reverse Z-order (top to
    bottom) for the highest-ordered visible top-level window on the specified
    desktop, excluding a given window.

    N.B. The global window list lock must be held by the caller.

Arguments:

    Desktop - Supplies an optional pointer to the desktop object used to
              filter candidate windows. If NULL, windows on any desktop are
              considered.

    ExcludeTop - Supplies an optional pointer to a window object to skip
                 during the search.

Return Value:

    PWND_OBJECT - Returns a pointer to the next highest visible top-level
                  window, or NULL if no matching window is found.

--*/
{
    PLIST_ENTRY Entry;

    /* Z-order paling atas ada di Blink */
    for (Entry = gWindowListHead.Blink; Entry != &gWindowListHead; Entry = Entry->Blink)
    {
        PWND_OBJECT Cand = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
        if (Cand->Magic != WND_OBJECT_MAGIC || !Cand->Used)
        {
            continue;
        }
        if (Cand == ExcludeTop || (Cand->Style & WS_CHILD))
        {
            continue;
        }
        if (Desktop != NULL && Cand->Desktop != Desktop)
        {
            continue;
        }
        if (!(Cand->Style & WS_VISIBLE))
        {
            continue;
        }
        return Cand;
    }

    return NULL;
}

static PWND_OBJECT
WndFindVisibleAncestorLocked(
    IN PWND_OBJECT ChildWnd
)
/*++

Routine Description:

    This routine walks up the parent chain of a child window to locate the
    nearest ancestor window that is effectively visible, capping traversal at
    64 levels to prevent cycles.

    N.B. The global window list lock must be held by the caller.

Arguments:

    ChildWnd - Supplies a pointer to the child window object whose visible
               ancestor is to be found.

Return Value:

    PWND_OBJECT - Returns a pointer to the nearest effectively visible
                  ancestor window, or NULL if none is found.

--*/
{
    PWND_OBJECT Cur = ChildWnd ? WndLookupHandleLocked(ChildWnd->hWndParent) : NULL;
    ULONG Depth = 0;

    while (Cur && Depth < 64)
    {
        if (Cur->Magic == WND_OBJECT_MAGIC && Cur->Used &&
            WndIsEffectivelyVisibleLocked(Cur))
        {
            return Cur;
        }
        if (!(Cur->Style & WS_CHILD) || Cur->hWndParent == NULL)
        {
            break;
        }
        Cur = WndLookupHandleLocked(Cur->hWndParent);
        Depth++;
    }

    return NULL;
}

static VOID
WndGetClientOriginLocked(
    IN  PWND_OBJECT Wnd,
    OUT PLONG       OutX,
    OUT PLONG       OutY
)
/*++

Routine Description:

    This routine calculates the screen-space origin coordinates of the
    specified window's client area, accounting for parent offsets on child
    windows and frame/titlebar insets on framed top-level windows.

    N.B. The global window list lock must be held by the caller.

Arguments:

    Wnd - Supplies a pointer to the target window object.

    OutX - Receives the screen-space horizontal coordinate, in pixels, of the
           client area origin.

    OutY - Receives the screen-space vertical coordinate, in pixels, of the
           client area origin.

Return Value:

    None.

--*/
{
    if (!Wnd)
    {
        *OutX = 0;
        *OutY = 0;
        return;
    }

    if (Wnd->Style & WS_CHILD)
    {
        WndGetClientOriginOnScreen(Wnd, OutX, OutY);
    }
    else
    {
        BOOLEAN Framed = !((Wnd->Style & WS_POPUP) && ((Wnd->Style & WS_CAPTION) != WS_CAPTION));
        *OutX = Framed ? (Wnd->X + WND_BORDER_WIDTH)    : Wnd->X;
        *OutY = Framed ? (Wnd->Y + WND_TITLEBAR_HEIGHT) : Wnd->Y;
    }
}

static BOOLEAN
WndIsDescendantLocked(
    IN PWND_OBJECT Ancestor,
    IN PWND_OBJECT Node
)
/*++

Routine Description:

    This routine determines whether a given window is a child descendant of
    the specified ancestor window by walking up its parent chain, capped at
    64 levels.

    N.B. The global window list lock must be held by the caller.

Arguments:

    Ancestor - Supplies a pointer to the potential ancestor window object.

    Node - Supplies a pointer to the window object to test.

Return Value:

    BOOLEAN - Returns TRUE if Node is a descendant of Ancestor; otherwise
              returns FALSE.

--*/
{
    PWND_OBJECT Cur = Node;
    ULONG Depth = 0;

    if (!Ancestor || !Node)
    {
        return FALSE;
    }

    while (Cur && (Cur->Style & WS_CHILD) && Cur->hWndParent != NULL && Depth < 64)
    {
        if (Cur->hWndParent == Ancestor->hWnd)
        {
            return TRUE;
        }
        Cur = WndLookupHandleLocked(Cur->hWndParent);
        Depth++;
    }

    return FALSE;
}

VOID
WndBringToFront(
    IN PWND_OBJECT TopWnd
)
/*++

Routine Description:

    This routine raises a top-level window and all of its child descendants to
    the front of the global Z-order while preserving their relative ordering.
    Non-topmost windows are placed immediately beneath the desktop's visible
    taskbar window, if one is present.

Arguments:

    TopWnd - Supplies a pointer to the top-level window object to bring to the
             front. Child windows are ignored.

Return Value:

    None.

--*/
{
    LIST_ENTRY GroupHead;
    PLIST_ENTRY Entry;
    KIRQL OldIrql;

    if (!TopWnd || (TopWnd->Style & WS_CHILD))
    {
        return;
    }

    InitializeListHead(&GroupHead);

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);

    /* 1. Pindahkan TopWnd lebih dulu ke GroupHead */
    RemoveEntryList(&TopWnd->GlobalListEntry);
    InsertTailList(&GroupHead, &TopWnd->GlobalListEntry);

    /* 2. Kumpulkan semua descendant WS_CHILD milik TopWnd */
    Entry = gWindowListHead.Flink;
    while (Entry != &gWindowListHead)
    {
        PLIST_ENTRY Next = Entry->Flink;
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);

        if ((Wnd->Style & WS_CHILD) && Wnd->hWndParent != NULL)
        {
            BOOLEAN BelongsToGroup = FALSE;
            PWND_OBJECT Cur = Wnd;
            ULONG Depth = 0;

            while (Cur && (Cur->Style & WS_CHILD) && Cur->hWndParent != NULL && Depth < 64)
            {
                if (Cur->hWndParent == TopWnd->hWnd)
                {
                    BelongsToGroup = TRUE;
                    break;
                }

                PWND_OBJECT P = WndLookupHandleLocked(Cur->hWndParent);
                if (!P)
                {
                    PLIST_ENTRY GEntry;
                    for (GEntry = GroupHead.Flink; GEntry != &GroupHead; GEntry = GEntry->Flink)
                    {
                        PWND_OBJECT GWnd = CONTAINING_RECORD(GEntry, WND_OBJECT, GlobalListEntry);
                        if (GWnd->hWnd == Cur->hWndParent &&
                            GWnd->Magic == WND_OBJECT_MAGIC && GWnd->Used)
                        {
                            P = GWnd;
                            break;
                        }
                    }
                }

                if (!P)
                {
                    break;
                }
                if (P == TopWnd)
                {
                    BelongsToGroup = TRUE;
                    break;
                }
                Cur = P;
                Depth++;
            }

            if (BelongsToGroup)
            {
                RemoveEntryList(&Wnd->GlobalListEntry);
                InsertTailList(&GroupHead, &Wnd->GlobalListEntry);
            }
        }

        Entry = Next;
    }

    /* 3. Tentukan titik sisip: window biasa ditaruh tepat di bawah TaskmanWindow */
    PLIST_ENTRY InsertTarget = &gWindowListHead;
    if (TopWnd->Desktop != NULL &&
        TopWnd->Desktop->TaskmanWindow != NULL &&
        TopWnd->hWnd != TopWnd->Desktop->TaskmanWindow &&
        !(TopWnd->ExStyle & WS_EX_TOPMOST))
    {
        PWND_OBJECT TaskmanWnd = WndLookupHandleLocked(TopWnd->Desktop->TaskmanWindow);
        if (TaskmanWnd != NULL && (TaskmanWnd->Style & WS_VISIBLE))
        {
            InsertTarget = &TaskmanWnd->GlobalListEntry;
        }
    }

    /* 4. Masukkan kembali [TopWnd, Child1, Child2, ...] */
    while (!IsListEmpty(&GroupHead))
    {
        Entry = RemoveHeadList(&GroupHead);
        InsertTailList(InsertTarget, Entry);
    }

    KsReleaseSpinLock(&gWindowListLock, OldIrql);
}

static BOOLEAN
WndStrEqual(
    IN CONST CHAR *S1,
    IN CONST CHAR *S2
)
{
    return RtlRawStringEqual((PCHAR)S1, (PCHAR)S2);
}

static BOOLEAN
WndCopyStringSafe(
    OUT PCHAR Dest,
    IN PCHAR Src,
    IN ULONG MaxLen,
    IN BOOLEAN FromUser
)
{
    if (!Dest || !Src || MaxLen == 0)
    {
        return FALSE;
    }

    RtlZeroMemory(Dest, MaxLen);

    _VEA_TRY
    {
        for (ULONG i = 0; i < (MaxLen - 1); i++)
        {
            ULONG_PTR Addr = (ULONG_PTR)(Src + i);
            if (FromUser && Addr >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
            {
                _VEA_YIELD(return FALSE);
            }

            CHAR Ch = Src[i];
            Dest[i] = Ch;
            if (Ch == '\0')
            {
                break;
            }
        }
        Dest[MaxLen - 1] = '\0';
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        return FALSE;
    }
    _VEA_END

    return TRUE;
}

static PWND_CLASS_OBJECT
WndFindClassByName(
    IN CONST CHAR *ClassName,
    IN PEPROCESS Process
)
/*++

Routine Description:

    This routine searches the global window class list for an active window
    class matching the specified name that is either registered as a global
    class (CS_GLOBALCLASS) or owned by the specified process.

Arguments:

    ClassName - Supplies a pointer to the null-terminated name of the window
                class to locate.

    Process - Supplies a pointer to the process object used to match
              process-local window classes.

Return Value:

    PWND_CLASS_OBJECT - Returns a pointer to the matching window class object,
                        or NULL if no matching class is found.

--*/
{
    PLIST_ENTRY Entry;
    KIRQL OldIrql;
    PWND_CLASS_OBJECT Found = NULL;

    WndEnsureListsInitialized();

    KsAcquireSpinLock(&gClassListLock, &OldIrql);
    for (Entry = gClassListHead.Flink; Entry != &gClassListHead; Entry = Entry->Flink)
    {
        PWND_CLASS_OBJECT Cls = CONTAINING_RECORD(Entry, WND_CLASS_OBJECT, ListEntry);
        if (Cls->Magic == WND_CLASS_MAGIC &&
            Cls->Used &&
            ((Cls->Style & CS_GLOBALCLASS) || Cls->OwnerProcess == Process) &&
            WndStrEqual(Cls->ClassName, ClassName))
        {
            Found = Cls;
            break;
        }
    }
    KsReleaseSpinLock(&gClassListLock, OldIrql);

    return Found;
}

VOID
VEAPI
WndCleanupProcessClasses(
    IN PEPROCESS Process
)
/*++

Routine Description:

    This routine unlinks, invalidates, and frees all window class objects
    owned by the specified process from the global window class list,
    deferring pool deallocation until after the class list spinlock is
    released.

Arguments:

    Process - Supplies a pointer to the terminating process whose registered
              window classes are to be cleaned up.

Return Value:

    None.

--*/
{
    LIST_ENTRY VictimsHead;
    PLIST_ENTRY Entry;
    KIRQL OldIrql;

    if (!Process)
    {
        return;
    }

    WndEnsureListsInitialized();
    InitializeListHead(&VictimsHead);

    KsAcquireSpinLock(&gClassListLock, &OldIrql);
    Entry = gClassListHead.Flink;
    while (Entry != &gClassListHead)
    {
        PLIST_ENTRY Next = Entry->Flink;
        PWND_CLASS_OBJECT Cls = CONTAINING_RECORD(Entry, WND_CLASS_OBJECT, ListEntry);

        if (Cls->OwnerProcess == Process)
        {
            RemoveEntryList(&Cls->ListEntry);
            Cls->Magic = 0;
            Cls->Used = FALSE;
            InsertTailList(&VictimsHead, &Cls->ListEntry);
        }

        Entry = Next;
    }
    KsReleaseSpinLock(&gClassListLock, OldIrql);

    while (!IsListEmpty(&VictimsHead))
    {
        Entry = RemoveHeadList(&VictimsHead);
        WND_POOL_FREE(CONTAINING_RECORD(Entry, WND_CLASS_OBJECT, ListEntry));
    }
}

/* PUBLIC *******************************************************************************/

BOOLEAN
VEAPI
WndIsEffectivelyVisibleLocked(
    IN PWND_OBJECT Wnd
)
/*++

Routine Description:

    This routine determines whether the specified window and every ancestor
    in its child-parent chain (up to 64 levels) are valid, active, and have
    the WS_VISIBLE style set.

    N.B. The global window list lock must be held by the caller.

Arguments:

    Wnd - Supplies a pointer to the window object to evaluate.

Return Value:

    BOOLEAN - Returns TRUE if the window and all of its ancestors are
              visible; otherwise returns FALSE.

--*/
{
    PWND_OBJECT Cur = Wnd;
    ULONG Depth = 0;

    while (Cur && Depth < 64)
    {
        if (Cur->Magic != WND_OBJECT_MAGIC || !Cur->Used)
        {
            return FALSE;
        }
        if (!(Cur->Style & WS_VISIBLE))
        {
            return FALSE;
        }
        if (!(Cur->Style & WS_CHILD) || Cur->hWndParent == NULL)
        {
            break;
        }
        Cur = WndLookupHandleLocked(Cur->hWndParent);
        Depth++;
    }

    return (Cur != NULL);
}

BOOLEAN
VEAPI
WndIsEffectivelyVisible(
    IN PWND_OBJECT Wnd
)
/*++

Routine Description:

    This routine acquires the global window list spinlock and determines
    whether the specified window is effectively visible within its parent
    hierarchy.

Arguments:

    Wnd - Supplies a pointer to the window object to evaluate.

Return Value:

    BOOLEAN - Returns TRUE if the window and all of its ancestors are
              visible; otherwise returns FALSE.

--*/
{
    KIRQL OldIrql;
    BOOLEAN Visible;

    if (!Wnd)
    {
        return FALSE;
    }

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    Visible = WndIsEffectivelyVisibleLocked(Wnd);
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    return Visible;
}

VOID
WndReference(
    IN PWND_OBJECT Wnd
)
/*++

Routine Description:

    This routine atomically increments the reference count of the specified
    window object to prevent it from being freed while in use.

Arguments:

    Wnd - Supplies a pointer to the window object to reference.

Return Value:

    None.

--*/
{
    if (Wnd)
    {
        InterlockedIncrement((PLONG)&Wnd->RefCount);
    }
}

VOID
WndDereference(
    IN PWND_OBJECT Wnd
)
/*++

Routine Description:

    This routine atomically decrements the reference count of the specified
    window object. When the reference count reaches zero, any allocated
    shadow backup buffer and the window object itself are freed.

Arguments:

    Wnd - Supplies a pointer to the window object to dereference.

Return Value:

    None.

--*/
{
    if (Wnd && InterlockedDecrement((PLONG)&Wnd->RefCount) == 0)
    {
        if (Wnd->ShadowBackup != NULL)
        {
            GDI_POOL_FREE(Wnd->ShadowBackup);
            Wnd->ShadowBackup = NULL;
        }
        WND_POOL_FREE(Wnd);
    }
}

VOID
WndEnsureListsInitialized(VOID)
/*++

Routine Description:

    This routine lazily initializes the global window class list, the global
    window list, and their corresponding spinlocks if they have not yet been
    initialized.

Arguments:

    None.

Return Value:

    None.

--*/
{
    if (!gWndListsInitialized)
    {
        InitializeListHead(&gClassListHead);
        KsInitializeSpinlock(&gClassListLock);
        InitializeListHead(&gWindowListHead);
        KsInitializeSpinlock(&gWindowListLock);
        gWndListsInitialized = TRUE;
    }
}

ULONG
WndGetDesktopBgColor(
    IN PDESKTOP_OBJECT Desktop
)
/*++

Routine Description:

    This routine retrieves the 32-bit ARGB background color configured for the
    specified desktop, falling back to the default teal color (0xFF008080)
    if no desktop is provided.

Arguments:

    Desktop - Supplies an optional pointer to the desktop object.

Return Value:

    ULONG - Returns the desktop's 32-bit ARGB background color, or 0xFF008080
            if Desktop is NULL.

--*/
{
    if (Desktop != NULL)
    {
        return Desktop->BackgroundColor;
    }
    return 0xFF008080;
}

PWND_OBJECT
WndLookupHandleLocked(
    IN HWND hWnd
)
/*++

Routine Description:

    This routine validates a window handle and locates its corresponding
    active window object in the global window list without incrementing its
    reference count.

    N.B. The global window list lock must be held by the caller.

Arguments:

    hWnd - Supplies the window handle to look up.

Return Value:

    PWND_OBJECT - Returns a pointer to the matching active window object, or
                  NULL if the handle is invalid, in user-mode address space,
                  or not found.

--*/
{
    PLIST_ENTRY Entry;

    if (!hWnd || MI_IS_USER_ADDRESS((ULONG_PTR)hWnd))
    {
        return NULL;
    }

    for (Entry = gWindowListHead.Flink; Entry != &gWindowListHead; Entry = Entry->Flink)
    {
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
        if (Wnd == (PWND_OBJECT)hWnd && Wnd->Magic == WND_OBJECT_MAGIC && Wnd->Used)
        {
            return Wnd;
        }
    }

    return NULL;
}

PWND_OBJECT
WndLookupHandle(
    IN HWND hWnd
)
/*++

Routine Description:

    This routine acquires the global window list spinlock, validates the
    specified window handle, and returns a referenced pointer to the matching
    active window object.

    N.B. The caller is responsible for calling WndDereference on the returned
         window object when finished.

Arguments:

    hWnd - Supplies the window handle to look up.

Return Value:

    PWND_OBJECT - Returns a referenced pointer to the matching active window
                  object, or NULL if the handle is invalid or not found.

--*/
{
    PLIST_ENTRY Entry;
    KIRQL OldIrql;
    PWND_OBJECT Found = NULL;

    if (!hWnd || MI_IS_USER_ADDRESS((ULONG_PTR)hWnd))
    {
        return NULL;
    }

    WndEnsureListsInitialized();

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    for (Entry = gWindowListHead.Flink; Entry != &gWindowListHead; Entry = Entry->Flink)
    {
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
        if (Wnd == (PWND_OBJECT)hWnd && Wnd->Magic == WND_OBJECT_MAGIC && Wnd->Used)
        {
            WndReference(Wnd);
            Found = Wnd;
            break;
        }
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    return Found;
}

LRESULT
WndInvokeUserProc(
    IN PWND_OBJECT Wnd,
    IN ULONG Msg,
    IN WPARAM wParam,
    IN LPARAM lParam
)
/*++

Routine Description:

    This routine dispatches a window message to the window's user-mode window
    procedure via a kernel-to-user callback. For WM_CREATE messages with a
    NULL lParam, it marshals a CREATESTRUCTA payload along with the window
    title and class name into the callback buffer using relative offsets.

Arguments:

    Wnd - Supplies a pointer to the target window object.

    Msg - Supplies the window message identifier.

    wParam - Supplies additional message-specific information.

    lParam - Supplies additional message-specific information.

Return Value:

    LRESULT - Returns the result value returned by the user-mode window
              procedure, or 0 if the window procedure is missing or the
              callback fails.

--*/
{
    UCHAR Payload[sizeof(WNDPROC_CALLBACK_ARGS) + sizeof(CREATESTRUCTA) + 128];
    PWNDPROC_CALLBACK_ARGS Args = (PWNDPROC_CALLBACK_ARGS)Payload;
    ULONG InputLength = sizeof(WNDPROC_CALLBACK_ARGS);
    PVOID CallbackResult = NULL;
    ULONG ResultLength = 0;
    VEASTATUS Status;

    if (!Wnd || !Wnd->lpfnWndProc)
    {
        return 0;
    }

    RtlZeroMemory(Payload, sizeof(Payload));

    Args->lpfnWndProc = Wnd->lpfnWndProc;
    Args->hWnd        = Wnd->hWnd;
    Args->Msg         = Msg;
    Args->wParam      = wParam;
    Args->lParam      = lParam;

    if (Msg == WM_CREATE && lParam == 0)
    {
        ULONG TitleLen = 0;
        ULONG ClassLen = 0;

        while (TitleLen < (sizeof(Wnd->Title) - 1) && Wnd->Title[TitleLen] != '\0')
        {
            TitleLen++;
        }
        while (ClassLen < (sizeof(Wnd->ClassName) - 1) && Wnd->ClassName[ClassLen] != '\0')
        {
            ClassLen++;
        }

        ULONG OffsetCs    = sizeof(WNDPROC_CALLBACK_ARGS);
        ULONG OffsetTitle = OffsetCs + sizeof(CREATESTRUCTA);
        ULONG OffsetClass = OffsetTitle + TitleLen + 1;
        InputLength       = OffsetClass + ClassLen + 1;

        PCREATESTRUCTA Cs = (PCREATESTRUCTA)(Payload + OffsetCs);
        Cs->lpCreateParams = Wnd->lpCreateParams;
        Cs->hInstance      = Wnd->hInstance;
        Cs->hMenu          = Wnd->hMenu;
        Cs->hwndParent     = Wnd->hWndParent;
        Cs->cx             = (INT)Wnd->Width;
        Cs->cy             = (INT)Wnd->Height;
        Cs->x              = (INT)Wnd->X;
        Cs->y              = (INT)Wnd->Y;
        Cs->style          = (LONG)Wnd->Style;
        Cs->dwExStyle      = Wnd->ExStyle;

        /* Simpan offset relatif terhadap awal CREATESTRUCTA */
        Cs->lpszName       = (PSTR)(ULONG_PTR)(OffsetTitle - OffsetCs);
        Cs->lpszClass      = (PSTR)(ULONG_PTR)(OffsetClass - OffsetCs);

        if (TitleLen > 0)
        {
            RtlCopyMemory(Payload + OffsetTitle, Wnd->Title, TitleLen);
        }
        Payload[OffsetTitle + TitleLen] = '\0';

        if (ClassLen > 0)
        {
            RtlCopyMemory(Payload + OffsetClass, Wnd->ClassName, ClassLen);
        }
        Payload[OffsetClass + ClassLen] = '\0';

        /* Tandai lParam dengan offset relatif CREATESTRUCTA dari awal Args */
        Args->lParam = (LPARAM)OffsetCs;
    }

    Status = KsUserModeCallback(
        USER_CALLBACK_WNDPROC,
        Payload,
        InputLength,
        &CallbackResult,
        &ResultLength
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[VEAPI/WND] KeUserModeCallback gagal (Msg=0x%X): 0x%08X\n",
                 Msg, Status);
        return 0;
    }

    return (LRESULT)(ULONG_PTR)CallbackResult;
}

VOID
VEAPI
WndCleanupProcessWindows(
    IN PEPROCESS Process
)
/*++

Routine Description:

    This routine unlinks and invalidates all window objects owned by the
    specified process from the global window list, detaches them from their
    associated desktops, clears any dangling foreground, focus, capture, or
    caption button references, and releases each window object's base
    reference.

Arguments:

    Process - Supplies a pointer to the terminating process whose windows are
              to be cleaned up.

Return Value:

    None.

--*/
{
    LIST_ENTRY VictimsHead;
    PLIST_ENTRY Entry;
    KIRQL OldIrql;

    if (!Process)
    {
        return;
    }

    WndEnsureListsInitialized();
    InitializeListHead(&VictimsHead);

    /* 1. Cabut semua window milik Process dari gWindowListHead di bawah lock */
    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    Entry = gWindowListHead.Flink;
    while (Entry != &gWindowListHead)
    {
        PLIST_ENTRY Next = Entry->Flink;
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);

        if (Wnd->OwnerProcess == Process)
        {
            RemoveEntryList(&Wnd->GlobalListEntry);
            Wnd->Magic = 0;
            Wnd->Used  = FALSE;
            InsertTailList(&VictimsHead, &Wnd->GlobalListEntry);
        }

        Entry = Next;
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    /* 2. Lepas dari Desktop & kembalikan memori ke pool tanpa nested lock */
    while (!IsListEmpty(&VictimsHead))
    {
        Entry = RemoveHeadList(&VictimsHead);
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
        PDESKTOP_OBJECT Desktop = Wnd->Desktop;

        if (Desktop)
        {
            KsAcquireSpinLock(&Wnd->Desktop->Lock, &OldIrql);

            RemoveEntryList(&Wnd->DesktopListEntry);
            if (Wnd->Desktop->ForegroundWindow == Wnd)
            {
                Wnd->Desktop->ForegroundWindow = NULL;
            }
            if (Wnd->Desktop->FocusWindow == Wnd)
            {
                Wnd->Desktop->FocusWindow = NULL;
            }

            KsReleaseSpinLock(&Wnd->Desktop->Lock, OldIrql);
        }

        if (IptFocusWindow == Wnd->hWnd)
        {
            IptFocusWindow = NULL;
        }
        if (gCaptureWindow == Wnd->hWnd)
        {
            gCaptureWindow = NULL;
        }

        if (gHotCaptionWnd == Wnd->hWnd) 
        { 
            gHotCaptionWnd = NULL; gHotCaptionBtn = CAPTION_BTN_NONE; 
        }

        if (gPressedCaptionWnd == Wnd->hWnd)
        {
            gPressedCaptionWnd = NULL;
        }

        WndDereference(Wnd);
    }
}

BOOLEAN
VEAPI
VeaUserRegisterClass(
    IN CONST PWNDCLASS UserWndClass
)
/*++

Routine Description:

    This routine validates and registers a new window class in the global
    class list. When invoked from user mode, it safely probes and copies the
    WNDCLASS structure and class name string, checks for duplicate class
    names within the global or current process scope, and inserts the new
    class object under the global class list spinlock.

Arguments:

    UserWndClass - Supplies a pointer to a WNDCLASS structure describing the
                   window class to register.

Return Value:

    BOOLEAN - Returns TRUE if the class was successfully registered;
              otherwise returns FALSE.

--*/
{
    WNDCLASS LocalClass;
    CHAR LocalClassName[64];
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    PLIST_ENTRY Entry;
    KIRQL OldIrql;

    if (!UserWndClass)
    {
        return FALSE;
    }

    WndEnsureListsInitialized();

    if (IsUser)
    {
        if ((ULONG_PTR)UserWndClass >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)UserWndClass + sizeof(WNDCLASS)) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return FALSE;
        }

        _VEA_TRY
        {
            RtlCopyMemory(&LocalClass, UserWndClass, sizeof(WNDCLASS));
        }
        _VEA_EXCEPT(1)
        {
            return FALSE;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(&LocalClass, UserWndClass, sizeof(WNDCLASS));
    }

    if (!LocalClass.lpfnWndProc || !LocalClass.lpszClassName)
    {
        return FALSE;
    }

    if (IsUser && (ULONG_PTR)LocalClass.lpfnWndProc >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
    {
        return FALSE;
    }

    if (!WndCopyStringSafe(LocalClassName, LocalClass.lpszClassName, sizeof(LocalClassName), IsUser) ||
        LocalClassName[0] == '\0')
    {
        return FALSE;
    }

    /* Cek cepat sebelum alokasi */
    if (WndFindClassByName(LocalClassName, (PEPROCESS)PtGetCurrentProcess()) != NULL)
    {
        return FALSE;
    }

    PWND_CLASS_OBJECT Cls = (PWND_CLASS_OBJECT)WND_POOL_ALLOC(sizeof(WND_CLASS_OBJECT));
    if (!Cls)
    {
        return FALSE;
    }

    RtlZeroMemory(Cls, sizeof(WND_CLASS_OBJECT));
    Cls->Magic         = WND_CLASS_MAGIC;
    Cls->Used          = TRUE;
    Cls->lpfnWndProc   = LocalClass.lpfnWndProc;
    Cls->Style         = LocalClass.Style;
    Cls->cbClsExtra    = LocalClass.cbClsExtra;
    Cls->cbWndExtra    = LocalClass.cbWndExtra;
    Cls->hInstance     = LocalClass.hInstance;
    Cls->hbrBackground = LocalClass.hbrBackground;
    Cls->OwnerProcess  = (LocalClass.Style & CS_GLOBALCLASS)
                       ? NULL
                       : (PEPROCESS)PtGetCurrentProcess();
    RtlCopyMemory(Cls->ClassName, LocalClassName, sizeof(LocalClassName));

    /* Cek ulang di dalam lock untuk mencegah race condition antar thread */
    KsAcquireSpinLock(&gClassListLock, &OldIrql);
    for (Entry = gClassListHead.Flink; Entry != &gClassListHead; Entry = Entry->Flink)
    {
        PWND_CLASS_OBJECT Existing = CONTAINING_RECORD(Entry, WND_CLASS_OBJECT, ListEntry);
        if (Existing->Magic == WND_CLASS_MAGIC &&
            Existing->Used &&
            ((Existing->Style & CS_GLOBALCLASS) ||
             Existing->OwnerProcess == (PEPROCESS)PtGetCurrentProcess()) &&
            WndStrEqual(Existing->ClassName, LocalClassName))
        {
            KsReleaseSpinLock(&gClassListLock, OldIrql);
            WND_POOL_FREE(Cls);
            return FALSE;
        }
    }
    InsertTailList(&gClassListHead, &Cls->ListEntry);
    KsReleaseSpinLock(&gClassListLock, OldIrql);

    KdPrintf("[VEAPI/WND] Registered class '%s' -> WndProc=0x%p\n",
             Cls->ClassName, Cls->lpfnWndProc);
    return TRUE;
}

HWND
VEAPI
VeaUserCreateWindow(
    IN PCHAR ClassName,
    IN PCHAR WindowName,
    IN ULONG Style
)
/*++

Routine Description:

    This routine creates a window with default position, dimensions, and zero
    extended style by delegating to VeaUserCreateWindowEx.

Arguments:

    ClassName - Supplies the name of the registered window class.

    WindowName - Supplies the optional title text for the window.

    Style - Supplies the window style flags.

Return Value:

    HWND - Returns the handle to the newly created window, or NULL on
           failure.

--*/
{
    return VeaUserCreateWindowEx(
        0,
        ClassName,
        WindowName,
        Style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        NULL,
        NULL,
        NULL,
        NULL
    );
}

LRESULT
VEAPI
VeaUserDispatchMessage(
    IN CONST PMESSAGE_QUEUE UserMsg
)
/*++

Routine Description:

    This routine dispatches a retrieved message to the appropriate user-mode
    window procedure or timer callback. When called from user mode, it safely
    copies the message structure, handles WM_TIMER messages with an explicit
    timer procedure in lParam, and otherwise invokes the target window's
    procedure.

Arguments:

    UserMsg - Supplies a pointer to a MESSAGE_QUEUE structure containing the
              message to dispatch.

Return Value:

    LRESULT - Returns the value returned by the window or timer procedure, or
              0 if the message is invalid or the target window is not found.

--*/
{
    MESSAGE_QUEUE LocalMsg;
    PWND_OBJECT Wnd;
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);

    if (!UserMsg)
    {
        return 0;
    }

    if (IsUser)
    {
        if ((ULONG_PTR)UserMsg >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)UserMsg + sizeof(MESSAGE_QUEUE)) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return 0;
        }

        _VEA_TRY
        {
            RtlCopyMemory(&LocalMsg, UserMsg, sizeof(MESSAGE_QUEUE));
        }
        _VEA_EXCEPT(1)
        {
            return 0;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(&LocalMsg, UserMsg, sizeof(MESSAGE_QUEUE));
    }

    if (LocalMsg.hwnd != NULL)
    {
        Wnd = WndLookupHandle(LocalMsg.hwnd);
        if (!Wnd)
        {
            return 0;
        }
    }
    else
    {
        Wnd = NULL;
    }

    if (LocalMsg.message == WM_TIMER && LocalMsg.lParam != 0)
    {
        WNDPROC_CALLBACK_ARGS CbArgs;
        PVOID CallbackResult = NULL;
        ULONG ResultLength = 0;

        RtlZeroMemory(&CbArgs, sizeof(CbArgs));
        CbArgs.lpfnWndProc = (PVOID)(ULONG_PTR)LocalMsg.lParam;
        CbArgs.hWnd        = LocalMsg.hwnd;
        CbArgs.Msg         = WM_TIMER;
        CbArgs.wParam      = LocalMsg.wParam;
        CbArgs.lParam      = (LPARAM)LocalMsg.time;

        KsUserModeCallback(
            USER_CALLBACK_WNDPROC,
            &CbArgs,
            sizeof(CbArgs),
            &CallbackResult,
            &ResultLength
        );

        if (Wnd)
        {
            WndDereference(Wnd);
        }
        return (LRESULT)(ULONG_PTR)CallbackResult;
    }

    if (!Wnd)
    {
        return 0;
    }

    LRESULT Result = WndInvokeUserProc(Wnd, LocalMsg.message, LocalMsg.wParam, LocalMsg.lParam);
    WndDereference(Wnd);
    return Result;
}

HWND
VEAPI
VeaUserCreateWindowEx(
    IN ULONG     ExStyle,
    IN PCHAR     ClassName,
    IN PCHAR     WindowName,
    IN ULONG     Style,
    IN INT       X,
    IN INT       Y,
    IN INT       Width,
    IN INT       Height,
    IN HWND      hWndParent,
    IN HMENU     hMenu,
    IN HINSTANCE hInstance,
    IN PVOID     lpParam
)
/*++

Routine Description:

    This routine allocates, initializes, and registers a new window object
    with the specified extended style, class, geometry, and parent. It links
    the window into the global and target desktop window lists, updates
    foreground and input focus for top-level windows, dispatches WM_NCCREATE
    and WM_CREATE to the user-mode window procedure, and notifies the shell
    hook if the window is taskbar-eligible. If creation callbacks fail, the
    window is destroyed.

Arguments:

    ExStyle - Supplies the extended window style flags.

    ClassName - Supplies the name of the registered window class.

    WindowName - Supplies the optional title text for the window.

    Style - Supplies the standard window style flags.

    X - Supplies the initial horizontal position, or CW_USEDEFAULT.

    Y - Supplies the initial vertical position, or CW_USEDEFAULT.

    Width - Supplies the initial window width, or CW_USEDEFAULT.

    Height - Supplies the initial window height, or CW_USEDEFAULT.

    hWndParent - Supplies the handle to the parent window (required if
                 WS_CHILD is set).

    hMenu - Supplies the handle to the menu or child-window identifier.

    hInstance - Supplies the handle to the module instance associated with
                the window.

    lpParam - Supplies an optional creation parameter passed in CREATESTRUCTA.

Return Value:

    HWND - Returns the handle to the newly created window, or NULL if
           validation, allocation, or creation callbacks fail.

--*/
{
    CHAR LocalClass[64];
    CHAR LocalTitle[64];
    PWND_CLASS_OBJECT FoundClass = NULL;
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    BOOLEAN IsChild = ((Style & WS_CHILD) != 0);
    KIRQL OldIrql;
    HWND CreatedHwnd;

    if (!ClassName)
    {
        return NULL;
    }

    WndEnsureListsInitialized();

    if (!WndCopyStringSafe(LocalClass, ClassName, sizeof(LocalClass), IsUser) ||
        LocalClass[0] == '\0')
    {
        return NULL;
    }

    if (WindowName)
    {
        if (!WndCopyStringSafe(LocalTitle, WindowName, sizeof(LocalTitle), IsUser))
        {
            return NULL;
        }
    }
    else
    {
        LocalTitle[0] = '\0';
    }

    FoundClass = WndFindClassByName(LocalClass, (PEPROCESS)PtGetCurrentProcess());
    if (!FoundClass)
    {
        KdPrintf("[VEAPI/WND] CreateWindowEx gagal: class '%s' tidak ditemukan\n", LocalClass);
        return NULL;
    }

    /* Validasi parent jika WS_CHILD */
    PWND_OBJECT ParentWnd = NULL;
    PDESKTOP_OBJECT ParentDesktop = NULL;
    if (IsChild)
    {
        ParentWnd = WndLookupHandle(hWndParent);
        if (!ParentWnd)
        {
            KdPrintf("[VEAPI/WND] CreateWindowEx failed: hWndParent=%p invalid\n", hWndParent);
            return NULL;
        }

        /* Cegah child window lintas proses agar DestroyWindow tidak macet */
        if (ParentWnd->OwnerProcess != (PEPROCESS)PtGetCurrentProcess())
        {
            KdPrintf("[VEAPI/WND] CreateWindowEx failed: hWndParent=%p is on another process\n", hWndParent);
            WndDereference(ParentWnd);
            return NULL;
        }

        ParentDesktop = ParentWnd->Desktop;
        WndDereference(ParentWnd);
    }

    PWND_OBJECT Wnd = (PWND_OBJECT)WND_POOL_ALLOC(sizeof(WND_OBJECT));
    if (!Wnd)
    {
        return NULL;
    }

    RtlZeroMemory(Wnd, sizeof(WND_OBJECT));

    Wnd->Magic          = WND_OBJECT_MAGIC;
    Wnd->Used           = TRUE;
    Wnd->hWnd           = (HWND)Wnd;
    Wnd->hWndParent     = hWndParent;
    Wnd->hMenu          = hMenu;
    Wnd->hInstance      = hInstance;
    Wnd->lpCreateParams = lpParam;
    Wnd->lpfnWndProc    = FoundClass->lpfnWndProc;
    Wnd->Style          = Style;
    Wnd->ExStyle        = ExStyle;
    Wnd->hbrBackground  = FoundClass->hbrBackground;
    Wnd->OwnerProcess   = (PEPROCESS)PtGetCurrentProcess();
    Wnd->OwnerThread    = (PETHREAD)PtGetCurrentThread();
    Wnd->RefCount       = 1;

    /* Resolusi posisi & ukuran */
    if (!IsChild)
    {
        ULONG CascadeStep = (gWindowCascadeCount++) % 10;
        Wnd->X      = (X == CW_USEDEFAULT) ? (120 + (LONG)(CascadeStep * 24)) : (LONG)X;
        Wnd->Y      = (Y == CW_USEDEFAULT) ? (90 + (LONG)(CascadeStep * 24))  : (LONG)Y;
        Wnd->Width  = (Width == CW_USEDEFAULT || Width <= 0)   ? 420 : (ULONG)Width;
        Wnd->Height = (Height == CW_USEDEFAULT || Height <= 0) ? 280 : (ULONG)Height;
    }
    else
    {
        Wnd->X      = (X == CW_USEDEFAULT) ? 0 : (LONG)X;
        Wnd->Y      = (Y == CW_USEDEFAULT) ? 0 : (LONG)Y;
        Wnd->Width  = (Width > 0)  ? (ULONG)Width  : 0;
        Wnd->Height = (Height > 0) ? (ULONG)Height : 0;
    }

    RtlCopyMemory(Wnd->ClassName, LocalClass, sizeof(LocalClass));
    RtlCopyMemory(Wnd->Title, LocalTitle, sizeof(LocalTitle));

    /* Daftarkan ke global window list */
    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    InsertTailList(&gWindowListHead, &Wnd->GlobalListEntry);
    Wnd->NeedsPaint = WndIsEffectivelyVisibleLocked(Wnd);
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    /* Tentukan Desktop target: ikuti parent jika child, atau rpdesk thread saat ini */
    PTHREADINFO pti = (PTHREADINFO)PtGetCurrentThreadVeapiThread();
    if (pti && pti->rpdesk == NULL)
    {
        DskEnsureThreadDesktop(pti);
    }

    PDESKTOP_OBJECT TargetDesktop = ParentDesktop
                                    ? ParentDesktop
                                    : ((pti && pti->rpdesk) ? pti->rpdesk : GpActiveDesktop);

    /* Cantolkan ke Desktop target */
    if (TargetDesktop)
    {
        KsAcquireSpinLock(&TargetDesktop->Lock, &OldIrql);

        Wnd->Desktop = TargetDesktop;
        InsertTailList(&TargetDesktop->WindowListHead, &Wnd->DesktopListEntry);

        /* Hanya top-level window yang mengambil alih fokus di desktop-nya sendiri */
        if (!IsChild)
        {
            TargetDesktop->ForegroundWindow = Wnd;
            TargetDesktop->FocusWindow      = Wnd;
        }

        KsReleaseSpinLock(&TargetDesktop->Lock, OldIrql);
    }

    if (pti)
    {
        pti->cWindows++;
    }

    /* Update fokus input global HANYA jika window dibuat di desktop yang sedang aktif */
    if (!IsChild && TargetDesktop == GpActiveDesktop)
    {
        IptFocusWindow = Wnd->hWnd;
    }

    KdPrintf("[VEAPI/WND] Created HWND=%p ('%s', class='%s') at (%ld,%ld %lux%lu)\n",
             Wnd->hWnd, Wnd->Title, Wnd->ClassName,
             Wnd->X, Wnd->Y, Wnd->Width, Wnd->Height);

    /* Tembak callback WM_CREATE pertama kali ke Ring 3 */
    CreatedHwnd = Wnd->hWnd;

    if (!WndInvokeUserProc(Wnd, WM_NCCREATE, 0, 0))
    {
        VeaUserDestroyWindow(CreatedHwnd);
        return NULL;
    }

    LRESULT Result = WndInvokeUserProc(Wnd, WM_CREATE, 0, 0);

    if (Result == (LRESULT)-1)
    {
        VeaUserDestroyWindow(CreatedHwnd);
        return NULL;
    }

    PWND_OBJECT AliveWnd = WndLookupHandle(CreatedHwnd);
    if (!AliveWnd)
    {
        return NULL;
    }

    if (WndIsTaskbarEligible(AliveWnd->Desktop, AliveWnd))
    {
        WndNotifyShellHook(AliveWnd->Desktop, HSHELL_WINDOWCREATED, CreatedHwnd);
        WndNotifyShellHook(AliveWnd->Desktop, HSHELL_WINDOWACTIVATED, CreatedHwnd);
    }

    WndDereference(AliveWnd);
    return CreatedHwnd;
}

LRESULT
VEAPI
VeaUserSendMessage(
    IN HWND   hWnd,
    IN UINT   Msg,
    IN WPARAM wParam,
    IN LPARAM lParam
)
/*++

Routine Description:

    This routine synchronously sends a message to the specified window. If the
    calling thread owns the window, the user-mode window procedure is invoked
    directly; otherwise, the message is queued to the owning thread's sent
    message list via MsgSendMessageToThread.

Arguments:

    hWnd - Supplies the handle to the target window.

    Msg - Supplies the window message identifier.

    wParam - Supplies additional message-specific information.

    lParam - Supplies additional message-specific information.

Return Value:

    LRESULT - Returns the result of the message processing, or 0 if the
              window handle, window procedure, or owning thread is invalid.

--*/
{
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return 0;
    }

    if (!Wnd->lpfnWndProc || !Wnd->OwnerThread)
    {
        WndDereference(Wnd);
        return 0;
    }

    PETHREAD TargetThread = Wnd->OwnerThread;

    /* Jalur 1: Thread pemilik window sama dengan pemanggil -> panggil langsung */
    if (TargetThread == (PETHREAD)PtGetCurrentThread())
    {
        LRESULT Result = WndInvokeUserProc(Wnd, Msg, wParam, lParam);
        WndDereference(Wnd);
        return Result;
    }

    /* Jalur 2: Beda thread atau beda proses -> titipkan lewat SentListHead */
    WndDereference(Wnd);
    return MsgSendMessageToThread(TargetThread, hWnd, Msg, wParam, lParam);
}

VOID
VEAPI
WndInvalidateChildWindows(
    IN HWND hWndParent
)
/*++

Routine Description:

    This routine scans the global window list under lock and marks all
    effectively visible descendant child windows of the specified parent
    window (up to 64 levels deep) as needing a repaint.

Arguments:

    hWndParent - Supplies the handle to the ancestor window whose visible
                 descendants should be invalidated.

Return Value:

    None.

--*/
{
    PLIST_ENTRY Entry;
    KIRQL OldIrql;

    WndEnsureListsInitialized();

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    for (Entry = gWindowListHead.Flink; Entry != &gWindowListHead; Entry = Entry->Flink)
    {
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
        if (Wnd->Magic != WND_OBJECT_MAGIC || !Wnd->Used || !(Wnd->Style & WS_CHILD))
        {
            continue;
        }

        PWND_OBJECT Cur = Wnd;
        ULONG Depth = 0;
        while (Cur && (Cur->Style & WS_CHILD) && Cur->hWndParent != NULL && Depth < 64)
        {
            if (Cur->hWndParent == hWndParent)
            {
                if (WndIsEffectivelyVisibleLocked(Wnd))
                {
                    Wnd->NeedsPaint = TRUE;
                }
                break;
            }
            Cur = WndLookupHandleLocked(Cur->hWndParent);
            Depth++;
        }
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);
}

VOID
VEAPI
UserInvalidateAllWindows(VOID)
/*++

Routine Description:

    This routine marks all effectively visible windows on the active desktop
    as needing a repaint and triggers a paint notification if at least one
    window was invalidated.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PLIST_ENTRY Entry;
    KIRQL OldIrql;
    ULONG Count = 0;

    WndEnsureListsInitialized();

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    for (Entry = gWindowListHead.Flink; Entry != &gWindowListHead; Entry = Entry->Flink)
    {
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
        if (Wnd->Magic == WND_OBJECT_MAGIC &&
            Wnd->Used &&
            WndIsEffectivelyVisibleLocked(Wnd) &&
            (GpActiveDesktop == NULL || Wnd->Desktop == GpActiveDesktop))
        {
            Wnd->NeedsPaint = TRUE;
            Count++;
        }
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    if (Count > 0)
    {
        MsgNotifyPaintAll();
    }
}

HWND
VEAPI
VeaUserSetFocus(
    IN HWND hWnd
)
/*++

Routine Description:

    This routine sets keyboard input focus to the specified window (which
    must belong to the calling process, or NULL to clear focus), updates the
    active desktop's focus window and global input focus handle, and
    synchronously dispatches WM_KILLFOCUS and WM_SETFOCUS messages if the
    focus changed.

Arguments:

    hWnd - Supplies the handle to the window that will receive keyboard
           focus, or NULL to remove focus.

Return Value:

    HWND - Returns the handle to the window that previously had keyboard
           focus, or NULL if no window had focus or validation failed.

--*/
{
    PWND_OBJECT NewWnd = NULL;
    PWND_OBJECT OldWnd = NULL;
    HWND OldHwnd = NULL;
    HWND NewHwnd = NULL;
    KIRQL OldIrql;

    if (hWnd != NULL)
    {
        NewWnd = WndLookupHandle(hWnd);
        if (!NewWnd)
        {
            return NULL;
        }

        /* Standar Win32: SetFocus hanya berlaku untuk window milik proses pemanggil */
        if (NewWnd->OwnerProcess != (PEPROCESS)PtGetCurrentProcess())
        {
            WndDereference(NewWnd);
            return NULL;
        }

        NewHwnd = NewWnd->hWnd;
    }

    PDESKTOP_OBJECT Desktop = GpActiveDesktop;
    if (Desktop != NULL)
    {
        KsAcquireSpinLock(&Desktop->Lock, &OldIrql);

        OldWnd = (PWND_OBJECT)Desktop->FocusWindow;
        if (OldWnd != NULL && OldWnd->Magic == WND_OBJECT_MAGIC && OldWnd->Used)
        {
            OldHwnd = OldWnd->hWnd;
        }

        Desktop->FocusWindow = NewWnd;

        KsReleaseSpinLock(&Desktop->Lock, OldIrql);
    }
    else
    {
        OldHwnd = IptFocusWindow;
    }

    IptFocusWindow = NewHwnd;

    if (NewWnd != NULL)
    {
        WndDereference(NewWnd);
    }

    if (OldHwnd != NewHwnd)
    {
        if (OldHwnd != NULL)
        {
            VeaUserSendMessage(OldHwnd, WM_KILLFOCUS, (WPARAM)NewHwnd, 0);
        }
        if (NewHwnd != NULL)
        {
            VeaUserSendMessage(NewHwnd, WM_SETFOCUS, (WPARAM)OldHwnd, 0);
        }
    }

    return OldHwnd;
}

BOOLEAN
VEAPI
VeaUserShowWindow(
    IN HWND hWnd,
    IN INT  nCmdShow
)
/*++

Routine Description:

    This routine changes the visibility and display state (hide, maximize,
    minimize, restore, or show) of the specified window. It updates window
    style flags, saved normal geometry, desktop foreground and input focus,
    shell hook notifications, shadow restoration, and repaint invalidation
    as appropriate for the requested command.

Arguments:

    hWnd - Supplies the handle to the target window.

    nCmdShow - Supplies the display command (such as SW_HIDE, SW_MAXIMIZE,
               SW_MINIMIZE, SW_RESTORE, or SW_SHOWNORMAL).

Return Value:

    BOOLEAN - Returns TRUE if the window was previously visible; otherwise
              returns FALSE.

--*/
{
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    KIRQL ListIrql, DeskIrql;
    if (!Wnd)
    {
        return FALSE;
    }

    BOOLEAN WasVisible = ((Wnd->Style & WS_VISIBLE) != 0);
    BOOLEAN IsChild    = ((Wnd->Style & WS_CHILD) != 0);
    HWND OldFocusHwnd  = NULL;
    HWND NewFocusHwnd  = NULL;

    if (nCmdShow == SW_HIDE)
    {
        if (WasVisible)
        {
            PDESKTOP_OBJECT Desktop    = Wnd->Desktop;
            PWND_OBJECT NextTop        = NULL;
            PWND_OBJECT FallbackAnc    = NULL;
            BOOLEAN WasTaskbarEligible = WndIsTaskbarEligible(Desktop, Wnd);
            BOOLEAN WasFg              = FALSE;
            HWND NextActiveTaskHwnd    = NULL;

            if (gCaptureWindow == hWnd)
            {
                gCaptureWindow = NULL;
            }

            KsAcquireSpinLock(&gWindowListLock, &ListIrql);
            Wnd->Style &= ~WS_VISIBLE;
            Wnd->NeedsPaint = FALSE;

            PLIST_ENTRY Entry;
            for (Entry = gWindowListHead.Flink; Entry != &gWindowListHead; Entry = Entry->Flink)
            {
                PWND_OBJECT Child = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
                if (Child->Magic == WND_OBJECT_MAGIC && Child->Used &&
                    WndIsDescendantLocked(Wnd, Child))
                {
                    Child->NeedsPaint = FALSE;
                    if (gCaptureWindow == Child->hWnd)
                    {
                        gCaptureWindow = NULL;
                    }
                }
            }

            if (IsChild)
            {
                FallbackAnc = WndFindVisibleAncestorLocked(Wnd);
            }
            else
            {
                NextTop = WndFindNextTopLevelLocked(Desktop, Wnd);
            }

            if (Desktop != NULL)
            {
                KsAcquireSpinLock(&Desktop->Lock, &DeskIrql);

                PWND_OBJECT PrevFg    = (PWND_OBJECT)Desktop->ForegroundWindow;
                PWND_OBJECT PrevFocus = (PWND_OBJECT)Desktop->FocusWindow;
                BOOLEAN HadFocus = (PrevFocus != NULL &&
                                    PrevFocus->Magic == WND_OBJECT_MAGIC &&
                                    PrevFocus->Used &&
                                    (PrevFocus == Wnd || WndIsDescendantLocked(Wnd, PrevFocus)));

                if (!IsChild)
                {
                    WasFg = (PrevFg == Wnd);
                    if (WasFg)
                    {
                        Desktop->ForegroundWindow = NextTop;
                    }

                    if (HadFocus || WasFg)
                    {
                        if (HadFocus)
                        {
                            OldFocusHwnd = PrevFocus->hWnd;
                        }
                        Desktop->FocusWindow = NextTop;
                        NewFocusHwnd = NextTop ? NextTop->hWnd : NULL;

                        if (GpActiveDesktop == NULL || Desktop == GpActiveDesktop)
                        {
                            IptFocusWindow = NewFocusHwnd;
                        }
                    }
                }
                else if (HadFocus)
                {
                    OldFocusHwnd = PrevFocus->hWnd;
                    Desktop->FocusWindow = FallbackAnc;
                    NewFocusHwnd = FallbackAnc ? FallbackAnc->hWnd : NULL;

                    if (GpActiveDesktop == NULL || Desktop == GpActiveDesktop)
                    {
                        IptFocusWindow = NewFocusHwnd;
                    }
                }

                KsReleaseSpinLock(&Desktop->Lock, DeskIrql);
            }
            else if (IptFocusWindow != NULL)
            {
                PWND_OBJECT FocusObj = WndLookupHandleLocked(IptFocusWindow);
                if (FocusObj == Wnd || WndIsDescendantLocked(Wnd, FocusObj))
                {
                    OldFocusHwnd = IptFocusWindow;
                    PWND_OBJECT Fallback = IsChild ? FallbackAnc : NextTop;
                    NewFocusHwnd   = Fallback ? Fallback->hWnd : NULL;
                    IptFocusWindow = NewFocusHwnd;
                }
            }

            if (NextTop != NULL)
            {
                NextTop->NeedsPaint = TRUE;
                if (WndIsTaskbarEligible(Desktop, NextTop))
                {
                    NextActiveTaskHwnd = NextTop->hWnd;
                }
            }
            KsReleaseSpinLock(&gWindowListLock, ListIrql);

            if (OldFocusHwnd != NULL && OldFocusHwnd != NewFocusHwnd)
            {
                MsgPostToWindow(OldFocusHwnd, WM_KILLFOCUS, (WPARAM)NewFocusHwnd, 0, FALSE);
            }
            if (NewFocusHwnd != NULL && NewFocusHwnd != OldFocusHwnd)
            {
                MsgPostToWindow(NewFocusHwnd, WM_SETFOCUS, (WPARAM)OldFocusHwnd, 0, FALSE);
            }

            if (WasTaskbarEligible)
            {
                WndNotifyShellHook(Desktop, HSHELL_WINDOWDESTROYED, Wnd->hWnd);
            }
            if (!IsChild && WasFg)
            {
                WndNotifyShellHook(Desktop, HSHELL_WINDOWACTIVATED, NextActiveTaskHwnd);
            }

            if (!IsChild && (GpActiveDesktop == NULL || Wnd->Desktop == GpActiveDesktop))
            {
                WndRestoreWindowBottomShadow(Wnd, TRUE);

                EngHidePointer();
                EngFillDesktopRect(Wnd->X, Wnd->Y, Wnd->Width, Wnd->Height,
                                   WndGetDesktopBgColor(GpActiveDesktop));
                EngShowPointer();

                if (NextTop != NULL)
                {
                    WndInvalidateChildWindows(NextTop->hWnd);
                }
                UserInvalidateAllWindows();
            }
        }
    }
    else if (!IsChild && (nCmdShow == SW_MAXIMIZE || nCmdShow == SW_SHOWMAXIMIZED))
    {
        ULONG ScrW = GpPrimaryDisplay.PrimarySurface.Width;
        ULONG ScrH = GpPrimaryDisplay.PrimarySurface.Height;
        if (ScrW == 0) ScrW = 800;
        if (ScrH == 0) ScrH = 600;

        LONG  WorkX = 0;
        LONG  WorkY = 0;
        ULONG WorkW = ScrW;
        ULONG WorkH = ScrH;

        if (Wnd->Desktop != NULL &&
            Wnd->Desktop->TaskmanWindow != NULL &&
            Wnd->hWnd != Wnd->Desktop->TaskmanWindow)
        {
            PWND_OBJECT TaskmanWnd = WndLookupHandle(Wnd->Desktop->TaskmanWindow);
            if (TaskmanWnd != NULL)
            {
                if (TaskmanWnd->Style & WS_VISIBLE)
                {
                    if (TaskmanWnd->Y > 0 && (ULONG)TaskmanWnd->Y < ScrH)
                    {
                        WorkH = (ULONG)TaskmanWnd->Y;
                    }
                    else if (TaskmanWnd->Y == 0 && TaskmanWnd->Height < ScrH)
                    {
                        WorkY = (LONG)TaskmanWnd->Height;
                        WorkH = ScrH - TaskmanWnd->Height;
                    }
                }
                WndDereference(TaskmanWnd);
            }
        }

        if (GpActiveDesktop == NULL || Wnd->Desktop == GpActiveDesktop)
        {
            WndRestoreWindowBottomShadow(Wnd, TRUE);
        }

        KsAcquireSpinLock(&gWindowListLock, &ListIrql);
        if (!(Wnd->Style & (WS_MAXIMIZE | WS_MINIMIZE)))
        {
            Wnd->NormalX      = Wnd->X;
            Wnd->NormalY      = Wnd->Y;
            Wnd->NormalWidth  = Wnd->Width;
            Wnd->NormalHeight = Wnd->Height;
        }
        Wnd->Style = (Wnd->Style & ~WS_MINIMIZE) | WS_MAXIMIZE | WS_VISIBLE;
        Wnd->X          = WorkX;
        Wnd->Y          = WorkY;
        Wnd->Width      = WorkW;
        Wnd->Height     = WorkH;
        Wnd->NeedsPaint = TRUE;
        KsReleaseSpinLock(&gWindowListLock, ListIrql);

        if (!WasVisible && WndIsTaskbarEligible(Wnd->Desktop, Wnd))
        {
            WndNotifyShellHook(Wnd->Desktop, HSHELL_WINDOWCREATED, Wnd->hWnd);
        }

        VeaUserSetForegroundWindow(Wnd->hWnd);
        WndInvalidateChildWindows(Wnd->hWnd);
        MsgNotifyPaintAll();
    }
    else if (!IsChild && (nCmdShow == SW_MINIMIZE || nCmdShow == SW_SHOWMINIMIZED))
    {
        PDESKTOP_OBJECT Desktop = Wnd->Desktop;
        BOOLEAN HasTaskbar      = FALSE;

        if (Desktop && Desktop->TaskmanWindow != NULL)
        {
            PWND_OBJECT Taskman = WndLookupHandle(Desktop->TaskmanWindow);
            if (Taskman != NULL)
            {
                HasTaskbar = TRUE;
                WndDereference(Taskman);
            }
            else
            {
                Desktop->TaskmanWindow = NULL;
            }
        }

        ULONG ScrH = GpPrimaryDisplay.PrimarySurface.Height;
        if (ScrH == 0) ScrH = 600;

        LONG  OldX = Wnd->X;
        LONG  OldY = Wnd->Y;
        ULONG OldW = Wnd->Width;
        ULONG OldH = Wnd->Height;

        if (GpActiveDesktop == NULL || Desktop == GpActiveDesktop)
        {
            WndRestoreWindowBottomShadow(Wnd, TRUE);
        }

        PWND_OBJECT NextTop     = NULL;
        BOOLEAN WasFg           = FALSE;
        HWND NextActiveTaskHwnd = NULL;

        KsAcquireSpinLock(&gWindowListLock, &ListIrql);
        if (!(Wnd->Style & (WS_MAXIMIZE | WS_MINIMIZE)))
        {
            Wnd->NormalX      = Wnd->X;
            Wnd->NormalY      = Wnd->Y;
            Wnd->NormalWidth  = Wnd->Width;
            Wnd->NormalHeight = Wnd->Height;
        }

        Wnd->Style = (Wnd->Style & ~WS_MAXIMIZE) | WS_MINIMIZE | WS_VISIBLE;

        if (HasTaskbar)
        {
            /* Mode Taskbar Aktif: lempar ke luar layar & jangan render */
            Wnd->X          = -32000;
            Wnd->Y          = -32000;
            Wnd->NeedsPaint = FALSE;
        }
        else
        {
            /* Mode Fallback (explorer mati): parkir kotak kecil di pojok kiri bawah */
            Wnd->Width      = 190;
            Wnd->Height     = WND_TITLEBAR_HEIGHT + WND_BORDER_WIDTH;
            Wnd->X          = 8;
            Wnd->Y          = (LONG)ScrH - (LONG)Wnd->Height - 8;
            Wnd->NeedsPaint = TRUE;
        }

        NextTop = WndFindNextTopLevelLocked(Desktop, Wnd);

        if (Desktop != NULL)
        {
            KsAcquireSpinLock(&Desktop->Lock, &DeskIrql);

            PWND_OBJECT PrevFg    = (PWND_OBJECT)Desktop->ForegroundWindow;
            PWND_OBJECT PrevFocus = (PWND_OBJECT)Desktop->FocusWindow;
            BOOLEAN HadFocus = (PrevFocus != NULL &&
                                PrevFocus->Magic == WND_OBJECT_MAGIC &&
                                PrevFocus->Used &&
                                (PrevFocus == Wnd || WndIsDescendantLocked(Wnd, PrevFocus)));

            WasFg = (PrevFg == Wnd);
            if (WasFg)
            {
                Desktop->ForegroundWindow = NextTop;
            }

            if (HadFocus || WasFg)
            {
                if (HadFocus)
                {
                    OldFocusHwnd = PrevFocus->hWnd;
                }
                Desktop->FocusWindow = NextTop;
                NewFocusHwnd = NextTop ? NextTop->hWnd : NULL;

                if (GpActiveDesktop == NULL || Desktop == GpActiveDesktop)
                {
                    IptFocusWindow = NewFocusHwnd;
                }
            }

            KsReleaseSpinLock(&Desktop->Lock, DeskIrql);
        }

        if (NextTop != NULL)
        {
            NextTop->NeedsPaint = TRUE;
            if (WndIsTaskbarEligible(Desktop, NextTop))
            {
                NextActiveTaskHwnd = NextTop->hWnd;
            }
        }
        KsReleaseSpinLock(&gWindowListLock, ListIrql);

        if (OldFocusHwnd != NULL && OldFocusHwnd != NewFocusHwnd)
        {
            MsgPostToWindow(OldFocusHwnd, WM_KILLFOCUS, (WPARAM)NewFocusHwnd, 0, FALSE);
        }
        if (NewFocusHwnd != NULL && NewFocusHwnd != OldFocusHwnd)
        {
            MsgPostToWindow(NewFocusHwnd, WM_SETFOCUS, (WPARAM)OldFocusHwnd, 0, FALSE);
        }

        if (!WasVisible && WndIsTaskbarEligible(Desktop, Wnd))
        {
            WndNotifyShellHook(Desktop, HSHELL_WINDOWCREATED, Wnd->hWnd);
        }
        if (WasFg)
        {
            WndNotifyShellHook(Desktop, HSHELL_WINDOWACTIVATED, NextActiveTaskHwnd);
        }

        if (GpActiveDesktop == NULL || Desktop == GpActiveDesktop)
        {
            EngHidePointer();
            EngFillDesktopRect(OldX, OldY, OldW, OldH,
                               WndGetDesktopBgColor(GpActiveDesktop));
            EngShowPointer();
            if (NextTop != NULL)
            {
                WndInvalidateChildWindows(NextTop->hWnd);
            }
            UserInvalidateAllWindows();
        }
    }
    else if (!IsChild && (nCmdShow == SW_RESTORE || nCmdShow == SW_SHOWNORMAL))
    {
        LONG  OldX = Wnd->X;
        LONG  OldY = Wnd->Y;
        ULONG OldW = Wnd->Width;
        ULONG OldH = Wnd->Height;
        BOOLEAN WasMaxOrMin = ((Wnd->Style & (WS_MAXIMIZE | WS_MINIMIZE)) != 0);

        if (GpActiveDesktop == NULL || Wnd->Desktop == GpActiveDesktop)
        {
            WndRestoreWindowBottomShadow(Wnd, TRUE);
        }

        KsAcquireSpinLock(&gWindowListLock, &ListIrql);
        if (WasMaxOrMin && Wnd->NormalWidth > 0 && Wnd->NormalHeight > 0)
        {
            Wnd->X      = Wnd->NormalX;
            Wnd->Y      = Wnd->NormalY;
            Wnd->Width  = Wnd->NormalWidth;
            Wnd->Height = Wnd->NormalHeight;
        }
        Wnd->Style = (Wnd->Style & ~(WS_MAXIMIZE | WS_MINIMIZE)) | WS_VISIBLE;
        Wnd->NeedsPaint = TRUE;
        KsReleaseSpinLock(&gWindowListLock, ListIrql);

        if (!WasVisible && WndIsTaskbarEligible(Wnd->Desktop, Wnd))
        {
            WndNotifyShellHook(Wnd->Desktop, HSHELL_WINDOWCREATED, Wnd->hWnd);
        }

        /* Angkat ke depan & jadikan foreground window aktif di taskbar */
        VeaUserSetForegroundWindow(Wnd->hWnd);

        if (WasMaxOrMin && (GpActiveDesktop == NULL || Wnd->Desktop == GpActiveDesktop))
        {
            EngHidePointer();
            EngFillDesktopRect(OldX, OldY, OldW, OldH,
                               WndGetDesktopBgColor(GpActiveDesktop));
            EngShowPointer();
            WndInvalidateChildWindows(Wnd->hWnd);
            UserInvalidateAllWindows();
        }
        else
        {
            WndInvalidateChildWindows(Wnd->hWnd);
            MsgNotifyPaintAll();
        }
    }
    else
    {
        KsAcquireSpinLock(&gWindowListLock, &ListIrql);
        Wnd->Style |= WS_VISIBLE;
        BOOLEAN EffVisible = WndIsEffectivelyVisibleLocked(Wnd);
        if (EffVisible)
        {
            Wnd->NeedsPaint = TRUE;
        }
        KsReleaseSpinLock(&gWindowListLock, ListIrql);

        if (!IsChild)
        {
            if (!WasVisible && WndIsTaskbarEligible(Wnd->Desktop, Wnd))
            {
                WndNotifyShellHook(Wnd->Desktop, HSHELL_WINDOWCREATED, Wnd->hWnd);
            }

            VeaUserSetForegroundWindow(Wnd->hWnd);
        }

        if (EffVisible)
        {
            WndInvalidateChildWindows(Wnd->hWnd);
            MsgNotifyPaintAll();
        }
    }

    WndDereference(Wnd);
    return WasVisible;
}

static BOOLEAN
WndDestroyWindowInternal(
    IN HWND    hWnd,
    IN BOOLEAN ForceSystem,
    IN ULONG   Depth
)
/*++

Routine Description:

    This routine recursively destroys all child windows of the specified
    window (up to a recursion depth of 32), cleans up associated timers,
    dispatches WM_DESTROY and WM_NCDESTROY to the user-mode window
    procedure, unlinks the window from the global and desktop lists,
    transfers foreground and input focus to the next eligible window,
    notifies shell hooks, releases references, and erases and invalidates
    affected screen regions.

Arguments:

    hWnd - Supplies the handle to the window to destroy.

    ForceSystem - Supplies TRUE to bypass process ownership checks for
                  internal or recursive destruction; FALSE for direct
                  user-mode requests.

    Depth - Supplies the current recursion depth for child window
            destruction.

Return Value:

    BOOLEAN - Returns TRUE if the window was destroyed (or already gone);
              otherwise returns FALSE if the handle is invalid, ownership
              validation fails, or the recursion limit is exceeded.

--*/
{
    PWND_OBJECT Wnd;
    KIRQL OldIrql;

    if (Depth > 32)
    {
        KdPrintf("[VEAPI/WND] DestroyWindow batas kedalaman rekursi tercapai (hWnd=%p)\n", hWnd);
        return FALSE;
    }

    Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return FALSE;
    }

    /* Cek kepemilikan proses hanya untuk panggilan langsung dari UserMode */
    if (!ForceSystem && Wnd->OwnerProcess != (PEPROCESS)PtGetCurrentProcess())
    {
        WndDereference(Wnd);
        return FALSE;
    }

    /* Lepas referensi lookup sementara sebelum masuk callback Ring 3 */
    WndDereference(Wnd);

    /* 1. Hancurkan semua child window terlebih dahulu */
    while (TRUE)
    {
        HWND ChildHwnd = NULL;
        PLIST_ENTRY Entry;

        KsAcquireSpinLock(&gWindowListLock, &OldIrql);
        for (Entry = gWindowListHead.Flink; Entry != &gWindowListHead; Entry = Entry->Flink)
        {
            PWND_OBJECT Candidate = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
            if (Candidate->Magic == WND_OBJECT_MAGIC &&
                Candidate->Used &&
                Candidate->hWndParent == hWnd)
            {
                ChildHwnd = Candidate->hWnd;
                break;
            }
        }
        KsReleaseSpinLock(&gWindowListLock, OldIrql);

        if (!ChildHwnd)
        {
            break;
        }

        /* Jika gagal (misal depth limit), break agar tidak infinite loop di Ring 0 */
        if (!WndDestroyWindowInternal(ChildHwnd, TRUE, Depth + 1))
        {
            break;
        }
    }

    //
    // Send WM_TIMER End
    //

    TimerCleanupWindowTimers(hWnd);

    /* 2. Kirim WM_DESTROY lalu WM_NCDESTROY ke WndProc di Ring 3 */
    Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return TRUE;
    }
    WndInvokeUserProc(Wnd, WM_DESTROY, 0, 0);
    WndDereference(Wnd);

    Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return TRUE;
    }
    WndInvokeUserProc(Wnd, WM_NCDESTROY, 0, 0);
    WndDereference(Wnd);

    /* Re-lookup terakhir untuk proses penghapusan dari list */
    Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return TRUE;
    }

    LONG OldX          = Wnd->X;
    LONG OldY          = Wnd->Y;
    ULONG OldW         = Wnd->Width;
    ULONG OldH         = Wnd->Height;
    BOOLEAN IsChild    = ((Wnd->Style & WS_CHILD) != 0);
    BOOLEAN WasVisible = ((Wnd->Style & WS_VISIBLE) != 0);
    PDESKTOP_OBJECT Desktop = Wnd->Desktop;

    /* Pulihkan shadow SEBELUM Wnd dicabut dari gWindowListHead dan Magic di-nolkan */
    if (!IsChild && (GpActiveDesktop == NULL || Desktop == GpActiveDesktop))
    {
        WndRestoreWindowBottomShadow(Wnd, TRUE);
    }

    /* 3. Cari pengganti fokus, lalu cabut Wnd dari global window list & invalidate magic */
    PWND_OBJECT NextTop     = NULL;
    PWND_OBJECT FallbackAnc = NULL;
    HWND NewFocusHwnd       = NULL;
    BOOLEAN WasFg              = FALSE;
    BOOLEAN WasTaskbarEligible = WndIsTaskbarEligible(Desktop, Wnd);
    HWND NextActiveTaskHwnd    = NULL;

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);

    if (IsChild)
    {
        FallbackAnc = WndFindVisibleAncestorLocked(Wnd);
    }
    else
    {
        NextTop = WndFindNextTopLevelLocked(Desktop, Wnd);
    }

    RemoveEntryList(&Wnd->GlobalListEntry);
    Wnd->Magic = 0;
    Wnd->Used  = FALSE;

    /* 4. Cabut dari Desktop & pindahkan fokus/foreground ke window berikutnya */
    if (Desktop)
    {
        KIRQL DeskIrql;
        KsAcquireSpinLock(&Desktop->Lock, &DeskIrql);

        if (Desktop->TaskmanWindow == hWnd)
        {
            Desktop->TaskmanWindow = NULL;
        }
        if (Desktop->ShellHookWindow == hWnd)
        {
            Desktop->ShellHookWindow = NULL;
        }

        if (Wnd->DesktopListEntry.Flink && Wnd->DesktopListEntry.Blink)
        {
            RemoveEntryList(&Wnd->DesktopListEntry);
            Wnd->DesktopListEntry.Flink = NULL;
            Wnd->DesktopListEntry.Blink = NULL;
        }

        WasFg            = (Desktop->ForegroundWindow == Wnd);
        BOOLEAN HadFocus = (Desktop->FocusWindow == Wnd);

        if (WasFg)
        {
            Desktop->ForegroundWindow = NextTop;
        }

        if (!IsChild && (HadFocus || WasFg))
        {
            Desktop->FocusWindow = NextTop;
            NewFocusHwnd = NextTop ? NextTop->hWnd : NULL;
            if (GpActiveDesktop == NULL || Desktop == GpActiveDesktop)
            {
                IptFocusWindow = NewFocusHwnd;
            }
        }
        else if (IsChild && HadFocus)
        {
            Desktop->FocusWindow = FallbackAnc;
            NewFocusHwnd = FallbackAnc ? FallbackAnc->hWnd : NULL;
            if (GpActiveDesktop == NULL || Desktop == GpActiveDesktop)
            {
                IptFocusWindow = NewFocusHwnd;
            }
        }

        Wnd->Desktop = NULL;
        KsReleaseSpinLock(&Desktop->Lock, DeskIrql);
    }
    else if (IptFocusWindow == hWnd)
    {
        PWND_OBJECT Fallback = IsChild ? FallbackAnc : NextTop;
        NewFocusHwnd   = Fallback ? Fallback->hWnd : NULL;
        IptFocusWindow = NewFocusHwnd;
    }

    if (NextTop != NULL)
    {
        NextTop->NeedsPaint = TRUE;
        if (WndIsTaskbarEligible(Desktop, NextTop))
        {
            NextActiveTaskHwnd = NextTop->hWnd;
        }
    }

    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    if (WasTaskbarEligible)
    {
        WndNotifyShellHook(Desktop, HSHELL_WINDOWDESTROYED, hWnd);
    }
    if (!IsChild && WasFg)
    {
        WndNotifyShellHook(Desktop, HSHELL_WINDOWACTIVATED, NextActiveTaskHwnd);
    }

    if (NewFocusHwnd != NULL)
    {
        MsgPostToWindow(NewFocusHwnd, WM_SETFOCUS, 0, 0, FALSE);
    }

    if (gCaptureWindow == hWnd)
    {
        gCaptureWindow = NULL;
    }

    /* Lepas referensi lookup terakhir + referensi awal saat CreateWindow */
    WndDereference(Wnd);
    WndDereference(Wnd);

    /* 5. Hapus bekas area top-level window di layar & jadwalkan repaint */
    if (!IsChild && WasVisible && OldW > 0 && OldH > 0 &&
        (GpActiveDesktop == NULL || Desktop == GpActiveDesktop))
    {
        EngHidePointer();
        EngFillDesktopRect(OldX, OldY, OldW, OldH,
                           WndGetDesktopBgColor(GpActiveDesktop));
        EngShowPointer();

        if (NextTop != NULL)
        {
            WndInvalidateChildWindows(NextTop->hWnd);
        }
    }
    UserInvalidateAllWindows();

    return TRUE;
}

BOOLEAN
VEAPI
VeaUserDestroyWindow(
    IN HWND hWnd
)
/*++

Routine Description:

    This routine destroys the specified window and its child hierarchy by
    delegating to WndDestroyWindowInternal with standard user-mode ownership
    validation enabled.

Arguments:

    hWnd - Supplies the handle to the window to destroy.

Return Value:

    BOOLEAN - Returns TRUE if the window was successfully destroyed;
              otherwise returns FALSE.

--*/
{
    return WndDestroyWindowInternal(hWnd, FALSE, 0);
}

VOID
VEAPI
WndAdoptOrphanWindowsToDesktop(
    IN PDESKTOP_OBJECT Desktop
)
/*++

Routine Description:

    This routine scans the global window list for active windows that are not
    yet associated with any desktop, attaches them to the specified desktop's
    window list, and updates the desktop's foreground and focus window
    pointers for adopted top-level windows.

Arguments:

    Desktop - Supplies a pointer to the target desktop object that will adopt
              orphaned windows.

Return Value:

    None.

--*/
{
    PLIST_ENTRY Entry;
    KIRQL OldIrql;

    if (!Desktop)
    {
        return;
    }

    WndEnsureListsInitialized();

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    for (Entry = gWindowListHead.Flink; Entry != &gWindowListHead; Entry = Entry->Flink)
    {
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
        if (Wnd->Magic == WND_OBJECT_MAGIC && Wnd->Used && Wnd->Desktop == NULL)
        {
            KIRQL DeskIrql;
            KsAcquireSpinLock(&Desktop->Lock, &DeskIrql);

            Wnd->Desktop = Desktop;
            InsertTailList(&Desktop->WindowListHead, &Wnd->DesktopListEntry);

            if (!(Wnd->Style & WS_CHILD))
            {
                Desktop->ForegroundWindow = Wnd;
                Desktop->FocusWindow      = Wnd;
            }

            KsReleaseSpinLock(&Desktop->Lock, DeskIrql);
        }
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);
}

PWND_OBJECT
VEAPI
WndHitTestPoint(
    IN  LONG  ScreenX,
    IN  LONG  ScreenY,
    OUT PLONG OutClientX,
    OUT PLONG OutClientY
)
/*++

Routine Description:

    This routine performs a reverse Z-order hit test across active,
    effectively visible windows on the active desktop to find the topmost
    window containing the specified screen coordinates. Child window bounds
    are clipped against their ancestor chain (up to 64 levels).

    N.B. The caller is responsible for calling WndDereference on the returned
         window object when finished.

Arguments:

    ScreenX - Supplies the horizontal screen coordinate to test.

    ScreenY - Supplies the vertical screen coordinate to test.

    OutClientX - Receives the horizontal coordinate relative to the hit
                 window's client origin if a match is found.

    OutClientY - Receives the vertical coordinate relative to the hit
                 window's client origin if a match is found.

Return Value:

    PWND_OBJECT - Returns a referenced pointer to the topmost window at the
                  specified point, or NULL if no window was hit.

--*/
{
    PLIST_ENTRY Entry;
    KIRQL OldIrql;
    PWND_OBJECT HitWnd = NULL;
    LONG HitOriginX = 0;
    LONG HitOriginY = 0;

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    for (Entry = gWindowListHead.Blink; Entry != &gWindowListHead; Entry = Entry->Blink)
    {
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
        if (Wnd->Magic != WND_OBJECT_MAGIC ||
            !Wnd->Used ||
            !WndIsEffectivelyVisibleLocked(Wnd) ||
            (GpActiveDesktop != NULL && Wnd->Desktop != GpActiveDesktop))
        {
            continue;
        }

        LONG Left, Top, Right, Bottom;
        LONG OrigX, OrigY;

        if (Wnd->Style & WS_CHILD)
        {
            WndGetClientOriginOnScreen(Wnd, &OrigX, &OrigY);
            Left   = OrigX;
            Top    = OrigY;
            Right  = OrigX + (LONG)Wnd->Width;
            Bottom = OrigY + (LONG)Wnd->Height;

            /* Potong area klik child agar tidak melewati batas parent */
            PWND_OBJECT Parent = WndLookupHandleLocked(Wnd->hWndParent);
            ULONG Depth = 0;
            while (Parent && Depth < 64)
            {
                LONG Px = 0, Py = 0;
                WndGetClientOriginOnScreen(Parent, &Px, &Py);
                LONG PR = Px + (LONG)Parent->Width;
                LONG PB = Py + (LONG)Parent->Height;
                if (!(Parent->Style & WS_CHILD) &&
                    !((Parent->Style & WS_POPUP) && ((Parent->Style & WS_CAPTION) != WS_CAPTION)))
                {
                    PR = Parent->X + (LONG)Parent->Width - WND_BORDER_WIDTH;
                    PB = Parent->Y + (LONG)Parent->Height - WND_BORDER_WIDTH;
                }
                if (Left < Px)   Left = Px;
                if (Top < Py)    Top = Py;
                if (Right > PR)  Right = PR;
                if (Bottom > PB) Bottom = PB;
                if (!(Parent->Style & WS_CHILD)) break;
                Parent = WndLookupHandleLocked(Parent->hWndParent);
                Depth++;
            }
        }
        else
        {
            BOOLEAN Framed = !((Wnd->Style & WS_POPUP) && ((Wnd->Style & WS_CAPTION) != WS_CAPTION));
            Left   = Wnd->X;
            Top    = Wnd->Y;
            Right  = Wnd->X + (LONG)Wnd->Width;
            Bottom = Wnd->Y + (LONG)Wnd->Height;
            OrigX  = Framed ? (Wnd->X + WND_BORDER_WIDTH)    : Wnd->X;
            OrigY  = Framed ? (Wnd->Y + WND_TITLEBAR_HEIGHT) : Wnd->Y;
        }

        if (ScreenX >= Left && ScreenX < Right && ScreenY >= Top && ScreenY < Bottom)
        {
            WndReference(Wnd);
            HitWnd     = Wnd;
            HitOriginX = OrigX;
            HitOriginY = OrigY;
            break;
        }
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    if (HitWnd)
    {
        *OutClientX = ScreenX - HitOriginX;
        *OutClientY = ScreenY - HitOriginY;
    }

    return HitWnd;
}

PWND_OBJECT
WndGetTopLevelAncestor(
    IN PWND_OBJECT Wnd
)
/*++

Routine Description:

    This routine walks up the child-parent chain of the specified window (up
    to 64 levels) under the global window list lock to locate its top-level
    ancestor window.

    N.B. The caller is responsible for calling WndDereference on the returned
         window object when finished.

Arguments:

    Wnd - Supplies a pointer to the starting window object.

Return Value:

    PWND_OBJECT - Returns a referenced pointer to the top-level ancestor
                  window, or NULL if the window or ancestor is invalid.

--*/
{
    PWND_OBJECT Cur = Wnd;
    KIRQL OldIrql;
    ULONG Depth = 0;

    if (!Wnd)
    {
        return NULL;
    }

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    while (Cur && (Cur->Style & WS_CHILD) && Cur->hWndParent != NULL && Depth < 64)
    {
        PWND_OBJECT Parent = WndLookupHandleLocked(Cur->hWndParent);
        if (!Parent)
        {
            break;
        }
        Cur = Parent;
        Depth++;
    }

    if (Cur && Cur->Magic == WND_OBJECT_MAGIC && Cur->Used)
    {
        WndReference(Cur);
    }
    else
    {
        Cur = NULL;
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    return Cur;
}

VOID
VEAPI
UserProcessMousePacket(
    IN PMOUSE_INPUT_DATA Mou
)
/*++

Routine Description:

    This routine processes a raw mouse input packet by updating the global
    cursor coordinates, primary display cursor position, mouse button state,
    and async key state. It resolves the target window (via active mouse
    capture or hit-testing), handles Z-order activation, foreground/focus
    transitions, shell hook and WM_NCACTIVATE notifications on left-button
    clicks, and posts the corresponding mouse messages with screen
    coordinates to the target window.

Arguments:

    Mou - Supplies a pointer to the MOUSE_INPUT_DATA packet to process.

Return Value:

    None.

--*/
{
    LONG ScreenW;
    LONG ScreenH;
    BOOLEAN Moved = FALSE;

    if (!Mou)
    {
        return;
    }

    ScreenW = (LONG)GpPrimaryDisplay.PrimarySurface.Width;
    ScreenH = (LONG)GpPrimaryDisplay.PrimarySurface.Height;
    if (ScreenW <= 0) ScreenW = 800;
    if (ScreenH <= 0) ScreenH = 600;

    if (Mou->Flags & MOUSE_MOVE_ABSOLUTE)
    {
        gCursorX = (Mou->LastX * ScreenW) / 65535;
        gCursorY = (Mou->LastY * ScreenH) / 65535;
        Moved = TRUE;
    }
    else if (Mou->LastX != 0 || Mou->LastY != 0)
    {
        gCursorX += Mou->LastX;
        gCursorY += Mou->LastY;
        Moved = TRUE;
    }

    if (gCursorX < 0) gCursorX = 0;
    if (gCursorY < 0) gCursorY = 0;
    if (gCursorX >= ScreenW) gCursorX = ScreenW - 1;
    if (gCursorY >= ScreenH) gCursorY = ScreenH - 1;

    GpPrimaryDisplay.CursorX = gCursorX;
    GpPrimaryDisplay.CursorY = gCursorY;

    if (Mou->ButtonFlags & MOUSE_LEFT_BUTTON_DOWN)
    {
        gMouseButtonState |= MK_LBUTTON;
        IptAsyncKeyState[VK_LBUTTON] = 0x80;
    }
    if (Mou->ButtonFlags & MOUSE_LEFT_BUTTON_UP)
    {
        gMouseButtonState &= ~MK_LBUTTON;
        IptAsyncKeyState[VK_LBUTTON] = 0x00;
    }
    if (Mou->ButtonFlags & MOUSE_RIGHT_BUTTON_DOWN)
    {
        gMouseButtonState |= MK_RBUTTON;
        IptAsyncKeyState[VK_RBUTTON] = 0x80;
    }
    if (Mou->ButtonFlags & MOUSE_RIGHT_BUTTON_UP)
    {
        gMouseButtonState &= ~MK_RBUTTON;
        IptAsyncKeyState[VK_RBUTTON] = 0x00;
    }
    if (Mou->ButtonFlags & MOUSE_MIDDLE_BUTTON_DOWN)
    {
        gMouseButtonState |= MK_MBUTTON;
        IptAsyncKeyState[VK_MBUTTON] = 0x80;
    }
    if (Mou->ButtonFlags & MOUSE_MIDDLE_BUTTON_UP)
    {
        gMouseButtonState &= ~MK_MBUTTON;
        IptAsyncKeyState[VK_MBUTTON] = 0x00;
    }

    /* Cari target window: pakai gCaptureWindow jika aktif, selain itu HitTestPoint */
    LONG DummyX = 0, DummyY = 0;
    PWND_OBJECT Target = gCaptureWindow ? WndLookupHandle(gCaptureWindow) : NULL;
    if (!Target)
    {
        gCaptureWindow = NULL;
        Target = WndHitTestPoint(gCursorX, gCursorY, &DummyX, &DummyY);
    }

    if (!Target)
    {
        return;
    }

    /* Aktivasi Z-order, fokus, dan WM_NCACTIVATE saat klik kiri (jika tidak sedang capture) */
    if ((Mou->ButtonFlags & MOUSE_LEFT_BUTTON_DOWN) && gCaptureWindow == NULL)
    {
        PWND_OBJECT TopWnd      = WndGetTopLevelAncestor(Target);
        PWND_OBJECT ActiveTop   = TopWnd ? TopWnd : Target;
        HWND OldFocusHwnd       = NULL;
        HWND NewFocusHwnd       = NULL;
        HWND PrevFgHwnd         = NULL;
        HWND ActiveTaskHwnd     = NULL;
        BOOLEAN FgChanged       = FALSE;

        WndBringToFront(ActiveTop);

        BOOLEAN ShouldChangeFocus = ((Target->Style & WS_CHILD) != 0);

        PDESKTOP_OBJECT Desktop = GpActiveDesktop;
        if (Desktop != NULL)
        {
            KIRQL ListIrql, DeskIrql;
            KsAcquireSpinLock(&gWindowListLock, &ListIrql);
            KsAcquireSpinLock(&Desktop->Lock, &DeskIrql);

            PWND_OBJECT PrevFg    = (PWND_OBJECT)Desktop->ForegroundWindow;
            PWND_OBJECT PrevFocus = (PWND_OBJECT)Desktop->FocusWindow;

            if (!ShouldChangeFocus)
            {
                if (PrevFocus == NULL ||
                    PrevFocus->Magic != WND_OBJECT_MAGIC ||
                    !PrevFocus->Used ||
                    (PrevFocus != ActiveTop && !WndIsDescendantLocked(ActiveTop, PrevFocus)))
                {
                    ShouldChangeFocus = TRUE;
                }
            }

            if (PrevFg != ActiveTop)
            {
                FgChanged = TRUE;
                if (PrevFg != NULL && PrevFg->Magic == WND_OBJECT_MAGIC && PrevFg->Used)
                {
                    PrevFgHwnd = PrevFg->hWnd;
                    PrevFg->NeedsPaint = TRUE;
                }
                ActiveTop->NeedsPaint = TRUE;
                ActiveTaskHwnd = WndIsTaskbarEligible(Desktop, ActiveTop) ? ActiveTop->hWnd : NULL;
            }

            Desktop->ForegroundWindow = ActiveTop;

            if (ShouldChangeFocus)
            {
                if (PrevFocus && PrevFocus != Target &&
                    PrevFocus->Magic == WND_OBJECT_MAGIC && PrevFocus->Used)
                {
                    OldFocusHwnd = PrevFocus->hWnd;
                }
                Desktop->FocusWindow = Target;
                NewFocusHwnd = Target->hWnd;
            }

            KsReleaseSpinLock(&Desktop->Lock, DeskIrql);
            KsReleaseSpinLock(&gWindowListLock, ListIrql);

            if (FgChanged)
            {
                WndInvalidateChildWindows(ActiveTop->hWnd);
                MsgNotifyPaintAll();
                WndNotifyShellHook(Desktop, HSHELL_WINDOWACTIVATED, ActiveTaskHwnd);
            }
        }
        else if (ShouldChangeFocus && IptFocusWindow != Target->hWnd)
        {
            OldFocusHwnd = IptFocusWindow;
            NewFocusHwnd = Target->hWnd;
        }

        if (FgChanged)
        {
            if (PrevFgHwnd != NULL && PrevFgHwnd != ActiveTop->hWnd)
            {
                MsgPostToWindow(PrevFgHwnd, WM_NCACTIVATE, FALSE, 0, FALSE);
            }
            MsgPostToWindow(ActiveTop->hWnd, WM_NCACTIVATE, TRUE, 0, FALSE);
        }

        if (NewFocusHwnd != NULL)
        {
            IptFocusWindow = NewFocusHwnd;
            if (OldFocusHwnd != NULL && OldFocusHwnd != NewFocusHwnd)
            {
                MsgPostToWindow(OldFocusHwnd, WM_KILLFOCUS, (WPARAM)NewFocusHwnd, 0, FALSE);
                MsgPostToWindow(NewFocusHwnd, WM_SETFOCUS, (WPARAM)OldFocusHwnd, 0, FALSE);
            }
        }

        if (TopWnd)
        {
            WndDereference(TopWnd);
        }
    }

    /* Kirim koordinat LAYAR di lParam; msg.c yang akan menerjemahkan via WM_NCHITTEST */
    LONG_PTR ScreenLParam = ((LONG_PTR)(SHORT)gCursorY << 16) | ((ULONG_PTR)(USHORT)gCursorX);

    if (Moved)
    {
        MsgPostMouseMessage(Target->hWnd, WM_MOUSEMOVE, gMouseButtonState, ScreenLParam);
    }
    if (Mou->ButtonFlags & MOUSE_LEFT_BUTTON_DOWN)
    {
        MsgPostMouseMessage(Target->hWnd, WM_LBUTTONDOWN, gMouseButtonState, ScreenLParam);
    }
    if (Mou->ButtonFlags & MOUSE_LEFT_BUTTON_UP)
    {
        MsgPostMouseMessage(Target->hWnd, WM_LBUTTONUP, gMouseButtonState, ScreenLParam);
    }
    if (Mou->ButtonFlags & MOUSE_RIGHT_BUTTON_DOWN)
    {
        MsgPostMouseMessage(Target->hWnd, WM_RBUTTONDOWN, gMouseButtonState, ScreenLParam);
    }
    if (Mou->ButtonFlags & MOUSE_RIGHT_BUTTON_UP)
    {
        MsgPostMouseMessage(Target->hWnd, WM_RBUTTONUP, gMouseButtonState, ScreenLParam);
    }
    if (Mou->ButtonFlags & MOUSE_MIDDLE_BUTTON_DOWN)
    {
        MsgPostMouseMessage(Target->hWnd, WM_MBUTTONDOWN, gMouseButtonState, ScreenLParam);
    }
    if (Mou->ButtonFlags & MOUSE_MIDDLE_BUTTON_UP)
    {
        MsgPostMouseMessage(Target->hWnd, WM_MBUTTONUP, gMouseButtonState, ScreenLParam);
    }
    if (Mou->ButtonFlags & MOUSE_WHEEL)
    {
        ULONG_PTR WheelWParam = ((ULONG_PTR)(USHORT)Mou->ButtonData << 16) | gMouseButtonState;
        MsgPostMouseMessage(Target->hWnd, WM_MOUSEWHEEL, WheelWParam, ScreenLParam);
    }

    WndDereference(Target);
}

VEASTATUS
VEAPI
VeaUserBuildHwndList(
    IN  HDESK   hDesktop,
    IN  HWND    hwndParent,
    IN  BOOLEAN bChildren,
    IN  ULONG   dwThreadId,
    IN  ULONG   cHwndMax,
    OUT HWND   *phwndFirst,
    OUT PULONG  pcHwndNeeded
)
/*++

Routine Description:

    This routine enumerates active windows in top-to-bottom Z-order and fills
    the caller's buffer with matching window handles. Depending on the
    parameters, it enumerates either top-level windows belonging to a target
    desktop or child/descendant windows under a specified parent, optionally
    filtered by owner thread ID. User-mode output pointers are validated and
    copied inside a structured exception block.

Arguments:

    hDesktop - Optionally supplies a handle to the desktop whose top-level
               windows are enumerated when bChildren is FALSE. If NULL, the
               current thread's desktop or the active desktop is used.

    hwndParent - Optionally supplies the parent window handle when bChildren
                 is TRUE; all matching descendant windows are included.

    bChildren - Supplies TRUE to enumerate child windows, or FALSE to
                enumerate top-level windows.

    dwThreadId - Optionally supplies a thread ID to filter windows by owner
                 thread, or 0 to match all threads.

    cHwndMax - Supplies the maximum number of HWND entries that phwndFirst
               can hold (capped at MAX_ENUM_WINDOWS).

    phwndFirst - Receives the enumerated window handles. Must be non-NULL if
                 cHwndMax is greater than 0.

    pcHwndNeeded - Receives the total number of matching window handles
                   found, even if cHwndMax is too small.

Return Value:

    VEASTATUS - Returns STATUS_SUCCESS if all matching handles fit in the
                buffer, STATUS_BUFFER_TOO_SMALL if more handles were found
                than cHwndMax, STATUS_INVALID_PARAMETER for invalid
                arguments, STATUS_INVALID_HANDLE if hDesktop is invalid,
                STATUS_INSUFFICIENT_RESOURCES on pool allocation failure, or
                STATUS_ACCESS_VIOLATION if user-mode buffer access fails.

--*/
{
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    PDESKTOP_OBJECT TargetDesktop = NULL;
    BOOLEAN DerefDesktop = FALSE;
    KIRQL OldIrql;
    PLIST_ENTRY Entry;

    if (!pcHwndNeeded || (cHwndMax > 0 && !phwndFirst) || cHwndMax > MAX_ENUM_WINDOWS)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (IsUser)
    {
        if ((ULONG_PTR)pcHwndNeeded >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)pcHwndNeeded + sizeof(ULONG)) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        if (cHwndMax > 0)
        {
            SIZE_T Bytes = (SIZE_T)cHwndMax * sizeof(HWND);
            if ((ULONG_PTR)phwndFirst >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
                ((ULONG_PTR)phwndFirst + Bytes) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
            {
                return STATUS_ACCESS_VIOLATION;
            }
        }
    }

    /* Tentukan desktop target jika bukan enumerasi child window */
    if (!bChildren)
    {
        if (hDesktop != NULL)
        {
            VEASTATUS Status = ObReferenceObjectByHandle(
                (HANDLE)hDesktop,
                0,
                UlDesktopObjectType,
                KeGetPreviousMode(),
                (PVOID *)&TargetDesktop
            );
            if (!VEA_SUCCESS(Status) || !TargetDesktop)
            {
                return STATUS_INVALID_HANDLE;
            }
            DerefDesktop = TRUE;
        }
        else
        {
            PTHREADINFO pti = (PTHREADINFO)PtGetCurrentThreadVeapiThread();
            TargetDesktop = (pti && pti->rpdesk) ? pti->rpdesk : GpActiveDesktop;
        }
    }

    HWND *KernelBuf = NULL;
    if (cHwndMax > 0)
    {
        KernelBuf = (HWND *)WND_POOL_ALLOC((SIZE_T)cHwndMax * sizeof(HWND));
        if (!KernelBuf)
        {
            if (DerefDesktop) ObDereferenceObject(TargetDesktop);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    ULONG Needed = 0;

    /* Iterasi dari Z-order paling atas (Blink) ke bawah (Flink), standar Win32 */
    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    for (Entry = gWindowListHead.Blink; Entry != &gWindowListHead; Entry = Entry->Blink)
    {
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
        if (Wnd->Magic != WND_OBJECT_MAGIC || !Wnd->Used)
        {
            continue;
        }

        if (dwThreadId != 0 && (ULONG)(ULONG_PTR)Wnd->OwnerThread != dwThreadId)
        {
            continue;
        }

        if (bChildren)
        {
            if (!(Wnd->Style & WS_CHILD))
            {
                continue;
            }

            if (hwndParent != NULL)
            {
                /* Cocokkan semua descendant milik hwndParent */
                if (Wnd->hWndParent != hwndParent)
                {
                    PWND_OBJECT ParentObj = WndLookupHandleLocked(hwndParent);
                    if (!ParentObj || !WndIsDescendantLocked(ParentObj, Wnd))
                    {
                        continue;
                    }
                }
            }
        }
        else
        {
            /* Top-level window saja */
            if (Wnd->Style & WS_CHILD)
            {
                continue;
            }
            if (TargetDesktop != NULL && Wnd->Desktop != TargetDesktop)
            {
                continue;
            }
        }

        if (Needed < cHwndMax && KernelBuf != NULL)
        {
            KernelBuf[Needed] = Wnd->hWnd;
        }
        Needed++;
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    if (DerefDesktop)
    {
        ObDereferenceObject(TargetDesktop);
    }

    ULONG Copied = (Needed < cHwndMax) ? Needed : cHwndMax;

    if (IsUser)
    {
        _VEA_TRY
        {
            *pcHwndNeeded = Needed;
            if (Copied > 0 && KernelBuf != NULL)
            {
                RtlCopyMemory(phwndFirst, KernelBuf, (SIZE_T)Copied * sizeof(HWND));
            }
        }
        _VEA_EXCEPT(1)
        {
            if (KernelBuf) WND_POOL_FREE(KernelBuf);
            return STATUS_ACCESS_VIOLATION;
        }
        _VEA_END
    }
    else
    {
        *pcHwndNeeded = Needed;
        if (Copied > 0 && KernelBuf != NULL)
        {
            RtlCopyMemory(phwndFirst, KernelBuf, (SIZE_T)Copied * sizeof(HWND));
        }
    }

    if (KernelBuf)
    {
        WND_POOL_FREE(KernelBuf);
    }

    return (Needed > cHwndMax) ? STATUS_BUFFER_TOO_SMALL : STATUS_SUCCESS;
}

HWND
VEAPI
VeaUserGetWindow(
    IN HWND hWnd,
    IN UINT uCmd
)
/*++

Routine Description:

    This routine retrieves a handle to a window that has the specified
    Z-order or ownership relationship (GW_OWNER, GW_CHILD, GW_HWNDFIRST,
    GW_HWNDLAST, GW_HWNDNEXT, or GW_HWNDPREV) to the given window. Sibling
    and top-level searches are scoped to the same desktop and parent level
    under the global window list lock.

Arguments:

    hWnd - Supplies the handle to the reference window.

    uCmd - Supplies the relationship command (GW_*) specifying which related
           window to retrieve.

Return Value:

    HWND - Returns the handle to the matching related window, or NULL if
           hWnd is invalid, uCmd is unsupported, or no matching window
           exists.

--*/
{
    KIRQL OldIrql;
    PLIST_ENTRY Entry;
    HWND ResultHwnd = NULL;

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    PWND_OBJECT Wnd = WndLookupHandleLocked(hWnd);
    if (!Wnd)
    {
        KsReleaseSpinLock(&gWindowListLock, OldIrql);
        return NULL;
    }

    BOOLEAN IsChild = ((Wnd->Style & WS_CHILD) != 0);
    HWND ParentHwnd = IsChild ? Wnd->hWndParent : NULL;
    PDESKTOP_OBJECT Desk = Wnd->Desktop;

    switch (uCmd)
    {
        case GW_OWNER:
            ResultHwnd = !IsChild ? Wnd->hWndParent : NULL;
            break;

        case GW_CHILD:
            /* Z-order paling atas = Blink */
            for (Entry = gWindowListHead.Blink; Entry != &gWindowListHead; Entry = Entry->Blink)
            {
                PWND_OBJECT Cand = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
                if (Cand->Magic == WND_OBJECT_MAGIC && Cand->Used &&
                    (Cand->Style & WS_CHILD) && Cand->hWndParent == hWnd)
                {
                    ResultHwnd = Cand->hWnd;
                    break;
                }
            }
            break;

        case GW_HWNDFIRST:
            for (Entry = gWindowListHead.Blink; Entry != &gWindowListHead; Entry = Entry->Blink)
            {
                PWND_OBJECT Cand = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
                if (Cand->Magic != WND_OBJECT_MAGIC || !Cand->Used || Cand->Desktop != Desk)
                    continue;
                if (IsChild ? ((Cand->Style & WS_CHILD) && Cand->hWndParent == ParentHwnd)
                            : !(Cand->Style & WS_CHILD))
                {
                    ResultHwnd = Cand->hWnd;
                    break;
                }
            }
            break;

        case GW_HWNDLAST:
            for (Entry = gWindowListHead.Flink; Entry != &gWindowListHead; Entry = Entry->Flink)
            {
                PWND_OBJECT Cand = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
                if (Cand->Magic != WND_OBJECT_MAGIC || !Cand->Used || Cand->Desktop != Desk)
                    continue;
                if (IsChild ? ((Cand->Style & WS_CHILD) && Cand->hWndParent == ParentHwnd)
                            : !(Cand->Style & WS_CHILD))
                {
                    ResultHwnd = Cand->hWnd;
                    break;
                }
            }
            break;

        case GW_HWNDNEXT: /* Z-order di bawahnya (arah Blink) */
            for (Entry = Wnd->GlobalListEntry.Blink; Entry != &gWindowListHead; Entry = Entry->Blink)
            {
                PWND_OBJECT Cand = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
                if (Cand->Magic != WND_OBJECT_MAGIC || !Cand->Used || Cand->Desktop != Desk)
                    continue;
                if (IsChild ? ((Cand->Style & WS_CHILD) && Cand->hWndParent == ParentHwnd)
                            : !(Cand->Style & WS_CHILD))
                {
                    ResultHwnd = Cand->hWnd;
                    break;
                }
            }
            break;

        case GW_HWNDPREV: /* Z-order di atasnya (arah Flink) */
            for (Entry = Wnd->GlobalListEntry.Flink; Entry != &gWindowListHead; Entry = Entry->Flink)
            {
                PWND_OBJECT Cand = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
                if (Cand->Magic != WND_OBJECT_MAGIC || !Cand->Used || Cand->Desktop != Desk)
                    continue;
                if (IsChild ? ((Cand->Style & WS_CHILD) && Cand->hWndParent == ParentHwnd)
                            : !(Cand->Style & WS_CHILD))
                {
                    ResultHwnd = Cand->hWnd;
                    break;
                }
            }
            break;

        default:
            break;
    }

    KsReleaseSpinLock(&gWindowListLock, OldIrql);
    return ResultHwnd;
}

HWND
VEAPI
VeaUserFindWindowEx(
    IN HWND        hwndParent,
    IN HWND        hwndChildAfter,
    IN CONST CHAR *lpszClass,
    IN CONST CHAR *lpszWindow
)
/*++

Routine Description:

    This routine searches windows in top-to-bottom Z-order for the first
    window matching the optional class name and window title strings. If
    hwndParent is specified, only direct child windows of that parent are
    searched; otherwise, top-level windows on the current thread's or active
    desktop are searched. Searching resumes after hwndChildAfter when
    provided.

Arguments:

    hwndParent - Optionally supplies the parent window whose direct children
                 are searched, or NULL to search top-level windows.

    hwndChildAfter - Optionally supplies a child/sibling window handle after
                     which the search begins, or NULL to start from the top
                     of the Z-order.

    lpszClass - Optionally supplies a null-terminated class name string to
                match (up to 63 characters), or NULL/empty to match any
                class.

    lpszWindow - Optionally supplies a null-terminated window title string to
                 match (up to 63 characters), or NULL to match any title.

Return Value:

    HWND - Returns the handle to the first matching window, or NULL if no
           match is found or string copy from user mode fails.

--*/
{
    CHAR LocalClass[64];
    CHAR LocalTitle[64];
    BOOLEAN HasClass = FALSE;
    BOOLEAN HasTitle = FALSE;
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    KIRQL OldIrql;
    PLIST_ENTRY Entry;
    HWND FoundHwnd = NULL;

    if (lpszClass != NULL)
    {
        if (!WndCopyStringSafe(LocalClass, (PCHAR)lpszClass, sizeof(LocalClass), IsUser))
            return NULL;
        HasClass = (LocalClass[0] != '\0');
    }

    if (lpszWindow != NULL)
    {
        if (!WndCopyStringSafe(LocalTitle, (PCHAR)lpszWindow, sizeof(LocalTitle), IsUser))
            return NULL;
        HasTitle = TRUE;
    }

    PTHREADINFO pti = (PTHREADINFO)PtGetCurrentThreadVeapiThread();
    PDESKTOP_OBJECT Desk = (pti && pti->rpdesk) ? pti->rpdesk : GpActiveDesktop;
    BOOLEAN PastAfter = (hwndChildAfter == NULL);

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    for (Entry = gWindowListHead.Blink; Entry != &gWindowListHead; Entry = Entry->Blink)
    {
        PWND_OBJECT Wnd = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
        if (Wnd->Magic != WND_OBJECT_MAGIC || !Wnd->Used)
            continue;

        if (hwndParent != NULL)
        {
            if (!(Wnd->Style & WS_CHILD) || Wnd->hWndParent != hwndParent)
                continue;
        }
        else
        {
            if ((Wnd->Style & WS_CHILD) || (Desk != NULL && Wnd->Desktop != Desk))
                continue;
        }

        if (!PastAfter)
        {
            if (Wnd->hWnd == hwndChildAfter)
                PastAfter = TRUE;
            continue;
        }

        if (HasClass && !WndStrEqual(Wnd->ClassName, LocalClass))
            continue;

        if (HasTitle && !WndStrEqual(Wnd->Title, LocalTitle))
            continue;

        FoundHwnd = Wnd->hWnd;
        break;
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    return FoundHwnd;
}

HWND
VEAPI
VeaUserGetForegroundWindow(VOID)
/*++

Routine Description:

    This routine retrieves the handle to the current foreground window on
    the active desktop.

Arguments:

    None.

Return Value:

    HWND - Returns the handle to the active desktop's foreground window, or
           NULL if there is no active desktop or no valid foreground window.

--*/
{
    PDESKTOP_OBJECT Desk = GpActiveDesktop;
    HWND FgHwnd = NULL;
    KIRQL OldIrql;

    if (!Desk)
    {
        return NULL;
    }

    KsAcquireSpinLock(&Desk->Lock, &OldIrql);
    PWND_OBJECT Fg = (PWND_OBJECT)Desk->ForegroundWindow;
    if (Fg && Fg->Magic == WND_OBJECT_MAGIC && Fg->Used)
    {
        FgHwnd = Fg->hWnd;
    }
    KsReleaseSpinLock(&Desk->Lock, OldIrql);

    return FgHwnd;
}

BOOLEAN
VEAPI
VeaUserSetForegroundWindow(
    IN HWND hWnd
)
/*++

Routine Description:

    This routine resolves the top-level ancestor of the specified window,
    verifies that it resides on the active desktop, brings it to the front
    of the Z-order, and sets it as both the desktop's foreground and focus
    window. It marks the previous and new foreground windows for repainting,
    invalidates child windows, posts WM_KILLFOCUS/WM_SETFOCUS transitions,
    and notifies the shell hook of window activation.

Arguments:

    hWnd - Supplies the handle to the window (or child window) to bring to
           the foreground.

Return Value:

    BOOLEAN - Returns TRUE if the window was successfully set as the
              foreground window; otherwise returns FALSE if the handle is
              invalid or the window is not on the active desktop.

--*/
{
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return FALSE;
    }

    PWND_OBJECT TopWnd = WndGetTopLevelAncestor(Wnd);
    PWND_OBJECT TargetTop = TopWnd ? TopWnd : Wnd;

    if (GpActiveDesktop != NULL && TargetTop->Desktop != GpActiveDesktop)
    {
        if (TopWnd) WndDereference(TopWnd);
        WndDereference(Wnd);
        return FALSE;
    }

    WndBringToFront(TargetTop);

    PDESKTOP_OBJECT Desk = GpActiveDesktop;
    HWND OldFocusHwnd = NULL;
    HWND NewFocusHwnd = TargetTop->hWnd;

    if (Desk != NULL)
    {
        KIRQL OldIrql;
        PWND_OBJECT PrevFg    = (PWND_OBJECT)Desk->ForegroundWindow;
        PWND_OBJECT PrevFocus = (PWND_OBJECT)Desk->FocusWindow;

        if (PrevFocus && PrevFocus != TargetTop &&
            PrevFocus->Magic == WND_OBJECT_MAGIC && PrevFocus->Used)
        {
            OldFocusHwnd = PrevFocus->hWnd;
        }

        KsAcquireSpinLock(&Desk->Lock, &OldIrql);
        Desk->ForegroundWindow = TargetTop;
        Desk->FocusWindow      = TargetTop;
        KsReleaseSpinLock(&Desk->Lock, OldIrql);

        if (PrevFg && PrevFg != TargetTop && PrevFg->Magic == WND_OBJECT_MAGIC && PrevFg->Used)
        {
            PrevFg->NeedsPaint = TRUE;
        }
    }

    IptFocusWindow = NewFocusHwnd;
    TargetTop->NeedsPaint = TRUE;
    WndInvalidateChildWindows(TargetTop->hWnd);
    MsgNotifyPaintAll();

    if (OldFocusHwnd != NULL && OldFocusHwnd != NewFocusHwnd)
    {
        MsgPostToWindow(OldFocusHwnd, WM_KILLFOCUS, (WPARAM)NewFocusHwnd, 0, FALSE);
        MsgPostToWindow(NewFocusHwnd, WM_SETFOCUS, (WPARAM)OldFocusHwnd, 0, FALSE);
    }

    if (TargetTop->Desktop != NULL)
    {
        HWND ActiveTaskHwnd = WndIsTaskbarEligible(TargetTop->Desktop, TargetTop)
                              ? TargetTop->hWnd
                              : NULL;
        WndNotifyShellHook(TargetTop->Desktop, HSHELL_WINDOWACTIVATED, ActiveTaskHwnd);
    }

    if (TopWnd) WndDereference(TopWnd);
    WndDereference(Wnd);
    return TRUE;
}

INT
VEAPI
VeaUserGetWindowText(
    IN  HWND  hWnd,
    OUT PCHAR lpString,
    IN  INT   nMaxCount
)
/*++

Routine Description:

    This routine copies the title text of the specified window into the
    caller's buffer, truncating to fit within nMaxCount bytes (including the
    null terminator) and the internal 64-byte title limit. User-mode output
    buffers are validated and written inside a structured exception block.

Arguments:

    hWnd - Supplies the handle to the window whose title text is retrieved.

    lpString - Receives the null-terminated window title string.

    nMaxCount - Supplies the maximum number of characters to copy into
                lpString, including the terminating null character.

Return Value:

    INT - Returns the length, in characters, of the copied string excluding
          the null terminator, or 0 if the window is invalid, parameters are
          invalid, or copying to user mode fails.

--*/
{
    CHAR LocalBuf[64];
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    KIRQL OldIrql;

    if (!lpString || nMaxCount <= 0)
    {
        return 0;
    }

    if (IsUser)
    {
        if ((ULONG_PTR)lpString >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)lpString + (ULONG_PTR)nMaxCount) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return 0;
        }
    }

    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return 0;
    }

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    RtlCopyMemory(LocalBuf, Wnd->Title, sizeof(LocalBuf));
    KsReleaseSpinLock(&gWindowListLock, OldIrql);
    WndDereference(Wnd);

    INT CopyLen = 0;
    while (CopyLen < (nMaxCount - 1) && CopyLen < (INT)(sizeof(LocalBuf) - 1) && LocalBuf[CopyLen] != '\0')
    {
        CopyLen++;
    }
    LocalBuf[CopyLen] = '\0';

    if (IsUser)
    {
        _VEA_TRY
        {
            RtlCopyMemory(lpString, LocalBuf, (SIZE_T)CopyLen + 1);
        }
        _VEA_EXCEPT(1)
        {
            return 0;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(lpString, LocalBuf, (SIZE_T)CopyLen + 1);
    }

    return CopyLen;
}

BOOLEAN
VEAPI
VeaUserSetWindowText(
    IN HWND        hWnd,
    IN CONST CHAR *lpString
)
/*++

Routine Description:

    This routine updates the title text of the specified window (up to 63
    characters, or empty if lpString is NULL). If the title changes and the
    window is effectively visible, it schedules a repaint for child or
    borderless windows or redraws the title bar for framed windows. For
    taskbar-eligible windows, it also notifies the shell hook with
    HSHELL_REDRAW.

Arguments:

    hWnd - Supplies the handle to the window whose title text is updated.

    lpString - Optionally supplies a null-terminated string for the new
               window title, or NULL to clear the title.

Return Value:

    BOOLEAN - Returns TRUE if the window handle is valid and the operation
              succeeded; otherwise returns FALSE if copying the string from
              user mode fails or hWnd is invalid.

--*/
{
    CHAR LocalBuf[64];
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    KIRQL OldIrql;

    RtlZeroMemory(LocalBuf, sizeof(LocalBuf));

    if (lpString != NULL)
    {
        if (!WndCopyStringSafe(LocalBuf, (PCHAR)lpString, sizeof(LocalBuf), IsUser))
        {
            return FALSE;
        }
    }

    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return FALSE;
    }

    BOOLEAN Changed = FALSE;
    BOOLEAN ShouldRedraw = FALSE;
    BOOLEAN IsChildOrBorderless = ((Wnd->Style & WS_CHILD) != 0) ||
                                  ((Wnd->Style & WS_POPUP) && ((Wnd->Style & WS_CAPTION) != WS_CAPTION));

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    if (RtlCompareMemory(Wnd->Title, LocalBuf, sizeof(LocalBuf)) != sizeof(LocalBuf))
    {
        RtlCopyMemory(Wnd->Title, LocalBuf, sizeof(LocalBuf));
        Changed = TRUE;

        if (WndIsEffectivelyVisibleLocked(Wnd))
        {
            ShouldRedraw = TRUE;
            if (IsChildOrBorderless)
            {
                Wnd->NeedsPaint = TRUE;
            }
        }
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    if (Changed && ShouldRedraw)
    {
        if (IsChildOrBorderless)
        {
            MsgNotifyPaint(Wnd->OwnerThread);
        }
        else
        {
            WndRedrawTitleBar(Wnd);
        }
    }

    if (Changed && Wnd->Desktop != NULL && WndIsTaskbarEligible(Wnd->Desktop, Wnd))
    {
        WndNotifyShellHook(Wnd->Desktop, HSHELL_REDRAW, Wnd->hWnd);
    }

    WndDereference(Wnd);
    return TRUE;
}

BOOLEAN
VEAPI
VeaUserIsWindow(
    IN HWND hWnd
)
/*++

Routine Description:

    This routine checks whether the specified handle refers to a valid,
    active window object.

Arguments:

    hWnd - Supplies the window handle to validate.

Return Value:

    BOOLEAN - Returns TRUE if hWnd identifies an existing window; otherwise
              returns FALSE.

--*/
{
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return FALSE;
    }
    WndDereference(Wnd);
    return TRUE;
}

BOOLEAN
VEAPI
VeaUserIsWindowVisible(
    IN HWND hWnd
)
/*++

Routine Description:

    This routine determines whether the specified window is effectively
    visible by verifying that the window and every ancestor in its parent
    chain (up to 64 levels) have the WS_VISIBLE style set.

Arguments:

    hWnd - Supplies the handle to the window to test.

Return Value:

    BOOLEAN - Returns TRUE if the window and all of its ancestors have
              WS_VISIBLE set; otherwise returns FALSE if hWnd is invalid or
              any window in the hierarchy is hidden.

--*/
{
    KIRQL OldIrql;
    BOOLEAN Visible = TRUE;
    ULONG Depth = 0;

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    PWND_OBJECT Cur = WndLookupHandleLocked(hWnd);
    if (!Cur)
    {
        KsReleaseSpinLock(&gWindowListLock, OldIrql);
        return FALSE;
    }

    while (Cur && Depth < 64)
    {
        if (!(Cur->Style & WS_VISIBLE))
        {
            Visible = FALSE;
            break;
        }
        if (!(Cur->Style & WS_CHILD) || Cur->hWndParent == NULL)
        {
            break;
        }
        Cur = WndLookupHandleLocked(Cur->hWndParent);
        Depth++;
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    return Visible;
}

BOOLEAN
VEAPI
VeaUserGetWindowRect(
    IN  HWND  hWnd,
    OUT PRECT lpRect
)
/*++

Routine Description:

    This routine retrieves the bounding rectangle of the specified window in
    screen coordinates. For child windows, the origin is resolved relative
    to the screen via WndGetClientOriginOnScreen. User-mode output pointers
    are validated and written inside a structured exception block.

Arguments:

    hWnd - Supplies the handle to the window.

    lpRect - Receives the window's bounding rectangle in screen coordinates.

Return Value:

    BOOLEAN - Returns TRUE on success; otherwise returns FALSE if lpRect is
              invalid, hWnd is invalid, or copying to user mode fails.

--*/
{
    RECT LocalRect;
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    KIRQL OldIrql;

    if (!lpRect)
    {
        return FALSE;
    }

    if (IsUser)
    {
        if ((ULONG_PTR)lpRect >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)lpRect + sizeof(RECT)) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return FALSE;
        }
    }

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    PWND_OBJECT Wnd = WndLookupHandleLocked(hWnd);
    if (!Wnd)
    {
        KsReleaseSpinLock(&gWindowListLock, OldIrql);
        return FALSE;
    }

    if (Wnd->Style & WS_CHILD)
    {
        LONG OrigX = 0, OrigY = 0;
        WndGetClientOriginOnScreen(Wnd, &OrigX, &OrigY);
        LocalRect.left   = OrigX;
        LocalRect.top    = OrigY;
        LocalRect.right  = OrigX + (LONG)Wnd->Width;
        LocalRect.bottom = OrigY + (LONG)Wnd->Height;
    }
    else
    {
        LocalRect.left   = Wnd->X;
        LocalRect.top    = Wnd->Y;
        LocalRect.right  = Wnd->X + (LONG)Wnd->Width;
        LocalRect.bottom = Wnd->Y + (LONG)Wnd->Height;
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    if (IsUser)
    {
        _VEA_TRY
        {
            RtlCopyMemory(lpRect, &LocalRect, sizeof(RECT));
        }
        _VEA_EXCEPT(1)
        {
            return FALSE;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(lpRect, &LocalRect, sizeof(RECT));
    }

    return TRUE;
}

BOOLEAN
VEAPI
VeaUserGetClientRect(
    IN  HWND  hWnd,
    OUT PRECT lpRect
)
/*++

Routine Description:

    This routine retrieves the dimensions of the specified window's client
    area with left and top set to 0. For framed top-level windows, the
    border widths and title bar height are subtracted and clamped to 0.
    User-mode output pointers are validated and written inside a structured
    exception block.

Arguments:

    hWnd - Supplies the handle to the window.

    lpRect - Receives the client area rectangle in client coordinates.

Return Value:

    BOOLEAN - Returns TRUE on success; otherwise returns FALSE if lpRect is
              invalid, hWnd is invalid, or copying to user mode fails.

--*/
{
    RECT LocalRect;
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    KIRQL OldIrql;

    if (!lpRect)
    {
        return FALSE;
    }

    if (IsUser)
    {
        if ((ULONG_PTR)lpRect >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)lpRect + sizeof(RECT)) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return FALSE;
        }
    }

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    PWND_OBJECT Wnd = WndLookupHandleLocked(hWnd);
    if (!Wnd)
    {
        KsReleaseSpinLock(&gWindowListLock, OldIrql);
        return FALSE;
    }

    BOOLEAN Framed = !(Wnd->Style & WS_CHILD) &&
                     !((Wnd->Style & WS_POPUP) && ((Wnd->Style & WS_CAPTION) != WS_CAPTION));

    LONG ClientW = (LONG)Wnd->Width;
    LONG ClientH = (LONG)Wnd->Height;

    if (Framed)
    {
        ClientW -= (2 * WND_BORDER_WIDTH);
        ClientH -= (WND_TITLEBAR_HEIGHT + WND_BORDER_WIDTH);
        if (ClientW < 0) ClientW = 0;
        if (ClientH < 0) ClientH = 0;
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    LocalRect.left   = 0;
    LocalRect.top    = 0;
    LocalRect.right  = ClientW;
    LocalRect.bottom = ClientH;

    if (IsUser)
    {
        _VEA_TRY
        {
            RtlCopyMemory(lpRect, &LocalRect, sizeof(RECT));
        }
        _VEA_EXCEPT(1)
        {
            return FALSE;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(lpRect, &LocalRect, sizeof(RECT));
    }

    return TRUE;
}

BOOLEAN
VEAPI
VeaUserClientToScreen(
    IN     HWND   hWnd,
    IN OUT PPOINT lpPoint
)
/*++

Routine Description:

    This routine converts the specified point from client-area coordinates
    of the given window to screen coordinates by adding the window's screen
    client origin. User-mode pointers are validated and accessed inside
    structured exception blocks.

Arguments:

    hWnd - Supplies the handle to the window whose client area is used for
           the conversion.

    lpPoint - Supplies the client coordinates and receives the converted
              screen coordinates.

Return Value:

    BOOLEAN - Returns TRUE on success; otherwise returns FALSE if lpPoint or
              hWnd is invalid, or user-mode memory access fails.

--*/
{
    POINT LocalPt;
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    KIRQL OldIrql;

    if (!lpPoint)
    {
        return FALSE;
    }

    if (IsUser)
    {
        if ((ULONG_PTR)lpPoint >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)lpPoint + sizeof(POINT)) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return FALSE;
        }

        _VEA_TRY
        {
            RtlCopyMemory(&LocalPt, lpPoint, sizeof(POINT));
        }
        _VEA_EXCEPT(1)
        {
            return FALSE;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(&LocalPt, lpPoint, sizeof(POINT));
    }

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    PWND_OBJECT Wnd = WndLookupHandleLocked(hWnd);
    if (!Wnd)
    {
        KsReleaseSpinLock(&gWindowListLock, OldIrql);
        return FALSE;
    }

    LONG OrigX = 0, OrigY = 0;
    WndGetClientOriginLocked(Wnd, &OrigX, &OrigY);
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    LocalPt.x += OrigX;
    LocalPt.y += OrigY;

    if (IsUser)
    {
        _VEA_TRY
        {
            RtlCopyMemory(lpPoint, &LocalPt, sizeof(POINT));
        }
        _VEA_EXCEPT(1)
        {
            return FALSE;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(lpPoint, &LocalPt, sizeof(POINT));
    }

    return TRUE;
}

BOOLEAN
VEAPI
VeaUserScreenToClient(
    IN     HWND   hWnd,
    IN OUT PPOINT lpPoint
)
/*++

Routine Description:

    This routine converts the specified point from screen coordinates to
    client-area coordinates of the given window by subtracting the window's
    screen client origin. User-mode pointers are validated and accessed
    inside structured exception blocks.

Arguments:

    hWnd - Supplies the handle to the window whose client area is used for
           the conversion.

    lpPoint - Supplies the screen coordinates and receives the converted
              client coordinates.

Return Value:

    BOOLEAN - Returns TRUE on success; otherwise returns FALSE if lpPoint or
              hWnd is invalid, or user-mode memory access fails.

--*/
{
    POINT LocalPt;
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    KIRQL OldIrql;

    if (!lpPoint)
    {
        return FALSE;
    }

    if (IsUser)
    {
        if ((ULONG_PTR)lpPoint >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)lpPoint + sizeof(POINT)) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return FALSE;
        }

        _VEA_TRY
        {
            RtlCopyMemory(&LocalPt, lpPoint, sizeof(POINT));
        }
        _VEA_EXCEPT(1)
        {
            return FALSE;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(&LocalPt, lpPoint, sizeof(POINT));
    }

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    PWND_OBJECT Wnd = WndLookupHandleLocked(hWnd);
    if (!Wnd)
    {
        KsReleaseSpinLock(&gWindowListLock, OldIrql);
        return FALSE;
    }

    LONG OrigX = 0, OrigY = 0;
    WndGetClientOriginLocked(Wnd, &OrigX, &OrigY);
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    LocalPt.x -= OrigX;
    LocalPt.y -= OrigY;

    if (IsUser)
    {
        _VEA_TRY
        {
            RtlCopyMemory(lpPoint, &LocalPt, sizeof(POINT));
        }
        _VEA_EXCEPT(1)
        {
            return FALSE;
        }
        _VEA_END
    }
    else
    {
        RtlCopyMemory(lpPoint, &LocalPt, sizeof(POINT));
    }

    return TRUE;
}

HWND
VEAPI
VeaUserWindowFromPoint(
    IN LONG ScreenX,
    IN LONG ScreenY
)
/*++

Routine Description:

    This routine performs a hit test at the specified screen coordinates and
    returns the handle of the topmost visible window containing that point.

Arguments:

    ScreenX - Supplies the horizontal screen coordinate.

    ScreenY - Supplies the vertical screen coordinate.

Return Value:

    HWND - Returns the handle of the window at the specified point, or NULL
           if no window is hit.

--*/
{
    LONG ClientX = 0, ClientY = 0;
    PWND_OBJECT Hit = WndHitTestPoint(ScreenX, ScreenY, &ClientX, &ClientY);
    if (!Hit)
    {
        return NULL;
    }

    HWND hWnd = Hit->hWnd;
    WndDereference(Hit);
    return hWnd;
}

ULONG
WndGetCaptionButtonVisualState(
    IN HWND  hWnd,
    IN ULONG BtnId
)
/*++

Routine Description:

    This routine determines the current visual state (normal, hover, or
    pressed) of a window's caption bar button based on the global hot and
    pressed caption tracking state.

Arguments:

    hWnd - Supplies the handle to the window owning the caption button.

    BtnId - Supplies the identifier of the caption button to query.

Return Value:

    ULONG - Returns CAPTION_STATE_PRESSED, CAPTION_STATE_HOVER, or
            CAPTION_STATE_NORMAL.

--*/
{
    if (gPressedCaptionWnd == hWnd && gPressedCaptionBtn == BtnId)
    {
        return (gHotCaptionWnd == hWnd && gHotCaptionBtn == BtnId)
               ? CAPTION_STATE_PRESSED
               : CAPTION_STATE_NORMAL;
    }

    if (gPressedCaptionWnd == NULL && gHotCaptionWnd == hWnd && gHotCaptionBtn == BtnId)
    {
        return CAPTION_STATE_HOVER;
    }

    return CAPTION_STATE_NORMAL;
}

VOID
WndScreenToClient(
    IN     PWND_OBJECT Wnd,
    IN OUT PPOINT      lpPoint
)
/*++

Routine Description:

    This internal routine converts a point from screen coordinates to
    client-area coordinates for the specified window object by subtracting
    its screen client origin under the global window list lock.

Arguments:

    Wnd - Supplies a pointer to the target window object.

    lpPoint - Supplies the screen coordinates and receives the converted
              client coordinates.

Return Value:

    None.

--*/
{
    KIRQL OldIrql;
    LONG OrigX = 0, OrigY = 0;

    if (!Wnd || !lpPoint)
    {
        return;
    }

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    WndGetClientOriginLocked(Wnd, &OrigX, &OrigY);
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    lpPoint->x -= OrigX;
    lpPoint->y -= OrigY;
}

/* EOF */