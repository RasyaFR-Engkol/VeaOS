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
#include "userapi2.h"
#include "veapi.h"
#include "veapi2.h"
#include "veapisyscall.h"
#include <psdk/virtkey.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef USHORT ATOM;
typedef INT    BOOL;
typedef struct _SECURITY_ATTRIBUTES
{
    DWORD nLength;
    PVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

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
    IN HDC        hDC,
    IN CONST RECT *lprc,
    IN HBRUSH     hbr
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

HCURSOR WINAPI LoadCursorA(IN HINSTANCE hInstance, IN PSTR lpCursorName);
HCURSOR WINAPI SetCursor(IN HCURSOR hCursor);
HCURSOR WINAPI GetCursor(VOID);
HCURSOR WINAPI CreateCursor(
    IN HINSTANCE hInst,
    IN INT xHotSpot,
    IN INT yHotSpot,
    IN INT nWidth,
    IN INT nHeight,
    IN CONST VOID *pvANDPlane,
    IN CONST VOID *pvXORPlane
);
BOOL    WINAPI DestroyCursor(IN HCURSOR hCursor);
BOOL    WINAPI GetCursorPos(OUT LPPOINT lpPoint);
BOOL    WINAPI SetCursorPos(IN INT X, IN INT Y);
INT     WINAPI ShowCursor(IN BOOL bShow);

HWND
WINAPI
WindowFromDC(
    IN HDC hDC
);

BOOL
WINAPI
DeleteDC(
    IN HDC hdc
);

INT
WINAPI
ReleaseDC(
    IN HWND hWnd,
    IN HDC  hDC
);

HDC
WINAPI
GetWindowDC(
    IN HWND hWnd
);

HDC
WINAPI
GetDC(
    IN HWND hWnd
);

HDC
WINAPI
GetDCEx(
    IN HWND  hWnd,
    IN HRGN  hrgnClip,
    IN DWORD flags
);

HDESK
WINAPI
CreateDesktopA(
    IN PSTR                  lpszDesktop,
    IN PSTR                  lpszDevice,
    IN PVOID                 pDevmode,
    IN DWORD                 dwFlags,
    IN ACCESS_MASK           dwDesiredAccess,
    IN LPSECURITY_ATTRIBUTES lpsa
);

HDESK
WINAPI
OpenDesktopA(
    IN PSTR        lpszDesktop,
    IN DWORD       dwFlags,
    IN BOOL        fInherit,
    IN ACCESS_MASK dwDesiredAccess
);

BOOL
WINAPI
SetThreadDesktop(
    IN HDESK hDesktop
);

BOOL
WINAPI
SwitchDesktop(
    IN HDESK hDesktop
);

BOOL
WINAPI
CloseDesktop(
    IN HDESK hDesktop
);

BOOL
WINAPI
PostMessageA(
    IN HWND hWnd,
    IN UINT msg,
    IN WPARAM wParam,
    IN LPARAM lParam
);

BOOL WINAPI SetLogonNotifyWindow(IN HWND hWnd);

#define GW_HWNDFIRST        0
#define GW_HWNDLAST         1
#define GW_HWNDNEXT         2
#define GW_HWNDPREV         3
#define GW_OWNER            4
#define GW_CHILD            5
#define GW_ENABLEDPOPUP     6

#define MAX_ENUM_WINDOWS    4096

typedef BOOLEAN (CALLBACK *WNDENUMPROC)(HWND hwnd, LPARAM lParam);

BOOLEAN WINAPI EnumWindows(IN WNDENUMPROC lpEnumFunc, IN LPARAM lParam);
BOOLEAN WINAPI EnumChildWindows(IN HWND hWndParent, IN WNDENUMPROC lpEnumFunc, IN LPARAM lParam);
HWND    WINAPI GetWindow(IN HWND hWnd, IN UINT uCmd);
HWND    WINAPI GetTopWindow(IN HWND hWnd);
HWND    WINAPI GetNextWindow(IN HWND hWnd, IN UINT wCmd);
HWND    WINAPI FindWindowA(IN CONST CHAR *lpClassName, IN CONST CHAR *lpWindowName);
HWND    WINAPI FindWindowExA(IN HWND hWndParent, IN HWND hWndChildAfter, IN CONST CHAR *lpszClass, IN CONST CHAR *lpszWindow);
HWND    WINAPI GetForegroundWindow(VOID);
BOOLEAN WINAPI SetForegroundWindow(IN HWND hWnd);
INT     WINAPI GetWindowTextA(IN HWND hWnd, OUT PCHAR lpString, IN INT nMaxCount);
INT     WINAPI GetWindowTextLengthA(IN HWND hWnd);
BOOLEAN WINAPI IsWindow(IN HWND hWnd);
BOOLEAN WINAPI IsWindowVisible(IN HWND hWnd);
BOOLEAN WINAPI GetWindowRect(IN HWND hWnd, OUT PRECT lpRect);
BOOLEAN WINAPI GetClientRect(IN HWND hWnd, OUT PRECT lpRect);
BOOLEAN WINAPI ClientToScreen(IN HWND hWnd, IN OUT PPOINT lpPoint);
BOOLEAN WINAPI ScreenToClient(IN HWND hWnd, IN OUT PPOINT lpPoint);
INT     WINAPI MapWindowPoints(IN HWND hWndFrom, IN HWND hWndTo, IN OUT PPOINT lpPoints, IN UINT cPoints);
HWND    WINAPI WindowFromPoint(IN POINT Point);

typedef INT_PTR (WINAPI *FARPROC)();
typedef INT_PTR (WINAPI *NEARPROC)();
typedef INT_PTR (WINAPI *PROC)();

FARPROC
VEAPI
GetProcAddress(
    IN HMODULE hModule,
    IN PCSTR   lpProcName
);

HRSRC   VEAPI FindResourceExA(IN HMODULE hModule, IN CONST CHAR *lpType, IN CONST CHAR *lpName, IN USHORT wLanguage);
HRSRC   VEAPI FindResourceA(IN HMODULE hModule, IN CONST CHAR *lpName, IN CONST CHAR *lpType);
HGLOBAL VEAPI LoadResource(IN HMODULE hModule, IN HRSRC hResInfo);
PVOID   VEAPI LockResource(IN HGLOBAL hResData);
ULONG   VEAPI SizeofResource(IN HMODULE hModule, IN HRSRC hResInfo);
INT     VEAPI LoadStringA(IN HINSTANCE hInstance, IN UINT uID, OUT PSTR lpBuffer, IN INT cchBufferMax);

INT WINAPI GetSystemMetrics(INT nIndex);
LONG WINAPI GetWindowLongA(HWND hWnd, INT nIndex);
LONG WINAPI SetWindowLongA(HWND hWnd, INT nIndex, LONG dwNewLong);
BOOL WINAPI SetWindowPos(
    HWND hWnd,
    HWND hWndInsertAfter,
    INT  X,
    INT  Y,
    INT  cx,
    INT  cy,
    UINT uFlags
);

INT
WINAPI
FrameRect(
    IN HDC        hdc,
    IN CONST RECT *lprc,
    IN HBRUSH     hbr
);

BOOL
WINAPI
Rectangle(
    IN HDC hdc,
    IN INT nLeftRect,
    IN INT nTopRect,
    IN INT nRightRect,
    IN INT nBottomRect
);

BOOL
WINAPI
LineTo(
    IN HDC hdc,
    IN INT nXEnd,
    IN INT nYEnd
);

BOOL
WINAPI
MoveToEx(
    IN  HDC     hdc,
    IN  INT     X,
    IN  INT     Y,
    OUT LPPOINT lpPoint
);

HGDIOBJ
WINAPI
GetStockObject(
    IN INT fnObject
);

HPEN
WINAPI
CreatePen(
    IN INT   fnPenStyle,
    IN INT   nWidth,
    IN ULONG crColor
);

HBRUSH
WINAPI
CreateSolidBrush(
    IN ULONG crColor
);

DWORD
WINAPI
SetClassLongA(
    IN HWND hWnd,
    IN INT  nIndex,
    IN LONG dwNewLong
);

DWORD
WINAPI
GetClassLongA(
    IN HWND hWnd,
    IN INT  nIndex
);

BOOL WINAPI RemoveFontMemResourceEx(HANDLE h);
HANDLE WINAPI AddFontMemResourceEx(PVOID pFileView, DWORD cjSize, PVOID pvResrved, DWORD *pNumFonts);
BOOL WINAPI RemoveFontResourceExA(PSTR name, DWORD fl, PVOID res);
int WINAPI AddFontResourceExA(PSTR name, DWORD fl, PVOID res);

#define SetWindowText SetWindowTextA

UINT_PTR
WINAPI
SetTimer(
    IN HWND hWnd,
    IN UINT_PTR nIDEvent,
    IN UINT uElapse,
    IN TIMERPROC lpTimerFunc
);

BOOL
WINAPI
KillTimer(
    IN HWND hWnd,
    IN UINT_PTR uIDEvent
);

HWND
WINAPI
SetCapture(
    IN HWND hWnd
);

BOOL
WINAPI
ReleaseCapture(VOID);

BOOL
WINAPI
SetWindowPos(
    IN HWND hWnd,
    IN HWND hWndInsertAfter,
    IN INT  X,
    IN INT  Y,
    IN INT  cx,
    IN INT  cy,
    IN UINT uFlags
);

BOOL
WINAPI
MoveWindow(
    IN HWND hWnd,
    IN INT  X,
    IN INT  Y,
    IN INT  nWidth,
    IN INT  nHeight,
    IN BOOL bRepaint
);

BOOL WINAPI SetTaskmanWindow(IN HWND hWnd);
BOOL WINAPI RegisterShellHookWindow(IN HWND hWnd);
BOOL WINAPI DeregisterShellHookWindow(IN HWND hWnd);

#ifdef __cplusplus
}
#endif