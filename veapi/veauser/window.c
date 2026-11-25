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
static HWND gDragWindow  = NULL;
static LONG gDragOffsetX = 0;
static LONG gDragOffsetY = 0;
static HWND  gHotCaptionWnd     = NULL;
static ULONG gHotCaptionBtn     = CAPTION_BTN_NONE;
static HWND  gPressedCaptionWnd = NULL;
static ULONG gPressedCaptionBtn = CAPTION_BTN_NONE;

/* PRIVATE *****************************************************************************/

static VOID
WndExecuteCaptionButton(
    IN HWND  hWnd,
    IN ULONG BtnId
)
{
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return;
    }

    ULONG Style = Wnd->Style;
    WndDereference(Wnd);

    switch (BtnId)
    {
        case CAPTION_BTN_MIN:
            if (Style & WS_MINIMIZE)
            {
                MsgPostToWindow(hWnd, WM_SYSCOMMAND, SC_RESTORE, 0, FALSE);
            }
            else
            {
                MsgPostToWindow(hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0, FALSE);
            }
            break;

        case CAPTION_BTN_MAX:
            if (Style & WS_MAXIMIZE)
            {
                MsgPostToWindow(hWnd, WM_SYSCOMMAND, SC_RESTORE, 0, FALSE);
            }
            else
            {
                MsgPostToWindow(hWnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0, FALSE);
            }
            break;

        case CAPTION_BTN_CLOSE:
            MsgPostToWindow(hWnd, WM_SYSCOMMAND, SC_CLOSE, 0, FALSE);
            break;

        default:
            break;
    }
}

static VOID
WndInvalidateWindowsInRect(
    IN CONST RECT *DirtyRect
)
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

static VOID
WndBlitMoveWindow(
    IN PVEAGDI_SURFACE Surface,
    IN LONG            OldX,
    IN LONG            OldY,
    IN LONG            NewX,
    IN LONG            NewY,
    IN LONG            Width,
    IN LONG            Height
)
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
                        if (GWnd == (PWND_OBJECT)Cur->hWndParent &&
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

    /* 3. Masukkan kembali [TopWnd, Child1, Child2, ...] ke tail gWindowListHead */
    while (!IsListEmpty(&GroupHead))
    {
        Entry = RemoveHeadList(&GroupHead);
        InsertTailList(&gWindowListHead, Entry);
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
{
    if (Desktop && Desktop->Name.Buffer &&
        RtlRawStringEqual(Desktop->Name.Buffer, "Winlogon"))
    {
        return 0xFF003366;
    }
    return 0xFF008080;
}

PWND_OBJECT
WndLookupHandleLocked(
    IN HWND hWnd
)
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
        if (gDragWindow == Wnd->hWnd) 
            gDragWindow = NULL;

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

    if (!LocalMsg.hwnd)
    {
        return 0;
    }

    Wnd = WndLookupHandle(LocalMsg.hwnd);
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
{
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    KIRQL ListIrql, DeskIrql;
    if (!Wnd)
    {
        return FALSE;
    }

    if (Wnd->OwnerProcess != (PEPROCESS)PtGetCurrentProcess())
    {
        WndDereference(Wnd);
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
            PDESKTOP_OBJECT Desktop = Wnd->Desktop;
            PWND_OBJECT NextTop     = NULL;
            PWND_OBJECT FallbackAnc = NULL;

            if (gDragWindow == hWnd)
            {
                gDragWindow  = NULL;
                gDragOffsetX = 0;
                gDragOffsetY = 0;
            }
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
                    BOOLEAN WasFg = (PrevFg == Wnd);
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

            if (!IsChild && (GpActiveDesktop == NULL || Wnd->Desktop == GpActiveDesktop))
            {
                /* Pulihkan area shadow di bawah window sebelum badan window dihapus */
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
        Wnd->X     = 0;
        Wnd->Y     = 0;
        Wnd->Width = ScrW;
        Wnd->Height = ScrH;
        Wnd->NeedsPaint = TRUE;
        KsReleaseSpinLock(&gWindowListLock, ListIrql);

        WndBringToFront(Wnd);
        WndInvalidateChildWindows(Wnd->hWnd);
        MsgNotifyPaintAll();
    }
    else if (!IsChild && (nCmdShow == SW_MINIMIZE || nCmdShow == SW_SHOWMINIMIZED))
    {
        ULONG ScrH = GpPrimaryDisplay.PrimarySurface.Height;
        if (ScrH == 0) ScrH = 600;

        LONG  OldX = Wnd->X;
        LONG  OldY = Wnd->Y;
        ULONG OldW = Wnd->Width;
        ULONG OldH = Wnd->Height;

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
        Wnd->Style      = (Wnd->Style & ~WS_MAXIMIZE) | WS_MINIMIZE | WS_VISIBLE;
        Wnd->Width      = 190;
        Wnd->Height     = WND_TITLEBAR_HEIGHT + WND_BORDER_WIDTH;
        Wnd->X          = 8;
        Wnd->Y          = (LONG)ScrH - (LONG)Wnd->Height - 8;
        Wnd->NeedsPaint = TRUE;
        KsReleaseSpinLock(&gWindowListLock, ListIrql);

        if (GpActiveDesktop == NULL || Wnd->Desktop == GpActiveDesktop)
        {
            EngHidePointer();
            EngFillDesktopRect(OldX, OldY, OldW, OldH,
                               WndGetDesktopBgColor(GpActiveDesktop));
            EngShowPointer();
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

        WndBringToFront(Wnd);

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
            WndBringToFront(Wnd);

            if (Wnd->Desktop != NULL)
            {
                KsAcquireSpinLock(&Wnd->Desktop->Lock, &DeskIrql);
                PWND_OBJECT PrevFg    = (PWND_OBJECT)Wnd->Desktop->ForegroundWindow;
                PWND_OBJECT PrevFocus = (PWND_OBJECT)Wnd->Desktop->FocusWindow;

                if (PrevFocus && PrevFocus != Wnd &&
                    PrevFocus->Magic == WND_OBJECT_MAGIC && PrevFocus->Used)
                {
                    OldFocusHwnd = PrevFocus->hWnd;
                }
                if (PrevFg && PrevFg != Wnd &&
                    PrevFg->Magic == WND_OBJECT_MAGIC && PrevFg->Used)
                {
                    PrevFg->NeedsPaint = TRUE;
                }

                Wnd->Desktop->ForegroundWindow = Wnd;
                Wnd->Desktop->FocusWindow      = Wnd;
                KsReleaseSpinLock(&Wnd->Desktop->Lock, DeskIrql);
            }
            else if (IptFocusWindow != Wnd->hWnd)
            {
                OldFocusHwnd = IptFocusWindow;
            }

            if (GpActiveDesktop == NULL || Wnd->Desktop == GpActiveDesktop)
            {
                NewFocusHwnd   = Wnd->hWnd;
                IptFocusWindow = NewFocusHwnd;
            }

            if (OldFocusHwnd != NULL && OldFocusHwnd != NewFocusHwnd)
            {
                MsgPostToWindow(OldFocusHwnd, WM_KILLFOCUS, (WPARAM)NewFocusHwnd, 0, FALSE);
            }
            if (NewFocusHwnd != NULL && NewFocusHwnd != OldFocusHwnd)
            {
                MsgPostToWindow(NewFocusHwnd, WM_SETFOCUS, (WPARAM)OldFocusHwnd, 0, FALSE);
            }
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

        if (Wnd->DesktopListEntry.Flink && Wnd->DesktopListEntry.Blink)
        {
            RemoveEntryList(&Wnd->DesktopListEntry);
            Wnd->DesktopListEntry.Flink = NULL;
            Wnd->DesktopListEntry.Blink = NULL;
        }

        BOOLEAN WasFg    = (Desktop->ForegroundWindow == Wnd);
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
    }

    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    if (NewFocusHwnd != NULL)
    {
        MsgPostToWindow(NewFocusHwnd, WM_SETFOCUS, 0, 0, FALSE);
    }

    if (gCaptureWindow == hWnd)
    {
        gCaptureWindow = NULL;
    }
    if (gDragWindow == hWnd)
    {
        gDragWindow  = NULL;
        gDragOffsetX = 0;
        gDragOffsetY = 0;
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
{
    return WndDestroyWindowInternal(hWnd, FALSE, 0);
}

VOID
VEAPI
WndAdoptOrphanWindowsToDesktop(
    IN PDESKTOP_OBJECT Desktop
)
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
        gCursorY += Mou->LastY; /* Jika driver PS/2 kamu masih inverted Y, ganti jadi -= */
        Moved = TRUE;
    }

    /* Jepit koordinat agar kursor tidak keluar batas layar */
    if (gCursorX < 0) gCursorX = 0;
    if (gCursorY < 0) gCursorY = 0;
    if (gCursorX >= ScreenW) gCursorX = ScreenW - 1;
    if (gCursorY >= ScreenH) gCursorY = ScreenH - 1;

    /* Update status tombol global walaupun klik jatuh di luar window */
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

    /* 0. Proses tombol titlebar yang sedang ditekan (Pressed state) */
    if (gPressedCaptionWnd != NULL)
    {
        PWND_OBJECT PressedWnd = WndLookupHandle(gPressedCaptionWnd);
        if (!PressedWnd || !(PressedWnd->Style & WS_VISIBLE) ||
            (GpActiveDesktop != NULL && PressedWnd->Desktop != GpActiveDesktop))
        {
            if (PressedWnd) WndDereference(PressedWnd);
            gPressedCaptionWnd = NULL;
            gPressedCaptionBtn = CAPTION_BTN_NONE;
            return;
        }

        ULONG CurHit = WndHitTestCaptionButton(PressedWnd, gCursorX, gCursorY);
        ULONG NewHot = (CurHit == gPressedCaptionBtn) ? gPressedCaptionBtn : CAPTION_BTN_NONE;

        if (Mou->ButtonFlags & MOUSE_LEFT_BUTTON_UP)
        {
            HWND  ExecHwnd = gPressedCaptionWnd;
            ULONG ExecBtn  = gPressedCaptionBtn;

            gPressedCaptionWnd = NULL;
            gPressedCaptionBtn = CAPTION_BTN_NONE;
            gHotCaptionWnd     = (NewHot != CAPTION_BTN_NONE) ? ExecHwnd : NULL;
            gHotCaptionBtn     = NewHot;

            WndRedrawCaptionButtons(PressedWnd);
            WndDereference(PressedWnd);

            if (NewHot == ExecBtn)
            {
                WndExecuteCaptionButton(ExecHwnd, ExecBtn);
            }
            return;
        }

        if (Moved && (gHotCaptionWnd != PressedWnd->hWnd || gHotCaptionBtn != NewHot))
        {
            gHotCaptionWnd = (NewHot != CAPTION_BTN_NONE) ? PressedWnd->hWnd : NULL;
            gHotCaptionBtn = NewHot;
            WndRedrawCaptionButtons(PressedWnd);
        }

        WndDereference(PressedWnd);
        return;
    }

    /* 1. Lepas drag titlebar saat tombol kiri dilepas */
    if ((Mou->ButtonFlags & MOUSE_LEFT_BUTTON_UP) && gDragWindow != NULL)
    {
        gDragWindow  = NULL;
        gDragOffsetX = 0;
        gDragOffsetY = 0;
        return;
    }

    /* 2. Proses pergeseran window jika sedang drag titlebar */
    if (Moved && (gMouseButtonState & MK_LBUTTON) && gDragWindow != NULL)
    {
        PWND_OBJECT DragWnd = WndLookupHandle(gDragWindow);
        if (!DragWnd || !(DragWnd->Style & WS_VISIBLE) ||
            (GpActiveDesktop != NULL && DragWnd->Desktop != GpActiveDesktop))
        {
            if (DragWnd) WndDereference(DragWnd);
            gDragWindow = NULL;
            return;
        }

        LONG NewX = gCursorX - gDragOffsetX;
        LONG NewY = gCursorY - gDragOffsetY;

        /* Jaga agar titlebar tetap terjangkau di layar */
        if (NewY < 0) NewY = 0;
        if (NewY > ScreenH - WND_TITLEBAR_HEIGHT) NewY = ScreenH - WND_TITLEBAR_HEIGHT;
        if (NewX + (LONG)DragWnd->Width < 32) NewX = 32 - (LONG)DragWnd->Width;
        if (NewX > ScreenW - 32) NewX = ScreenW - 32;

        if (NewX != DragWnd->X || NewY != DragWnd->Y)
        {
            KIRQL ListIrql;
            BOOLEAN HasWindowAbove = FALSE;

            WndRestoreWindowBottomShadow(DragWnd, TRUE);
            KsAcquireSpinLock(&gWindowListLock, &ListIrql);
            LONG OldX  = DragWnd->X;
            LONG OldY  = DragWnd->Y;
            LONG W     = (LONG)DragWnd->Width;
            LONG H     = (LONG)DragWnd->Height;
            DragWnd->X = NewX;
            DragWnd->Y = NewY;

            PLIST_ENTRY Entry;
            for (Entry = DragWnd->GlobalListEntry.Flink; Entry != &gWindowListHead; Entry = Entry->Flink)
            {
                PWND_OBJECT Cand = CONTAINING_RECORD(Entry, WND_OBJECT, GlobalListEntry);
                if (Cand->Magic == WND_OBJECT_MAGIC && Cand->Used &&
                    !(Cand->Style & WS_CHILD) && (Cand->Style & WS_VISIBLE) &&
                    Cand->Desktop == DragWnd->Desktop)
                {
                    HasWindowAbove = TRUE;
                    break;
                }
            }
            KsReleaseSpinLock(&gWindowListLock, ListIrql);

            PVEAGDI_SURFACE Primary = EngGetTargetSurface();
            ULONG BgColor = WndGetDesktopBgColor(GpActiveDesktop);
            BOOLEAN WasClipped = (OldX < 0 || OldY < 0 || OldX + W > ScreenW || OldY + H > ScreenH);

            RECT Strips[2];
            ULONG StripCount = 0;

            if (NewY > OldY)
            {
                LONG StripB = (OldY + H < NewY) ? (OldY + H) : NewY;
                if (StripB > OldY)
                {
                    Strips[StripCount].left   = OldX;
                    Strips[StripCount].top    = OldY;
                    Strips[StripCount].right  = OldX + W;
                    Strips[StripCount].bottom = StripB;
                    StripCount++;
                }
            }
            else if (NewY < OldY)
            {
                LONG StripT = (OldY > NewY + H) ? OldY : (NewY + H);
                if (OldY + H > StripT)
                {
                    Strips[StripCount].left   = OldX;
                    Strips[StripCount].top    = StripT;
                    Strips[StripCount].right  = OldX + W;
                    Strips[StripCount].bottom = OldY + H;
                    StripCount++;
                }
            }

            LONG MidT = (OldY > NewY) ? OldY : NewY;
            LONG MidB = (OldY + H < NewY + H) ? (OldY + H) : (NewY + H);
            if (MidB > MidT)
            {
                if (NewX > OldX)
                {
                    LONG StripR = (OldX + W < NewX) ? (OldX + W) : NewX;
                    if (StripR > OldX)
                    {
                        Strips[StripCount].left   = OldX;
                        Strips[StripCount].top    = MidT;
                        Strips[StripCount].right  = StripR;
                        Strips[StripCount].bottom = MidB;
                        StripCount++;
                    }
                }
                else if (NewX < OldX)
                {
                    LONG StripL = (OldX > NewX + W) ? OldX : (NewX + W);
                    if (OldX + W > StripL)
                    {
                        Strips[StripCount].left   = StripL;
                        Strips[StripCount].top    = MidT;
                        Strips[StripCount].right  = OldX + W;
                        Strips[StripCount].bottom = MidB;
                        StripCount++;
                    }
                }
            }

            EngHidePointer();

            if (!HasWindowAbove && Primary != NULL)
            {
                WndBlitMoveWindow(Primary, OldX, OldY, NewX, NewY, W, H);
            }

            for (ULONG s = 0; s < StripCount; s++)
            {
                LONG Sl = (Strips[s].left < 0) ? 0 : Strips[s].left;
                LONG St = (Strips[s].top < 0)  ? 0 : Strips[s].top;
                LONG Sr = (Strips[s].right > ScreenW)  ? ScreenW : Strips[s].right;
                LONG Sb = (Strips[s].bottom > ScreenH) ? ScreenH : Strips[s].bottom;

                if (Sl < Sr && St < Sb)
                {
                    if (Primary != NULL)
                    {
                        EngFillRect32(Primary, Sl, St, (ULONG)(Sr - Sl), (ULONG)(Sb - St), BgColor);
                    }
                    EngPresentRect(Sl, St, (ULONG)(Sr - Sl), (ULONG)(Sb - St));
                    WndInvalidateWindowsInRect(&Strips[s]);
                }
            }

            if (!HasWindowAbove)
            {
                LONG Wl = (NewX < 0) ? 0 : NewX;
                LONG Wt = (NewY < 0) ? 0 : NewY;
                LONG Wr = (NewX + W > ScreenW) ? ScreenW : (NewX + W);
                LONG Wb = (NewY + H > ScreenH) ? ScreenH : (NewY + H);

                if (Wl < Wr && Wt < Wb)
                {
                    EngPresentRect(Wl, Wt, (ULONG)(Wr - Wl), (ULONG)(Wb - Wt));
                }
            }

            EngShowPointer();
            WndDrawWindowBottomShadow(DragWnd, TRUE);

            /* Repaint DragWnd hanya jika ada bagian yang sebelumnya terpotong tepi layar / tertutup window lain */
            if (HasWindowAbove || WasClipped)
            {
                DragWnd->NeedsPaint = TRUE;
                WndInvalidateChildWindows(DragWnd->hWnd);
                MsgNotifyPaint(DragWnd->OwnerThread);
            }
        }

        WndDereference(DragWnd);
        return;
    }

    /* 3. Cari target window (hanya sekali lookup supaya tidak leak reference) */
    LONG ClientX = 0;
    LONG ClientY = 0;
    PWND_OBJECT Target = gCaptureWindow ? WndLookupHandle(gCaptureWindow) : NULL;

    if (Target)
    {
        if (Target->Style & WS_CHILD)
        {
            LONG OrigX, OrigY;
            WndGetClientOriginOnScreen(Target, &OrigX, &OrigY);
            ClientX = gCursorX - OrigX;
            ClientY = gCursorY - OrigY;
        }
        else
        {
            BOOLEAN Framed = !((Target->Style & WS_POPUP) && ((Target->Style & WS_CAPTION) != WS_CAPTION));
            ClientX = gCursorX - (Framed ? (Target->X + WND_BORDER_WIDTH)    : Target->X);
            ClientY = gCursorY - (Framed ? (Target->Y + WND_TITLEBAR_HEIGHT) : Target->Y);
        }
    }
    else
    {
        gCaptureWindow = NULL;
        Target = WndHitTestPoint(gCursorX, gCursorY, &ClientX, &ClientY);
    }

    if (!Target)
    {
        return;
    }

    /* 4. Cek klik pada Titlebar untuk memulai drag window */
    if ((Mou->ButtonFlags & MOUSE_LEFT_BUTTON_DOWN) &&
        !(Target->Style & WS_CHILD) &&
        !((Target->Style & WS_POPUP) && ((Target->Style & WS_CAPTION) != WS_CAPTION)))
    {
        if (gCursorY >= Target->Y + WND_BORDER_WIDTH &&
            gCursorY <  Target->Y + WND_TITLEBAR_HEIGHT)
        {
            WndBringToFront(Target);

            PDESKTOP_OBJECT Desktop = GpActiveDesktop;
            if (Desktop != NULL)
            {
                KIRQL OldIrql;
                PWND_OBJECT PrevFg    = (PWND_OBJECT)Desktop->ForegroundWindow;
                PWND_OBJECT PrevFocus = (PWND_OBJECT)Desktop->FocusWindow;

                BOOLEAN KeepChildFocus = FALSE;
                if (PrevFocus && PrevFocus->Magic == WND_OBJECT_MAGIC && PrevFocus->Used)
                {
                    KIRQL ListIrql;
                    KsAcquireSpinLock(&gWindowListLock, &ListIrql);
                    KeepChildFocus = WndIsDescendantLocked(Target, PrevFocus);
                    KsReleaseSpinLock(&gWindowListLock, ListIrql);
                }

                KsAcquireSpinLock(&Desktop->Lock, &OldIrql);
                Desktop->ForegroundWindow = Target;
                if (!KeepChildFocus)
                {
                    Desktop->FocusWindow = Target;
                    IptFocusWindow = Target->hWnd;
                }
                KsReleaseSpinLock(&Desktop->Lock, OldIrql);

                if (PrevFg && PrevFg != Target && PrevFg->Magic == WND_OBJECT_MAGIC && PrevFg->Used)
                {
                    PrevFg->NeedsPaint = TRUE;
                }
            }
            else
            {
                IptFocusWindow = Target->hWnd;
            }

            ULONG HitBtn = WndHitTestCaptionButton(Target, gCursorX, gCursorY);
            if (HitBtn != CAPTION_BTN_NONE)
            {
                gPressedCaptionWnd = Target->hWnd;
                gPressedCaptionBtn = HitBtn;
                gHotCaptionWnd     = Target->hWnd;
                gHotCaptionBtn     = HitBtn;

                Target->NeedsPaint = TRUE;
                WndInvalidateChildWindows(Target->hWnd);
                MsgNotifyPaintAll();
                WndRedrawCaptionButtons(Target);

                WndDereference(Target);
                return;
            }

            /* Jangan drag window jika sedang fullscreen maximized */
            if (!(Target->Style & WS_MAXIMIZE))
            {
                gDragWindow  = Target->hWnd;
                gDragOffsetX = gCursorX - Target->X;
                gDragOffsetY = gCursorY - Target->Y;
            }

            Target->NeedsPaint = TRUE;
            WndInvalidateChildWindows(Target->hWnd);
            MsgNotifyPaintAll();

            WndDereference(Target);
            return;
        }
    }

    /* Update efek hover saat kursor bergerak di atas 3 tombol */
    if (Moved && gDragWindow == NULL)
    {
        ULONG NewHotBtn  = WndHitTestCaptionButton(Target, gCursorX, gCursorY);
        HWND  NewHotHwnd = (NewHotBtn != CAPTION_BTN_NONE) ? Target->hWnd : NULL;

        if (NewHotHwnd != gHotCaptionWnd || NewHotBtn != gHotCaptionBtn)
        {
            HWND OldHotHwnd = gHotCaptionWnd;
            gHotCaptionWnd  = NewHotHwnd;
            gHotCaptionBtn  = NewHotBtn;

            if (OldHotHwnd != NULL && OldHotHwnd != NewHotHwnd)
            {
                PWND_OBJECT OldHotWnd = WndLookupHandle(OldHotHwnd);
                if (OldHotWnd)
                {
                    WndRedrawCaptionButtons(OldHotWnd);
                    WndDereference(OldHotWnd);
                }
            }

            if (NewHotHwnd != NULL)
            {
                WndRedrawCaptionButtons(Target);
            }
        }
    }

    /* Pack koordinat signed 16-bit (penting saat ClientX/Y negatif ketika drag keluar window) */
    LONG_PTR ClientLParam = ((LONG_PTR)(SHORT)ClientY << 16) | ((ULONG_PTR)(USHORT)ClientX);

    /* 5. Kirim pesan gerakan mouse */
    if (Moved)
    {
        MsgPostMouseMessage(Target->hWnd, WM_MOUSEMOVE, gMouseButtonState, ClientLParam);
    }

    /* 6. Kirim pesan klik & pindahkan fokus + Z-order ke window yang diklik */
    if (Mou->ButtonFlags & MOUSE_LEFT_BUTTON_DOWN)
    {
        PWND_OBJECT TopWnd    = WndGetTopLevelAncestor(Target);
        PWND_OBJECT ActiveTop = TopWnd ? TopWnd : Target;
        HWND OldFocusHwnd     = NULL;
        HWND NewFocusHwnd     = NULL;

        WndBringToFront(ActiveTop);

        BOOLEAN ShouldChangeFocus = ((Target->Style & WS_CHILD) != 0);

        PDESKTOP_OBJECT Desktop = GpActiveDesktop;
        if (Desktop != NULL)
        {
            KIRQL OldIrql;
            PWND_OBJECT PrevFg    = (PWND_OBJECT)Desktop->ForegroundWindow;
            PWND_OBJECT PrevFocus = (PWND_OBJECT)Desktop->FocusWindow;

            if (!ShouldChangeFocus)
            {
                KIRQL ListIrql;
                KsAcquireSpinLock(&gWindowListLock, &ListIrql);
                if (PrevFocus == NULL ||
                    PrevFocus->Magic != WND_OBJECT_MAGIC ||
                    !PrevFocus->Used ||
                    (PrevFocus != ActiveTop && !WndIsDescendantLocked(ActiveTop, PrevFocus)))
                {
                    ShouldChangeFocus = TRUE;
                }
                KsReleaseSpinLock(&gWindowListLock, ListIrql);
            }

            KsAcquireSpinLock(&Desktop->Lock, &OldIrql);
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
            KsReleaseSpinLock(&Desktop->Lock, OldIrql);

            if (PrevFg && PrevFg != ActiveTop && PrevFg->Magic == WND_OBJECT_MAGIC && PrevFg->Used)
            {
                PrevFg->NeedsPaint = TRUE;
                ActiveTop->NeedsPaint = TRUE;
                WndInvalidateChildWindows(ActiveTop->hWnd);
                MsgNotifyPaintAll();
            }
        }
        else if (ShouldChangeFocus)
        {
            if (IptFocusWindow != Target->hWnd)
            {
                OldFocusHwnd = IptFocusWindow;
                NewFocusHwnd = Target->hWnd;
            }
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

        MsgPostMouseMessage(Target->hWnd, WM_LBUTTONDOWN, gMouseButtonState, ClientLParam);
    }

    if (Mou->ButtonFlags & MOUSE_LEFT_BUTTON_UP)
    {
        MsgPostMouseMessage(Target->hWnd, WM_LBUTTONUP, gMouseButtonState, ClientLParam);
    }

    if (Mou->ButtonFlags & MOUSE_RIGHT_BUTTON_DOWN)
    {
        MsgPostMouseMessage(Target->hWnd, WM_RBUTTONDOWN, gMouseButtonState, ClientLParam);
    }

    if (Mou->ButtonFlags & MOUSE_RIGHT_BUTTON_UP)
    {
        MsgPostMouseMessage(Target->hWnd, WM_RBUTTONUP, gMouseButtonState, ClientLParam);
    }

    if (Mou->ButtonFlags & MOUSE_MIDDLE_BUTTON_DOWN)
    {
        MsgPostMouseMessage(Target->hWnd, WM_MBUTTONDOWN, gMouseButtonState, ClientLParam);
    }

    if (Mou->ButtonFlags & MOUSE_MIDDLE_BUTTON_UP)
    {
        MsgPostMouseMessage(Target->hWnd, WM_MBUTTONUP, gMouseButtonState, ClientLParam);
    }

    if (Mou->ButtonFlags & MOUSE_WHEEL)
    {
        ULONG_PTR WheelWParam  = ((ULONG_PTR)(USHORT)Mou->ButtonData << 16) | gMouseButtonState;
        LONG_PTR  ScreenLParam = ((LONG_PTR)(SHORT)gCursorY << 16) | ((ULONG_PTR)(USHORT)gCursorX);
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
{
    CHAR LocalBuf[64];
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    KIRQL OldIrql;

    if (lpString != NULL)
    {
        if (!WndCopyStringSafe(LocalBuf, (PCHAR)lpString, sizeof(LocalBuf), IsUser))
            return FALSE;
    }
    else
    {
        LocalBuf[0] = '\0';
    }

    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return FALSE;
    }

    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    RtlCopyMemory(Wnd->Title, LocalBuf, sizeof(LocalBuf));
    BOOLEAN ShouldPaint = WndIsEffectivelyVisibleLocked(Wnd);
    if (ShouldPaint)
    {
        Wnd->NeedsPaint = TRUE;
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    if (ShouldPaint)
    {
        MsgNotifyPaint(Wnd->OwnerThread);
    }
    
    WndDereference(Wnd);
    return TRUE;
}

BOOLEAN
VEAPI
VeaUserIsWindow(
    IN HWND hWnd
)
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

/* EOF */