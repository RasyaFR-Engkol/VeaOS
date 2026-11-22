#pragma once

#define CALLBACK VEAPI

#ifndef DEFINE_HANDLE
    #define DEFINE_HANDLE(x) typedef struct x##__ { int unused; } *x
#endif

/* =========================================================================
 * Module & Memory Handles
 * ========================================================================= */
DEFINE_HANDLE(HINSTANCE);
typedef HINSTANCE HMODULE;      /* Di Win32, HMODULE & HINSTANCE identik */
DEFINE_HANDLE(HGLOBAL);
DEFINE_HANDLE(HLOCAL);
DEFINE_HANDLE(HRSRC);

/* =========================================================================
 * USER Handles (Window, Menu, Input, WindowStation/Desktop)
 * ========================================================================= */
DEFINE_HANDLE(HWND);
DEFINE_HANDLE(HMENU);
DEFINE_HANDLE(HICON);
typedef HICON HCURSOR;          /* HCURSOR & HICON berbagi tipe dasar di Win32 */
DEFINE_HANDLE(HACCEL);
DEFINE_HANDLE(HHOOK);
DEFINE_HANDLE(HDWP);
DEFINE_HANDLE(HMONITOR);
DEFINE_HANDLE(HWINSTA);
DEFINE_HANDLE(HDESK);

/* =========================================================================
 * GDI Handles (veagdi)
 * ========================================================================= */
typedef PVOID HGDIOBJ;          /* Base handle untuk SelectObject / DeleteObject */

DEFINE_HANDLE(HDC);
DEFINE_HANDLE(HBITMAP);
DEFINE_HANDLE(HBRUSH);
DEFINE_HANDLE(HPEN);
DEFINE_HANDLE(HFONT);
DEFINE_HANDLE(HRGN);
DEFINE_HANDLE(HPALETTE);
DEFINE_HANDLE(HCOLORSPACE);
DEFINE_HANDLE(HMETAFILE);
DEFINE_HANDLE(HENHMETAFILE);

typedef LONG_PTR LRESULT;
typedef ULONG_PTR WPARAM;
typedef LONG_PTR  LPARAM;

typedef LRESULT (CALLBACK *WNDPROC)(
    HWND      hWnd,
    UINT     Msg,
    ULONG_PTR wParam,
    LONG_PTR  lParam
);

#define WA_INACTIVE             0

#define WM_NULL                 0x0000
#define WM_CREATE               0x0001
#define WM_DESTROY              0x0002
#define WM_MOVE                 0x0003
#define WM_SIZE                 0x0005
#define WM_ACTIVATE             0x0006
#define WM_SETFOCUS             0x0007
#define WM_KILLFOCUS            0x0008
#define WM_PAINT                0x000F
#define WM_CLOSE                0x0010
#define WM_QUIT                 0x0012
#define WM_ERASEBKGND           0x0014
#define WM_NCDESTROY            0x0082
#define WM_KEYDOWN              0x0100
#define WM_KEYUP                0x0101
#define WM_CHAR                 0x0102
#define WM_DEADCHAR             0x0103
#define WM_SYSKEYDOWN           0x0104
#define WM_SYSKEYUP             0x0105
#define WM_SYSCHAR              0x0106
#define WM_SYSDEADCHAR          0x0107
#define WM_SYSCOMMAND           0x0112

#define WS_OVERLAPPED           0x00000000L
#define WS_POPUP                0x80000000L
#define WS_CHILD                0x40000000L
#define WS_VISIBLE              0x10000000L
#define WS_DISABLED             0x08000000L
#define WS_CAPTION              0x00C00000L
#define WS_BORDER               0x00800000L
#define WS_SYSMENU              0x00080000L

#define CS_VREDRAW          0x0001
#define CS_HREDRAW          0x0002
#define CS_DBLCLKS          0x0008
#define CS_OWNDC            0x0020
#define CS_CLASSDC          0x0040
#define CS_PARENTDC         0x0080
#define CS_NOCLOSE          0x0200
#define CS_SAVEBITS         0x0800
#define CS_BYTEALIGNCLIENT  0x1000
#define CS_BYTEALIGNWINDOW  0x2000
#define CS_GLOBALCLASS      0x4000

typedef struct _WNDCLASS {
    ULONG     Style;
    WNDPROC   lpfnWndProc;
    LONG      cbClsExtra;
    LONG      cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    PCHAR     lpszMenuName;
    PCHAR     lpszClassName;
} WNDCLASS, *PWNDCLASS, *LPWNDCLASS;

typedef struct tagWNDCLASSEXA {
    UINT      cbSize;
    UINT      style;
    WNDPROC   lpfnWndProc;
    INT       cbClsExtra;
    INT       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    PSTR    lpszMenuName;
    PSTR    lpszClassName;
    HICON     hIconSm;
} WNDCLASSEXA, *PWNDCLASSEXA, *NPWNDCLASSEXA, *LPWNDCLASSEXA;
typedef WNDCLASSEXA WNDCLASSEX, *PWNDCLASSEX, *NPWNDCLASSEX, *LPWNDCLASSEX;

#define USER_CALLBACK_WNDPROC   0

typedef struct _WNDPROC_CALLBACK_ARGS {
    WNDPROC   lpfnWndProc;      /* +0  */
    HWND      hWnd;             /* +4  */
    ULONG     Msg;              /* +8  */
    ULONG_PTR wParam;           /* +12 */
    LONG_PTR  lParam;           /* +16 */
} WNDPROC_CALLBACK_ARGS, *PWNDPROC_CALLBACK_ARGS;

typedef struct tagCREATESTRUCTA
{
    PVOID     lpCreateParams;
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
    ULONG     dwExStyle;
} CREATESTRUCTA, *PCREATESTRUCTA, *LPCREATESTRUCTA;

/* Window Styles tambahan */
#define WS_CHILD            0x40000000L
#define WS_VISIBLE          0x10000000L
#define WS_DISABLED         0x08000000L
#define WS_BORDER           0x00800000L
#define WS_TABSTOP          0x00010000L
#define WS_EX_CLIENTEDGE    0x00000200L

/* Static (Label) Styles */
#define SS_LEFT             0x00000000L
#define SS_CENTER           0x00000001L
#define SS_RIGHT            0x00000002L

/* Edit (TextBox) Styles */
#define ES_LEFT             0x0000L
#define ES_CENTER           0x0001L
#define ES_RIGHT            0x0002L
#define ES_PASSWORD         0x0020L
#define ES_AUTOHSCROLL      0x0080L
#define ES_READONLY         0x0800L

/* Button Styles & Notifications */
#define BS_PUSHBUTTON       0x00000000L
#define BS_DEFPUSHBUTTON    0x00000001L
#define BN_CLICKED          0

/* Messages & Helper Macros */
#define WM_COMMAND          0x0111
#define WM_SETTEXT          0x000C
#define WM_GETTEXT          0x000D
#define WM_GETTEXTLENGTH    0x000E
#define WM_LBUTTONDOWN      0x0201
#define WM_LBUTTONUP        0x0202

/* SW */
#define SW_HIDE         0
#define SW_SHOWNORMAL   1
#define SW_SHOW         5

/* SC */
#define SC_CLOSE        0xF060

#define LOWORD(l)           ((USHORT)((ULONG_PTR)(l) & 0xFFFF))
#define HIWORD(l)           ((USHORT)(((ULONG_PTR)(l) >> 16) & 0xFFFF))
#define MAKEWPARAM(l, h)    ((WPARAM)(((USHORT)(l)) | ((ULONG)((USHORT)(h)) << 16)))

/* EOF */