/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : window.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Veapi Window Manager
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file window.c
 * --------------------------------------------------------------------- */

static LIST_ENTRY   gClassListHead;
static KSPIN_LOCK   gClassListLock;
LIST_ENTRY   gWindowListHead;
KSPIN_LOCK   gWindowListLock;
static BOOLEAN      gWndListsInitialized = FALSE;
static ULONG        gWindowCascadeCount  = 0;
HWND                    IptFocusWindow = NULL;

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
    IN CONST CHAR *ClassName
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
        if (Cls->Magic == WND_CLASS_MAGIC && Cls->Used && WndStrEqual(Cls->ClassName, ClassName))
        {
            Found = Cls;
            break;
        }
    }
    KsReleaseSpinLock(&gClassListLock, OldIrql);

    return Found;
}

PWND_OBJECT
WndLookupHandle(
    IN HWND hWnd
)
{
    if (!hWnd || MI_IS_USER_ADDRESS((ULONG_PTR)hWnd))
    {
        return NULL;
    }

    PWND_OBJECT Wnd = (PWND_OBJECT)hWnd;
    if (Wnd->Magic != WND_OBJECT_MAGIC || !Wnd->Used)
    {
        return NULL;
    }

    return Wnd;
}

static LRESULT
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

        if (Wnd->Desktop)
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

        WND_POOL_FREE(Wnd);
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
    if (WndFindClassByName(LocalClassName) != NULL)
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
    RtlCopyMemory(Cls->ClassName, LocalClassName, sizeof(LocalClassName));

    /* Cek ulang di dalam lock untuk mencegah race condition antar thread */
    KsAcquireSpinLock(&gClassListLock, &OldIrql);
    for (Entry = gClassListHead.Flink; Entry != &gClassListHead; Entry = Entry->Flink)
    {
        PWND_CLASS_OBJECT Existing = CONTAINING_RECORD(Entry, WND_CLASS_OBJECT, ListEntry);
        if (Existing->Magic == WND_CLASS_MAGIC &&
            Existing->Used &&
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
    CHAR LocalClass[64];
    CHAR LocalTitle[64];
    PWND_CLASS_OBJECT FoundClass = NULL;
    BOOLEAN IsUser = (KeGetPreviousMode() == UserMode);
    KIRQL OldIrql;

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

    FoundClass = WndFindClassByName(LocalClass);
    if (!FoundClass)
    {
        KdPrintf("[VEAPI/WND] CreateWindow gagal: class '%s' tidak ditemukan\n", LocalClass);
        return NULL;
    }

    PWND_OBJECT Wnd = (PWND_OBJECT)WND_POOL_ALLOC(sizeof(WND_OBJECT));
    if (!Wnd)
    {
        return NULL;
    }

    RtlZeroMemory(Wnd, sizeof(WND_OBJECT));

    ULONG CascadeStep = (gWindowCascadeCount++) % 10;

    Wnd->Magic        = WND_OBJECT_MAGIC;
    Wnd->Used         = TRUE;
    Wnd->hWnd         = (HWND)Wnd;
    Wnd->lpfnWndProc  = FoundClass->lpfnWndProc;
    Wnd->Style        = Style;
    Wnd->OwnerProcess = (PEPROCESS)PtGetCurrentProcess();
    Wnd->OwnerThread  = (PETHREAD)PtGetCurrentThread();

    /* Default posisi & ukuran window */
    Wnd->X            = 120 + (LONG)(CascadeStep * 24);
    Wnd->Y            = 90 + (LONG)(CascadeStep * 24);
    Wnd->Width        = 420;
    Wnd->Height       = 280;
    Wnd->NeedsPaint   = TRUE;

    RtlCopyMemory(Wnd->ClassName, LocalClass, sizeof(LocalClass));
    RtlCopyMemory(Wnd->Title, LocalTitle, sizeof(LocalTitle));

    /* Daftarkan ke global window list */
    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    InsertTailList(&gWindowListHead, &Wnd->GlobalListEntry);
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    /* Cantolkan ke Desktop aktif (Default) */
    if (GpActiveDesktop)
    {
        KsAcquireSpinLock(&GpActiveDesktop->Lock, &OldIrql);

        Wnd->Desktop = GpActiveDesktop;
        InsertTailList(&GpActiveDesktop->WindowListHead, &Wnd->DesktopListEntry);
        GpActiveDesktop->ForegroundWindow = Wnd;
        GpActiveDesktop->FocusWindow      = Wnd;

        KsReleaseSpinLock(&GpActiveDesktop->Lock, OldIrql);
    }

    /* Tetap sync IptFocusWindow buat kompatibilitas RIT */
    IptFocusWindow = Wnd->hWnd;

    /* Tembak callback WM_CREATE pertama kali ke Ring 3 */
    WndInvokeUserProc(Wnd, WM_CREATE, 0, 0);

    return Wnd->hWnd;
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

    return WndInvokeUserProc(Wnd, LocalMsg.message, LocalMsg.wParam, LocalMsg.lParam);
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

    FoundClass = WndFindClassByName(LocalClass);
    if (!FoundClass)
    {
        KdPrintf("[VEAPI/WND] CreateWindowEx gagal: class '%s' tidak ditemukan\n", LocalClass);
        return NULL;
    }

    /* Validasi parent jika WS_CHILD */
    PWND_OBJECT ParentWnd = NULL;
    if (IsChild)
    {
        ParentWnd = WndLookupHandle(hWndParent);
        if (!ParentWnd)
        {
            KdPrintf("[VEAPI/WND] CreateWindowEx gagal: hWndParent=%p tidak valid\n", hWndParent);
            return NULL;
        }
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

    Wnd->NeedsPaint = TRUE;

    RtlCopyMemory(Wnd->ClassName, LocalClass, sizeof(LocalClass));
    RtlCopyMemory(Wnd->Title, LocalTitle, sizeof(LocalTitle));

    /* Daftarkan ke global window list */
    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    InsertTailList(&gWindowListHead, &Wnd->GlobalListEntry);
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    /* Cantolkan ke Desktop aktif */
    if (GpActiveDesktop)
    {
        KsAcquireSpinLock(&GpActiveDesktop->Lock, &OldIrql);

        Wnd->Desktop = GpActiveDesktop;
        InsertTailList(&GpActiveDesktop->WindowListHead, &Wnd->DesktopListEntry);

        /* Hanya top-level window yang otomatis mengambil alih fokus desktop */
        if (!IsChild)
        {
            GpActiveDesktop->ForegroundWindow = Wnd;
            GpActiveDesktop->FocusWindow      = Wnd;
        }

        KsReleaseSpinLock(&GpActiveDesktop->Lock, OldIrql);
    }

    if (!IsChild)
    {
        IptFocusWindow = Wnd->hWnd;
    }

    KdPrintf("[VEAPI/WND] Created HWND=%p ('%s', class='%s') at (%ld,%ld %lux%lu)\n",
             Wnd->hWnd, Wnd->Title, Wnd->ClassName,
             Wnd->X, Wnd->Y, Wnd->Width, Wnd->Height);

    /* Tembak callback WM_CREATE pertama kali ke Ring 3 */
    WndInvokeUserProc(Wnd, WM_CREATE, 0, 0);

    return Wnd->hWnd;
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
    if (!Wnd || !Wnd->lpfnWndProc)
    {
        return 0;
    }

    if (Wnd->OwnerProcess == (PEPROCESS)PtGetCurrentProcess())
    {
        return WndInvokeUserProc(Wnd, Msg, wParam, lParam);
    }

    KdPrintf("[VEAPI/WND] Cross-process SendMessage belum didukung (hWnd=%p, Msg=0x%X)\n",
             hWnd, Msg);
    return 0;
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
        if (Wnd->Magic == WND_OBJECT_MAGIC &&
            Wnd->Used &&
            (Wnd->Style & WS_CHILD) &&
            Wnd->hWndParent == hWndParent)
        {
            Wnd->NeedsPaint = TRUE;
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
            (Wnd->Style & WS_VISIBLE))
        {
            Wnd->NeedsPaint = TRUE;
            Count++;
        }
    }
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    if (Count > 0)
    {
        KsSetEvent(&MsgInputEvent, FALSE);
    }

    DPRINT("[VEAPI/WND] UserInvalidateAllWindows: %lu window scheduled for repaint\n", Count);
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
            return NULL;
        }
    }

    if (GpActiveDesktop != NULL)
    {
        KsAcquireSpinLock(&GpActiveDesktop->Lock, &OldIrql);

        OldWnd = (PWND_OBJECT)GpActiveDesktop->FocusWindow;
        if (OldWnd && OldWnd->Used)
        {
            OldHwnd = OldWnd->hWnd;
        }

        GpActiveDesktop->FocusWindow = NewWnd;

        KsReleaseSpinLock(&GpActiveDesktop->Lock, OldIrql);
    }
    else
    {
        OldHwnd = IptFocusWindow;
    }

    IptFocusWindow = NewWnd ? NewWnd->hWnd : NULL;

    if (OldHwnd != IptFocusWindow)
    {
        if (OldHwnd != NULL)
        {
            VeaUserSendMessage(OldHwnd, WM_KILLFOCUS, (WPARAM)IptFocusWindow, 0);
        }
        if (IptFocusWindow != NULL)
        {
            VeaUserSendMessage(IptFocusWindow, WM_SETFOCUS, (WPARAM)OldHwnd, 0);
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
    if (!Wnd)
    {
        return FALSE;
    }

    BOOLEAN WasVisible = ((Wnd->Style & WS_VISIBLE) != 0);

    if (nCmdShow == SW_HIDE)
    {
        if (WasVisible)
        {
            Wnd->Style &= ~WS_VISIBLE;

            if (IptFocusWindow == Wnd->hWnd)
            {
                IptFocusWindow = NULL;
            }

            /* Bersihkan area window dari framebuffer & repaint sisa window lain */
            EngFillRect32(
                &GpPrimaryDisplay.PrimarySurface,
                Wnd->X,
                Wnd->Y,
                Wnd->Width,
                Wnd->Height,
                0xFF008080
            );
            UserInvalidateAllWindows();
        }
    }
    else
    {
        Wnd->Style |= WS_VISIBLE;
        Wnd->NeedsPaint = TRUE;
        WndInvalidateChildWindows(Wnd->hWnd);
        KsSetEvent(&MsgInputEvent, FALSE);
    }

    return WasVisible;
}

BOOLEAN
VEAPI
VeaUserDestroyWindow(
    IN HWND hWnd
)
{
    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    KIRQL OldIrql;

    if (!Wnd)
    {
        return FALSE;
    }

    /* Standar Win32: hanya proses pemilik window yang boleh menghancurkannya */
    if (Wnd->OwnerProcess != (PEPROCESS)PtGetCurrentProcess())
    {
        return FALSE;
    }

    /* 1. Hancurkan semua child window (STATIC, EDIT, BUTTON, dll.) terlebih dahulu */
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

        VeaUserDestroyWindow(ChildHwnd);
    }

    /* 2. Kirim WM_DESTROY lalu WM_NCDESTROY ke WndProc di Ring 3 */
    WndInvokeUserProc(Wnd, WM_DESTROY, 0, 0);

    /* Re-lookup jaga-jaga jika aplikasi sudah memanggil sesuatu saat WM_DESTROY */
    Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return TRUE;
    }

    WndInvokeUserProc(Wnd, WM_NCDESTROY, 0, 0);

    Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return TRUE;
    }

    LONG OldX = Wnd->X;
    LONG OldY = Wnd->Y;
    ULONG OldW = Wnd->Width;
    ULONG OldH = Wnd->Height;
    BOOLEAN IsChild = ((Wnd->Style & WS_CHILD) != 0);

    /* 3. Cabut dari global window list & invalidate magic */
    KsAcquireSpinLock(&gWindowListLock, &OldIrql);
    RemoveEntryList(&Wnd->GlobalListEntry);
    Wnd->Magic = 0;
    Wnd->Used  = FALSE;
    KsReleaseSpinLock(&gWindowListLock, OldIrql);

    /* 4. Cabut dari Desktop aktif */
    if (Wnd->Desktop)
    {
        KsAcquireSpinLock(&Wnd->Desktop->Lock, &OldIrql);

        if (Wnd->DesktopListEntry.Flink && Wnd->DesktopListEntry.Blink)
        {
            RemoveEntryList(&Wnd->DesktopListEntry);
        }
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

    if (IptFocusWindow == hWnd)
    {
        IptFocusWindow = NULL;
    }

    WND_POOL_FREE(Wnd);

    /* 5. Hapus bekas area top-level window di layar & jadwalkan repaint */
    if (!IsChild && OldW > 0 && OldH > 0)
    {
        EngFillRect32(&GpPrimaryDisplay.PrimarySurface, OldX, OldY, OldW, OldH, 0xFF008080);
    }
    UserInvalidateAllWindows();

    return TRUE;
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
        if (Wnd->Magic != WND_OBJECT_MAGIC || !Wnd->Used || !(Wnd->Style & WS_VISIBLE))
        {
            continue;
        }

        LONG Left, Top, Right, Bottom;
        if (Wnd->Style & WS_CHILD)
        {
            WndGetClientOriginOnScreen(Wnd, &Left, &Top);
            Right  = Left + (LONG)Wnd->Width;
            Bottom = Top  + (LONG)Wnd->Height;
        }
        else
        {
            Left   = Wnd->X;
            Top    = Wnd->Y;
            Right  = Wnd->X + (LONG)Wnd->Width;
            Bottom = Wnd->Y + (LONG)Wnd->Height;
        }

        if (ScreenX >= Left && ScreenX < Right && ScreenY >= Top && ScreenY < Bottom)
        {
            HitWnd = Wnd;
            if (Wnd->Style & WS_CHILD)
            {
                HitOriginX = Left;
                HitOriginY = Top;
            }
            else
            {
                HitOriginX = Wnd->X + WND_BORDER_WIDTH;
                HitOriginY = Wnd->Y + WND_TITLEBAR_HEIGHT;
            }
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
    while (Cur && (Cur->Style & WS_CHILD) && Cur->hWndParent != NULL)
    {
        PWND_OBJECT Parent = WndLookupHandle(Cur->hWndParent);
        if (!Parent) break;
        Cur = Parent;
    }
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

    /* Pindahkan gambar kursor di layar (panggil EngMovePointer jika sudah siap) */
    if (Moved)
    {
        /* EngMovePointer(gCursorX, gCursorY); */
    }

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
            ClientX = gCursorX - (Target->X + WND_BORDER_WIDTH);
            ClientY = gCursorY - (Target->Y + WND_TITLEBAR_HEIGHT);
        }
    }
    else
    {
        gCaptureWindow = NULL;
        Target = WndHitTestPoint(gCursorX, gCursorY, &ClientX, &ClientY);
    }

    /* Tetap update status tombol global walaupun klik jatuh di luar window */
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

    if (!Target)
    {
        return;
    }

    /* Pack koordinat signed 16-bit (penting saat ClientX/Y negatif ketika drag keluar window) */
    LONG_PTR ClientLParam = ((LONG_PTR)(SHORT)ClientY << 16) | ((ULONG_PTR)(USHORT)ClientX);

    /* 3. Kirim pesan gerakan mouse */
    if (Moved)
    {
        MsgPostMouseMessage(Target->hWnd, WM_MOUSEMOVE, gMouseButtonState, ClientLParam);
    }

    /* 4. Kirim pesan klik & pindahkan fokus ke window/control yang diklik */
    if (Mou->ButtonFlags & MOUSE_LEFT_BUTTON_DOWN)
    {
        if (GpActiveDesktop)
        {
            KIRQL OldIrql;
            KsAcquireSpinLock(&GpActiveDesktop->Lock, &OldIrql);
            GpActiveDesktop->FocusWindow      = Target;
            GpActiveDesktop->ForegroundWindow = WndGetTopLevelAncestor(Target);
            KsReleaseSpinLock(&GpActiveDesktop->Lock, OldIrql);
        }
        IptFocusWindow = Target->hWnd;

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
        ULONG_PTR WheelWParam = ((ULONG_PTR)(USHORT)Mou->ButtonData << 16) | gMouseButtonState;
        LONG_PTR  ScreenLParam = ((LONG_PTR)(SHORT)gCursorY << 16) | ((ULONG_PTR)(USHORT)gCursorX);
        MsgPostMouseMessage(Target->hWnd, WM_MOUSEWHEEL, WheelWParam, ScreenLParam);
    }
}

/* EOF */