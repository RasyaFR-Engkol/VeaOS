/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : userapi.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VeaOS User API Shared Library (userapi.so)
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "userapi_priv.h"
#include <psdk/userapi.h>
#include <psdk/gdiapi.h>

/* Revision History ------------------------------------------------------
- DATE       : 21-11-2026
- NAME       : RasyaFR-Engkol
- REVISION   : Creating file userapi.c
- --------------------------------------------------------------------- */

static HINSTANCE ghUserApiInstance = NULL;
HMODULE gUserApiModule = NULL;
BOOLEAN UserpDcInitialized = FALSE;

extern VOID
UserapiRegisterSystemControls(VOID);

typedef struct _USER_DC_WND_ENTRY {
    LIST_ENTRY ListEntry;
    HDC        hDC;
    HWND       hWnd;
} USER_DC_WND_ENTRY, *PUSER_DC_WND_ENTRY;

static LIST_ENTRY gUserDcWndListHead;

VOID
UserapiResetDcAttr(
    IN HDC hdc
);

VOID
UserapiInitDcAttr(
    IN HDC  hdc,
    IN HWND hWnd
);

static
VOID
UserpRegisterDcWindow(
    IN HDC  hDC,
    IN HWND hWnd
)
{
    PLIST_ENTRY Entry;
    PUSER_DC_WND_ENTRY Node;

    if (!hDC || !hWnd)
    {
        return;
    }

    for (Entry = gUserDcWndListHead.Flink;
         Entry != &gUserDcWndListHead;
         Entry = Entry->Flink)
    {
        Node = CONTAINING_RECORD(Entry, USER_DC_WND_ENTRY, ListEntry);
        if (Node->hDC == hDC)
        {
            Node->hWnd = hWnd;
            return;
        }
    }

    Node = (PUSER_DC_WND_ENTRY)RtlAllocateHeap(
        RtlGetProcessHeap(),
        0,
        sizeof(USER_DC_WND_ENTRY)
    );
    if (!Node)
    {
        return;
    }

    Node->hDC  = hDC;
    Node->hWnd = hWnd;
    InsertTailList(&gUserDcWndListHead, &Node->ListEntry);
}

static
VOID
UserpUnregisterDcWindow(
    IN HDC hDC
)
{
    PLIST_ENTRY Entry;

    if (!hDC)
    {
        return;
    }

    for (Entry = gUserDcWndListHead.Flink;
         Entry != &gUserDcWndListHead;
         Entry = Entry->Flink)
    {
        PUSER_DC_WND_ENTRY Node = CONTAINING_RECORD(Entry, USER_DC_WND_ENTRY, ListEntry);
        if (Node->hDC == hDC)
        {
            RemoveEntryList(&Node->ListEntry);
            RtlFreeHeap(RtlGetProcessHeap(), 0, Node);
            return;
        }
    }
}

HWND
WINAPI
CreateWindowExA(
    IN DWORD     dwExStyle,
    IN PSTR    lpClassName,
    IN PSTR    lpWindowName,
    IN DWORD     dwStyle,
    IN INT       X,
    IN INT       Y,
    IN INT       nWidth,
    IN INT       nHeight,
    IN HWND      hWndParent,
    IN HMENU     hMenu,
    IN HINSTANCE hInstance,
    IN PVOID    lpParam
)
{
    return VeaUserCreateWindowEx(
        dwExStyle,
        (PCHAR)lpClassName,
        (PCHAR)lpWindowName,
        dwStyle,
        X,
        Y,
        nWidth,
        nHeight,
        hWndParent,
        hMenu,
        hInstance,
        lpParam
    );
}

BOOL
WINAPI
PostMessageA(
    IN HWND hWnd,
    IN UINT msg,
    IN WPARAM wParam,
    IN LPARAM lParam
)
{
    return VeaUserPostMessage(
        hWnd,
        msg,
        wParam,
        lParam
    );
}

BOOL
WINAPI
GetMessageA(
    OUT PMSG lpMsg,
    IN  HWND hWnd,
    IN  UINT wMsgFilterMin,
    IN  UINT wMsgFilterMax
)
{
    MESSAGE_QUEUE RawMsg;
    VEASTATUS Status;

    if (!lpMsg)
    {
        return -1;
    }

    RtlZeroMemory(&RawMsg, sizeof(MESSAGE_QUEUE));

    Status = VeaUserGetMessage(
        &RawMsg,
        (PVOID)hWnd,
        wMsgFilterMin,
        wMsgFilterMax
    );

    if (!VEA_SUCCESS(Status))
    {
        return -1;
    }

    lpMsg->hwnd    = RawMsg.hwnd;
    lpMsg->message = RawMsg.message;
    lpMsg->wParam  = RawMsg.wParam;
    lpMsg->lParam  = RawMsg.lParam;
    lpMsg->time    = RawMsg.time;
    lpMsg->pt.x    = 0;
    lpMsg->pt.y    = 0;

    if (lpMsg->message == WM_QUIT)
    {
        return 0;
    }

    return 1;
}

BOOL
WINAPI
TranslateMessage(
    IN CONST MSG *lpMsg
)
{
    MESSAGE_QUEUE RawMsg;

    if (!lpMsg)
    {
        return FALSE;
    }

    RtlZeroMemory(&RawMsg, sizeof(MESSAGE_QUEUE));
    RawMsg.hwnd    = lpMsg->hwnd;
    RawMsg.message = lpMsg->message;
    RawMsg.wParam  = lpMsg->wParam;
    RawMsg.lParam  = lpMsg->lParam;
    RawMsg.time    = lpMsg->time;

    return VeaUserTranslateMessage(&RawMsg) ? TRUE : FALSE;
}

LRESULT
WINAPI
DispatchMessageA(
    IN CONST MSG *lpMsg
)
{
    MESSAGE_QUEUE RawMsg;

    if (!lpMsg)
    {
        return 0;
    }

    RtlZeroMemory(&RawMsg, sizeof(MESSAGE_QUEUE));
    RawMsg.hwnd    = lpMsg->hwnd;
    RawMsg.message = lpMsg->message;
    RawMsg.wParam  = lpMsg->wParam;
    RawMsg.lParam  = lpMsg->lParam;
    RawMsg.time    = lpMsg->time;

    return VeaUserDispatchMessage(&RawMsg);
}

HDC
WINAPI
BeginPaint(
    IN  HWND          hWnd,
    OUT LPPAINTSTRUCT lpPaint
)
{
    if (!hWnd || !lpPaint)
    {
        return NULL;
    }

    HDC hdc = VeaUserBeginPaint(hWnd, lpPaint);
    if (hdc)
    {
        UserpRegisterDcWindow(hdc, hWnd);
        GdiInitDcAttr(hdc);
    }

    return hdc;
}

BOOL
WINAPI
EndPaint(
    IN HWND               hWnd,
    IN CONST PAINTSTRUCT *lpPaint
)
{
    if (!hWnd || !lpPaint)
    {
        return FALSE;
    }

    HDC hdc = lpPaint->hdc;
    if (!VeaUserEndPaint(hWnd, lpPaint))
    {
        return FALSE;
    }

    if (hdc)
    {
        UserpUnregisterDcWindow(hdc);
        GdiResetDcAttr(hdc);
    }

    return TRUE;
}

BOOL
WINAPI
InvalidateRect(
    IN HWND hWnd,
    IN CONST RECT *lpRect,
    IN BOOL bErase
)
{
    if (!hWnd)
    {
        return FALSE;
    }

    return VeaUserInvalidateRect(hWnd, (PRECT)lpRect, bErase ? TRUE : FALSE) ? TRUE : FALSE;
}

INT
WINAPI
FillRect(
    IN HDC        hDC,
    IN CONST RECT *lprc,
    IN HBRUSH     hbr
)
{
    if (!hDC || !lprc || !hbr)
    {
        return 0;
    }

    return VeaGdiFillRectBrush(hDC, (PRECT)lprc, hbr) ? 1 : 0;
}

LRESULT
WINAPI
DefWindowProcA(
    IN HWND hWnd,
    IN UINT Msg,
    IN WPARAM wParam,
    IN LPARAM lParam
)
{
    switch (Msg)
    {
        case WM_SETTEXT:
            return VeaUserSetWindowText(hWnd, (CONST CHAR *)lParam) ? TRUE : FALSE;

        case WM_GETTEXT:
            if (!lParam || (INT)wParam <= 0)
            {
                return 0;
            }
            return (LRESULT)VeaUserGetWindowText(hWnd, (PCHAR)lParam, (INT)wParam);

        case WM_GETTEXTLENGTH:
        {
            CHAR Temp[64];
            return (LRESULT)VeaUserGetWindowText(hWnd, Temp, sizeof(Temp));
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            if (BeginPaint(hWnd, &ps))
            {
                EndPaint(hWnd, &ps);
            }
            return 0;
        }

        case WM_ACTIVATE:
            if (LOWORD(wParam) != WA_INACTIVE)
            {
                SetFocus(hWnd);
            }
            return 0;

        case WM_SYSCOMMAND:
        {
            ULONG Cmd = (ULONG)(wParam & 0xFFF0);
            if (Cmd == SC_MOVE || Cmd == SC_SIZE)
            {
                UserDefDoSizeMoveLoop(hWnd, wParam, lParam);
            }
            else if (Cmd == SC_MINIMIZE)
            {
                ShowWindow(hWnd, SW_MINIMIZE);
            }
            else if (Cmd == SC_MAXIMIZE)
            {
                ShowWindow(hWnd, SW_MAXIMIZE);
            }
            else if (Cmd == SC_RESTORE)
            {
                ShowWindow(hWnd, SW_RESTORE);
            }
            else if (Cmd == SC_CLOSE)
            {
                SendMessageA(hWnd, WM_CLOSE, 0, 0);
            }
            return 0;
        }

        case WM_ERASEBKGND:
        {
            HDC hdc = (HDC)wParam;
            HBRUSH hbr = (HBRUSH)(ULONG_PTR)GetClassLongA(hWnd, GCL_HBRBACKGROUND);
            if (!hdc || !hbr)
            {
                return 0;
            }

            RECT rcClient;
            if (GetClientRect(hWnd, &rcClient))
            {
                FillRect(hdc, &rcClient, hbr);
            }
            return 1;
        }

        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_CTLCOLORSTATIC:
        {
            HBRUSH hBrush = (HBRUSH)(ULONG_PTR)GetClassLongA(hWnd, GCL_HBRBACKGROUND);
            return (LRESULT)(hBrush ? hBrush : (HBRUSH)(COLOR_BTNFACE + 1));
        }

        case WM_NCCREATE:
            return TRUE;

        case WM_NCHITTEST:
            return UserDefNcHitTest(
                hWnd,
                (LONG)(SHORT)(lParam & 0xFFFF),
                (LONG)(SHORT)((lParam >> 16) & 0xFFFF)
            );

        case WM_SETCURSOR:
        {
            USHORT Hit = (USHORT)(lParam & 0xFFFF);
            PSTR CursorId = IDC_ARROW;

            if (Hit == HTLEFT || Hit == HTRIGHT)
                CursorId = IDC_SIZEWE;
            else if (Hit == HTTOP || Hit == HTBOTTOM)
                CursorId = IDC_SIZENS;
            else if (Hit == HTTOPLEFT || Hit == HTBOTTOMRIGHT)
                CursorId = IDC_SIZENWSE;
            else if (Hit == HTTOPRIGHT || Hit == HTBOTTOMLEFT)
                CursorId = IDC_SIZENESW;

            SetCursor(LoadCursorA(NULL, CursorId));
            return TRUE;
        }

        case WM_NCMOUSEMOVE:
        {
            ULONG BtnId = CAPTION_BTN_NONE;
            if (wParam == HTMINBUTTON) BtnId = CAPTION_BTN_MIN;
            else if (wParam == HTMAXBUTTON) BtnId = CAPTION_BTN_MAX;
            else if (wParam == HTCLOSE) BtnId = CAPTION_BTN_CLOSE;

            VeaUserSetCaptionButtonState(hWnd, BtnId, CAPTION_BTN_NONE);
            return 0;
        }

        case WM_NCLBUTTONDOWN:
        {
            if (wParam == HTCAPTION)
            {
                SendMessageA(hWnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, lParam);
            }
            else if (wParam >= HTLEFT && wParam <= HTBOTTOMRIGHT)
            {
                ULONG Edge = (ULONG)(wParam - HTLEFT + 1);
                SendMessageA(hWnd, WM_SYSCOMMAND, SC_SIZE + Edge, lParam);
            }
            else if (wParam == HTMINBUTTON || wParam == HTMAXBUTTON || wParam == HTCLOSE)
            {
                UserDefTrackCaptionButton(hWnd, wParam);
            }
            return 0;
        }

        default:
            return 0;
    }
}

LRESULT
WINAPI
SendMessageA(
    IN HWND   hWnd,
    IN UINT   Msg,
    IN WPARAM wParam,
    IN LPARAM lParam
)
{
    return VeaUserSendMessage(hWnd, Msg, wParam, lParam);
}

HWND
WINAPI
SetFocus(
    IN HWND hWnd
)
{
    if (!hWnd) return NULL;

    return VeaUserSetFocus(hWnd);
}

HDC
WINAPI
GetDCEx(
    IN HWND  hWnd,
    IN HRGN  hrgnClip,
    IN DWORD flags
)
{
    DWORD CleanFlags = flags & ~DCX_INTERNAL_RELEASE;
    HDC hDC = VeaUserGetDCEx(hWnd, (HANDLE)hrgnClip, CleanFlags);
    if (hDC)
    {
        UserpRegisterDcWindow(hDC, hWnd);
        GdiInitDcAttr(hDC);
    }
    return hDC;
}

HDC
WINAPI
GetDC(
    IN HWND hWnd
)
{
    DWORD Flags = (hWnd != NULL) ? (DCX_CACHE | DCX_USESTYLE) : (DCX_CACHE | DCX_WINDOW);
    return GetDCEx(hWnd, NULL, Flags);
}

HDC
WINAPI
GetWindowDC(
    IN HWND hWnd
)
{
    DWORD Flags = DCX_WINDOW | ((hWnd != NULL) ? DCX_USESTYLE : DCX_CACHE);
    return GetDCEx(hWnd, NULL, Flags);
}

INT
WINAPI
ReleaseDC(
    IN HWND hWnd,
    IN HDC  hDC
)
{
    if (!hDC)
    {
        return 0;
    }

    UserpUnregisterDcWindow(hDC);
    GdiResetDcAttr(hDC);
    return (VeaUserGetDCEx(hWnd, (HANDLE)hDC, DCX_INTERNAL_RELEASE) != NULL) ? 1 : 0;
}

INT
WINAPI
DrawTextA(
    IN     HDC    hdc,
    IN     PSTR   lpchText,
    IN     INT    cchText,
    IN OUT LPRECT lprc,
    IN     UINT   format
)
{
    CHAR  LocalBuf[256];
    ULONG Len = 0;
    SIZE  TextSize;

    if (!hdc || !lpchText || !lprc)
    {
        return 0;
    }

    if (cchText < 0)
    {
        while (lpchText[Len] != '\0' && Len < (sizeof(LocalBuf) - 1))
        {
            LocalBuf[Len] = lpchText[Len];
            Len++;
        }
    }
    else
    {
        ULONG Limit = (ULONG)cchText;
        if (Limit > (sizeof(LocalBuf) - 1)) Limit = sizeof(LocalBuf) - 1;
        while (Len < Limit && lpchText[Len] != '\0')
        {
            LocalBuf[Len] = lpchText[Len];
            Len++;
        }
    }
    LocalBuf[Len] = '\0';

    if (!GetTextExtentPoint32A(hdc, LocalBuf, (INT)Len, &TextSize))
    {
        return 0;
    }

    /* Mode pengukuran saja tanpa menggambar ke layar */
    if (format & DT_CALCRECT)
    {
        lprc->right  = lprc->left + TextSize.cx;
        lprc->bottom = lprc->top  + TextSize.cy;
        return (INT)TextSize.cy;
    }

    LONG RectW = lprc->right - lprc->left;
    LONG RectH = lprc->bottom - lprc->top;
    INT DrawX  = (INT)lprc->left;
    INT DrawY  = (INT)lprc->top;

    /* Horizontal alignment */
    if (format & DT_CENTER)
    {
        DrawX = (INT)lprc->left + ((INT)RectW - (INT)TextSize.cx) / 2;
    }
    else if (format & DT_RIGHT)
    {
        DrawX = (INT)lprc->right - (INT)TextSize.cx;
    }

    /* Vertical alignment */
    if (format & DT_VCENTER)
    {
        DrawY = (INT)lprc->top + ((INT)RectH - (INT)TextSize.cy) / 2;
    }
    else if (format & DT_BOTTOM)
    {
        DrawY = (INT)lprc->bottom - (INT)TextSize.cy;
    }

    if (!TextOutA(hdc, DrawX, DrawY, LocalBuf, (INT)Len))
    {
        return 0;
    }

    return (INT)TextSize.cy;
}

HWND
WINAPI
WindowFromDC(
    IN HDC hDC
)
{
    PLIST_ENTRY Entry;

    if (!hDC)
    {
        return NULL;
    }

    for (Entry = gUserDcWndListHead.Flink;
         Entry != &gUserDcWndListHead;
         Entry = Entry->Flink)
    {
        PUSER_DC_WND_ENTRY Node = CONTAINING_RECORD(Entry, USER_DC_WND_ENTRY, ListEntry);
        if (Node->hDC == hDC)
        {
            return Node->hWnd;
        }
    }

    return NULL;
}

BOOL
WINAPI
ShowWindow(
    IN HWND hWnd,
    IN INT  nCmdShow
)
{
    if (!hWnd)
    {
        return FALSE;
    }

    return VeaUserShowWindow(hWnd, nCmdShow) ? TRUE : FALSE;
}

BOOL
WINAPI
DestroyWindow(
    IN HWND hWnd
)
{
    if (!hWnd)
    {
        return FALSE;
    }

    return VeaUserDestroyWindow(hWnd) ? TRUE : FALSE;
}

VOID
WINAPI
PostQuitMessage(
    IN INT nExitCode
)
{
    VeaUserPostQuitMessage(nExitCode);
}

UINT_PTR
WINAPI
SetTimer(
    IN HWND hWnd,
    IN UINT_PTR nIDEvent,
    IN UINT uElapse,
    IN TIMERPROC lpTimerFunc
)
{
    LRESULT Result = (LRESULT)VeaUserSetTimer(
        hWnd,
        nIDEvent,
        uElapse,
        lpTimerFunc
    );
    
    return (UINT_PTR)Result;
}

BOOL
WINAPI
KillTimer(
    IN HWND hWnd,
    IN UINT_PTR uIDEvent
)
{
    BOOLEAN Result;

    Result = VeaUserKillTimer(hWnd, uIDEvent);
    return Result ? TRUE : FALSE;
}

BOOL
WINAPI
PeekMessageA(
    OUT LPMSG Msg,
    IN HWND Hwnd OPTIONAL,
    IN UINT wMsgFilterMin,
    IN UINT wMsgFilterMax,
    IN UINT wRemoveMsg
)
{
    MESSAGE_QUEUE LocalMsg;
    VEASTATUS Status;

    if (!Msg)
    {
        return FALSE;
    }

    RtlZeroMemory(&LocalMsg, sizeof(LocalMsg));

    Status = VeaUserPeekMessage(
        &LocalMsg,
        Hwnd,
        wMsgFilterMin,
        wMsgFilterMax,
        wRemoveMsg
    );

    if (!VEA_SUCCESS(Status))
    {
        return FALSE;
    }

    Msg->hwnd    = LocalMsg.hwnd;
    Msg->message = LocalMsg.message;
    Msg->wParam  = LocalMsg.wParam;
    Msg->lParam  = LocalMsg.lParam;
    Msg->time    = LocalMsg.time;
    Msg->pt      = LocalMsg.pt;

    return TRUE;
}

VOID
WINAPI
UserapiInitDcListHead(VOID)
{
    if (UserpDcInitialized) return;

    UserpDcInitialized = TRUE;
    InitializeListHead(&gUserDcWndListHead);
}

BOOL
WINAPI
DllMain(
    IN HINSTANCE hinstDLL,
    IN ULONG     fdwReason,
    IN PVOID     lpvReserved
)
{
    UNREFERENCED_PARAMETER(lpvReserved);

    KdPrintf("DllMain of USERAPI\n");

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            ghUserApiInstance = hinstDLL;
            gUserApiModule = (HMODULE)hinstDLL;
            UserapiRegisterSystemControls();
            UserapiInitDefaultCursor();
            UserapiInitDcListHead();
            break;

        case DLL_PROCESS_DETACH:
            ghUserApiInstance = NULL;
            gUserApiModule = NULL;
            break;
    }

    return TRUE;
}

/* EOF */