/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : userapi.h
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Public User-Mode Windowing & Messaging API (userapi.so)
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#pragma once

#include <procbind.h>
#include <psdk/msg.h>
#include <psdk/winuser.h>
#include <psdk/veapi3.h>
#include "veapi.h"
#include "veapi2.h"
#include "veapisyscall.h"
#include "userapi2.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef USHORT ATOM;
typedef INT    BOOL;

typedef struct tagPOINT
{
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;

typedef struct tagWNDCLASSA
{
    UINT      style;
    WNDPROC   lpfnWndProc;
    INT       cbClsExtra;
    INT       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    PCSTR     lpszMenuName;
    PCSTR     lpszClassName;
} WNDCLASSA, *PWNDCLASSA, *LPWNDCLASSA;

typedef struct tagMSG
{
    HWND   hwnd;
    UINT   message;
    WPARAM wParam;
    LPARAM lParam;
    ULONG  time;
    POINT  pt;
} MSG, *PMSG, *LPMSG;

ATOM
WINAPI
RegisterClassA(
    IN CONST WNDCLASSA *lpWndClass
);

HWND
WINAPI
CreateWindowExA(
    IN ULONG     dwExStyle,
    IN PSTR     lpClassName,
    IN PSTR     lpWindowName,
    IN ULONG     dwStyle,
    IN INT       X,
    IN INT       Y,
    IN INT       nWidth,
    IN INT       nHeight,
    IN HWND      hWndParent,
    IN HMENU     hMenu,
    IN HINSTANCE hInstance,
    IN PVOID     lpParam
);

typedef struct tagCREATESTRUCTA {
    PVOID    lpCreateParams;
    HINSTANCE hInstance;
    HMENU     hMenu;
    HWND      hwndParent;
    INT       cy;
    INT       cx;
    INT       y;
    INT       x;
    LONG      style;
    PSTR    lpszName;
    PSTR    lpszClass;
    DWORD     dwExStyle;
} CREATESTRUCTA, *LPCREATESTRUCTA;

typedef CREATESTRUCTA CREATESTRUCT, *LPCREATESTRUCT;

#define CreateWindowA(lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam) \
    CreateWindowExA(0, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam)

BOOL
WINAPI
GetMessageA(
    OUT PMSG lpMsg,
    IN  HWND hWnd,
    IN  UINT wMsgFilterMin,
    IN  UINT wMsgFilterMax
);

BOOL
WINAPI
TranslateMessage(
    IN CONST MSG *lpMsg
);

LRESULT
WINAPI
DispatchMessageA(
    IN CONST MSG *lpMsg
);

HDC
WINAPI
BeginPaint(
    IN HWND hWnd,
    OUT LPPAINTSTRUCT lpPaint
);

BOOL
WINAPI
EndPaint(
    IN HWND hWnd,
    IN CONST PAINTSTRUCT *lpPaint
);

BOOL
WINAPI
InvalidateRect(
    IN HWND hWnd,
    IN CONST RECT *lpRect,
    IN BOOL bErase
);

INT
WINAPI
FillRect(
    IN HDC hDC,
    IN CONST RECT *lprc,
    IN ULONG ColorARGB
);

LRESULT
WINAPI
DefWindowProcA(
    IN HWND hWnd,
    IN UINT Msg,
    IN WPARAM wParam,
    IN LPARAM lParam
);

ATOM
WINAPI
RegisterClassA(
    IN CONST WNDCLASSA *lpWndClass
);

LRESULT
WINAPI
SendMessageA(
    IN HWND   hWnd,
    IN UINT   Msg,
    IN WPARAM wParam,
    IN LPARAM lParam
);

BOOL
WINAPI
SetWindowTextA(
    IN HWND   hWnd,
    IN PSTR lpString
);

INT
WINAPI
GetWindowTextA(
    IN HWND  hWnd,
    IN PSTR lpString,
    IN INT   nMaxCount
);

HWND
WINAPI
SetFocus(
    IN HWND hWnd
);

#define DefWindowProc DefWindowProcA

typedef struct _SIZE {
    LONG cx;
    LONG cy;
} SIZE, *PSIZE, *LPSIZE;

HFONT   WINAPI SelectFont(IN HDC hdc, IN HFONT hFont);
BOOL    WINAPI DeleteFont(IN HFONT hFont);
HGDIOBJ WINAPI SelectObject(IN HDC hdc, IN HGDIOBJ h);
BOOL    WINAPI DeleteObject(IN HGDIOBJ ho);
BOOL    WINAPI GetTextExtentPoint32A(IN HDC hdc, IN PSTR lpString, IN INT c, OUT LPSIZE psizl);

/* Font Weights (Win32 wingdi.h) */
#define FW_DONTCARE         0
#define FW_THIN             100
#define FW_EXTRALIGHT       200
#define FW_LIGHT            300
#define FW_NORMAL           400
#define FW_MEDIUM           500
#define FW_SEMIBOLD         600
#define FW_BOLD             700
#define FW_EXTRABOLD        800
#define FW_HEAVY            900

/* Character Sets & Quality */
#define ANSI_CHARSET            0
#define DEFAULT_CHARSET         1
#define OUT_DEFAULT_PRECIS      0
#define OUT_TT_PRECIS           4
#define CLIP_DEFAULT_PRECIS     0
#define DEFAULT_QUALITY         0
#define ANTIALIASED_QUALITY     4
#define CLEARTYPE_QUALITY       5
#define DEFAULT_PITCH           0
#define FF_DONTCARE             (0 << 4)
#define FF_SWISS                (2 << 4)

/* DrawText() Format Flags (Win32 winuser.h) */
#define DT_TOP                  0x00000000
#define DT_LEFT                 0x00000000
#define DT_CENTER               0x00000001
#define DT_RIGHT                0x00000002
#define DT_VCENTER              0x00000004
#define DT_BOTTOM               0x00000008
#define DT_WORDBREAK            0x00000010
#define DT_SINGLELINE           0x00000020
#define DT_EXPANDTABS           0x00000040
#define DT_NOCLIP               0x00000100
#define DT_CALCRECT             0x00000400
#define DT_NOPREFIX             0x00000800

#define LF_FACESIZE             32

typedef struct _LOGFONTA {
    LONG lfHeight;
    LONG lfWidth;
    LONG lfEscapement;
    LONG lfOrientation;
    LONG lfWeight;
    BYTE lfItalic;
    BYTE lfUnderline;
    BYTE lfStrikeOut;
    BYTE lfCharSet;
    BYTE lfOutPrecision;
    BYTE lfClipPrecision;
    BYTE lfQuality;
    BYTE lfPitchAndFamily;
    CHAR lfFaceName[LF_FACESIZE];
} LOGFONTA, *PLOGFONTA, *LPLOGFONTA;

HFONT   WINAPI CreateFontIndirectA(IN CONST LOGFONTA *lplf);
HFONT   WINAPI CreateFontA(
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
);

ULONG   WINAPI SetTextColor(IN HDC hdc, IN ULONG color);
ULONG   WINAPI GetTextColor(IN HDC hdc);

INT     WINAPI DrawTextA(
    IN     HDC    hdc,
    IN     PSTR lpchText,
    IN     INT    cchText,
    IN OUT LPRECT lprc,
    IN     UINT   format
);

#define DrawText DrawTextA

VOID
WINAPI
PostQuitMessage(
    IN INT nExitCode
);

BOOL
WINAPI
DestroyWindow(
    IN HWND hWnd
);

BOOL
WINAPI
ShowWindow(
    IN HWND hWnd,
    IN INT  nCmdShow
);

#ifdef __cplusplus
}
#endif