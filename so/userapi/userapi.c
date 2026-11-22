/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : userapi.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VeaOS User API Shared Library (userapi.so)
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <psdk/userapi.h>

/* Revision History ------------------------------------------------------
- DATE       : 21-11-2026
- NAME       : RasyaFR-Engkol
- REVISION   : Creating file userapi.c
- --------------------------------------------------------------------- */

static HINSTANCE ghUserApiInstance = NULL;

extern VOID
UserapiRegisterSystemControls(VOID);

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

    return VeaUserBeginPaint(hWnd, lpPaint);
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
    IN HDC hDC,
    IN CONST RECT *lprc,
    IN ULONG ColorARGB
)
{
    if (!hDC || !lprc)
    {
        return 0;
    }

    return VeaGdiFillRect(hDC, (PRECT)lprc, ColorARGB) ? 1 : 0;
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
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    switch (Msg)
    {
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
            if ((wParam & 0xFFF0) == SC_CLOSE)
            {
                SendMessageA(hWnd, WM_CLOSE, 0, 0);
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_CLOSE:
            DestroyWindow(hWnd);
            PostQuitMessage(0);
            return 0;

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

BOOL
WINAPI
SetWindowTextA(
    IN HWND   hWnd,
    IN PSTR lpString
)
{
    return (SendMessageA(hWnd, WM_SETTEXT, 0, (LPARAM)lpString) != 0);
}

INT
WINAPI
GetWindowTextA(
    IN HWND  hWnd,
    IN PSTR lpString,
    IN INT   nMaxCount
)
{
    if (!lpString || nMaxCount <= 0) return 0;
    return (INT)SendMessageA(hWnd, WM_GETTEXT, (WPARAM)nMaxCount, (LPARAM)lpString);
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

    /* Di Win32, cHeight negatif (-16) = character height dalam pixel,
       cHeight positif (16) = cell height, 0 = ukuran default (14px) */
    LONG RawHeight = lplf->lfHeight;
    ULONG PixelHeight = 14;
    if (RawHeight < 0)
    {
        PixelHeight = (ULONG)(-RawHeight);
    }
    else if (RawHeight > 0)
    {
        PixelHeight = (ULONG)RawHeight;
    }

    HFONT hFont = VeaGdiCreateFont(NULL, PixelHeight);

    /* Jika varian Bold.ttf tidak ada di disk, otomatis fallback ke file default */
    if (!hFont)
    {
        hFont = VeaGdiCreateFont("D1P1:/VeaOS/Fonts/VerdanaNow.ttf", PixelHeight);
    }

    return hFont;
}

HFONT
WINAPI
CreateFontA(
    IN INT    cHeight,
    IN INT    cWidth,
    IN INT    cEscapement,
    IN INT    cOrientation,
    IN INT    cWeight,
    IN DWORD  bItalic,
    IN DWORD  bUnderline,
    IN DWORD  bStrikeOut,
    IN DWORD  iCharSet,
    IN DWORD  iOutPrecision,
    IN DWORD  iClipPrecision,
    IN DWORD  iQuality,
    IN DWORD  iPitchAndFamily,
    IN PSTR pszFaceName
)
{
    LOGFONTA lf;
    RtlZeroMemory(&lf, sizeof(lf));

    lf.lfHeight         = cHeight;
    lf.lfWidth          = cWidth;
    lf.lfEscapement     = cEscapement;
    lf.lfOrientation    = cOrientation;
    lf.lfWeight         = cWeight;
    lf.lfItalic         = (BYTE)(bItalic != 0);
    lf.lfUnderline      = (BYTE)(bUnderline != 0);
    lf.lfStrikeOut      = (BYTE)(bStrikeOut != 0);
    lf.lfCharSet        = (BYTE)iCharSet;
    lf.lfOutPrecision   = (BYTE)iOutPrecision;
    lf.lfClipPrecision  = (BYTE)iClipPrecision;
    lf.lfQuality        = (BYTE)iQuality;
    lf.lfPitchAndFamily = (BYTE)iPitchAndFamily;

    if (pszFaceName)
    {
        for (ULONG i = 0; i < (LF_FACESIZE - 1) && pszFaceName[i] != '\0'; i++)
        {
            lf.lfFaceName[i] = pszFaceName[i];
        }
    }

    return CreateFontIndirectA(&lf);
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
    return (HGDIOBJ)SelectFont(hdc, (HFONT)h);
}

BOOL
WINAPI
DeleteObject(
    IN HGDIOBJ ho
)
{
    return DeleteFont((HFONT)ho);
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
    ULONG   TextColor;
} USER_DC_ATTR;

#define MAX_USER_DC_ATTRS 32
static USER_DC_ATTR gUserDcAttrs[MAX_USER_DC_ATTRS];

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
    UserapiRegisterSystemControls();

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            ghUserApiInstance = hinstDLL;
            break;

        case DLL_PROCESS_DETACH:
            ghUserApiInstance = NULL;
            break;
    }

    return TRUE;
}

__attribute__((constructor))
static VOID
UserapiElfInit(VOID)
{
    DllMain((HINSTANCE)&DllMain, DLL_PROCESS_ATTACH, NULL);
}

__attribute__((destructor))
static VOID
UserapiElfFini(VOID)
{
    DllMain((HINSTANCE)&DllMain, DLL_PROCESS_DETACH, NULL);
}

/* EOF */