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

/* Revision History ------------------------------------------------------
- DATE       : 21-11-2026
- NAME       : RasyaFR-Engkol
- REVISION   : Creating file userapi.c
- --------------------------------------------------------------------- */

static HINSTANCE ghUserApiInstance = NULL;
HMODULE gUserApiModule = NULL;

extern VOID
UserapiRegisterSystemControls(VOID);

VOID
UserapiResetDcAttr(
    IN HDC hdc
);

VOID
UserapiInitDcAttr(
    IN HDC  hdc,
    IN HWND hWnd
);

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
    IN HWND hWnd,
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
        UserapiInitDcAttr(hdc, hWnd);
    }

    return hdc;
}

BOOL
WINAPI
EndPaint(
    IN HWND hWnd,
    IN CONST PAINTSTRUCT *lpPaint
)
{
    if (!hWnd || !lpPaint)
    {
        return FALSE;
    }

    UserapiResetDcAttr(lpPaint->hdc);
    return VeaUserEndPaint(hWnd, lpPaint) ? TRUE : FALSE;
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
            switch (wParam & 0xFFF0)
            {
                case SC_MINIMIZE:
                    ShowWindow(hWnd, SW_MINIMIZE);
                    break;

                case SC_MAXIMIZE:
                    ShowWindow(hWnd, SW_MAXIMIZE);
                    break;

                case SC_RESTORE:
                    ShowWindow(hWnd, SW_RESTORE);
                    break;

                case SC_CLOSE:
                    SendMessageA(hWnd, WM_CLOSE, 0, 0);
                    break;
            }
            return 0;

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

HFONT
WINAPI
CreateFontIndirectA(
    IN CONST LOGFONTA *lplf
)
{
    if (!lplf)
    {
        return NULL;
    }

    return VeaGdiCreateFont(
        lplf->lfHeight,
        lplf->lfWeight,
        (lplf->lfItalic != 0),
        (lplf->lfFaceName[0] != '\0') ? lplf->lfFaceName : NULL
    );
}

HFONT
WINAPI
SelectFont(
    IN HDC   hdc,
    IN HFONT hFont
)
{
    if (!hdc)
    {
        return NULL;
    }

    /* hFont == NULL diizinkan untuk reset kembali ke font default sistem */
    return VeaGdiSelectFont(hdc, hFont);
}

BOOL
WINAPI
DeleteFont(
    IN HFONT hFont
)
{
    if (!hFont)
    {
        return FALSE;
    }

    return VeaGdiDeleteFont(hFont) ? TRUE : FALSE;
}

HGDIOBJ
WINAPI
SelectObject(
    IN HDC     hdc,
    IN HGDIOBJ h
)
{
    if (!hdc)
    {
        return NULL;
    }

    /* h == NULL diizinkan untuk reset font ke default sistem */
    if (h == NULL)
    {
        return (HGDIOBJ)VeaGdiSelectFont(hdc, NULL);
    }

    return VeaGdiSelectObject(hdc, h);
}

BOOL
WINAPI
DeleteObject(
    IN HGDIOBJ ho
)
{
    if (!ho)
    {
        return FALSE;
    }

    if (VeaGdiDeleteObject(ho))
    {
        return TRUE;
    }

    return VeaGdiDeleteFont((HFONT)ho) ? TRUE : FALSE;
}

BOOL
WINAPI
GetTextExtentPoint32A(
    IN  HDC    hdc,
    IN  PSTR   lpString,
    IN  INT    c,
    OUT LPSIZE psizl
)
{
    ULONG Width  = 0;
    ULONG Height = 0;

    if (!hdc || !lpString || !psizl || c < 0)
    {
        return FALSE;
    }

    if (c == 0)
    {
        psizl->cx = 0;
        psizl->cy = 0;
        return TRUE;
    }

    if (!VeaGdiGetTextExtentFont(hdc, (CONST CHAR *)lpString, &Width, &Height))
    {
        return FALSE;
    }

    psizl->cx = (LONG)Width;
    psizl->cy = (LONG)Height;
    return TRUE;
}

typedef struct _USER_DC_ATTR {
    BOOLEAN Used;
    HDC     hDC;
    HWND    hWnd;
    ULONG   TextColor;
} USER_DC_ATTR;

#define MAX_USER_DC_ATTRS 128
static USER_DC_ATTR gUserDcAttrs[MAX_USER_DC_ATTRS];

VOID
UserapiInitDcAttr(
    IN HDC  hdc,
    IN HWND hWnd
)
{
    if (!hdc) return;

    /* Reuse kalau slot dengan HDC yang sama masih ada */
    for (ULONG i = 0; i < MAX_USER_DC_ATTRS; i++)
    {
        if (gUserDcAttrs[i].Used && gUserDcAttrs[i].hDC == hdc)
        {
            gUserDcAttrs[i].hWnd      = hWnd;
            gUserDcAttrs[i].TextColor = 0xFF000000;
            return;
        }
    }

    for (ULONG i = 0; i < MAX_USER_DC_ATTRS; i++)
    {
        if (!gUserDcAttrs[i].Used)
        {
            gUserDcAttrs[i].Used      = TRUE;
            gUserDcAttrs[i].hDC       = hdc;
            gUserDcAttrs[i].hWnd      = hWnd;
            gUserDcAttrs[i].TextColor = 0xFF000000;
            return;
        }
    }
}

VOID
UserapiResetDcAttr(
    IN HDC hdc
)
{
    if (!hdc) return;

    for (ULONG i = 0; i < MAX_USER_DC_ATTRS; i++)
    {
        if (gUserDcAttrs[i].Used && gUserDcAttrs[i].hDC == hdc)
        {
            gUserDcAttrs[i].Used      = FALSE;
            gUserDcAttrs[i].hDC       = NULL;
            gUserDcAttrs[i].hWnd      = NULL;
            gUserDcAttrs[i].TextColor = 0xFF000000;
            break;
        }
    }
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
        UserapiInitDcAttr(hDC, hWnd);
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

    UserapiResetDcAttr(hDC);
    return (VeaUserGetDCEx(hWnd, (HANDLE)hDC, DCX_INTERNAL_RELEASE) != NULL) ? 1 : 0;
}

BOOL
WINAPI
DeleteDC(
    IN HDC hdc
)
{
    if (!hdc)
    {
        return FALSE;
    }

    UserapiResetDcAttr(hdc);
    return VeaGdiDeleteDC(hdc) ? TRUE : FALSE;
}

HWND
WINAPI
WindowFromDC(
    IN HDC hDC
)
{
    if (!hDC) return NULL;

    for (ULONG i = 0; i < MAX_USER_DC_ATTRS; i++)
    {
        if (gUserDcAttrs[i].Used && gUserDcAttrs[i].hDC == hDC)
        {
            return gUserDcAttrs[i].hWnd;
        }
    }

    return NULL;
}

ULONG
WINAPI
SetTextColor(
    IN HDC   hdc,
    IN ULONG color
)
{
    if (!hdc) return 0xFFFFFFFF;

    /* Pastikan format alpha 0xFF terisi jika caller pass 0x00RRGGBB */
    ULONG Argb = ((color & 0xFF000000) == 0) ? (0xFF000000 | color) : color;

    for (ULONG i = 0; i < MAX_USER_DC_ATTRS; i++)
    {
        if (gUserDcAttrs[i].Used && gUserDcAttrs[i].hDC == hdc)
        {
            ULONG Prev = gUserDcAttrs[i].TextColor;
            gUserDcAttrs[i].TextColor = Argb;
            return Prev;
        }
    }

    for (ULONG i = 0; i < MAX_USER_DC_ATTRS; i++)
    {
        if (!gUserDcAttrs[i].Used)
        {
            gUserDcAttrs[i].Used      = TRUE;
            gUserDcAttrs[i].hDC       = hdc;
            gUserDcAttrs[i].TextColor = Argb;
            return 0xFF000000;
        }
    }

    return 0xFF000000;
}

ULONG
WINAPI
GetTextColor(
    IN HDC hdc
)
{
    if (!hdc) return 0xFF000000;

    for (ULONG i = 0; i < MAX_USER_DC_ATTRS; i++)
    {
        if (gUserDcAttrs[i].Used && gUserDcAttrs[i].hDC == hdc)
        {
            return gUserDcAttrs[i].TextColor;
        }
    }

    return 0xFF000000;
}

INT
WINAPI
DrawTextA(
    IN     HDC    hdc,
    IN     PSTR lpchText,
    IN     INT    cchText,
    IN OUT LPRECT lprc,
    IN     UINT   format
)
{
    CHAR LocalBuf[256];
    ULONG Len = 0;

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

    ULONG TextW = 0;
    ULONG TextH = 0;
    if (!VeaGdiGetTextExtentFont(hdc, LocalBuf, &TextW, &TextH))
    {
        return 0;
    }

    /* Mode pengukuran saja tanpa menggambar ke layar */
    if (format & DT_CALCRECT)
    {
        lprc->right  = lprc->left + (LONG)TextW;
        lprc->bottom = lprc->top  + (LONG)TextH;
        return (INT)TextH;
    }

    LONG RectW = lprc->right - lprc->left;
    LONG RectH = lprc->bottom - lprc->top;
    INT DrawX  = (INT)lprc->left;
    INT DrawY  = (INT)lprc->top;

    /* Horizontal alignment */
    if (format & DT_CENTER)
    {
        DrawX = (INT)lprc->left + ((INT)RectW - (INT)TextW) / 2;
    }
    else if (format & DT_RIGHT)
    {
        DrawX = (INT)lprc->right - (INT)TextW;
    }

    /* Vertical alignment */
    if (format & DT_VCENTER)
    {
        DrawY = (INT)lprc->top + ((INT)RectH - (INT)TextH) / 2;
    }
    else if (format & DT_BOTTOM)
    {
        DrawY = (INT)lprc->bottom - (INT)TextH;
    }

    ULONG Color = GetTextColor(hdc);
    if (!VeaGdiDrawText(hdc, DrawX, DrawY, LocalBuf, Color))
    {
        return 0;
    }

    return (INT)TextH;
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
            break;

        case DLL_PROCESS_DETACH:
            ghUserApiInstance = NULL;
            gUserApiModule = NULL;
            break;
    }

    return TRUE;
}

/* EOF */