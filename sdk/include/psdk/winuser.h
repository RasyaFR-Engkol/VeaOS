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
#define WM_KEYFIRST 256
#define WM_UNICHAR 0x0109
#define WM_KEYLAST WM_UNICHAR
#define WM_APP 0x8000
#define WM_SAS  0x03B0
#define WM_CTLCOLORMSGBOX 306
#define WM_CTLCOLOREDIT 307
#define WM_CTLCOLORLISTBOX 308
#define WM_CTLCOLORBTN 309
#define WM_CTLCOLORDLG 310
#define WM_CTLCOLORSCROLLBAR 311
#define WM_CTLCOLORSTATIC 312
#define MN_GETHMENU 481
#define WM_MOUSEFIRST 512

#define WS_OVERLAPPED           0x00000000L
#define WS_POPUP                0x80000000L
#define WS_CHILD                0x40000000L
#define WS_THICKFRAME           0x00040000L
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
#define SW_HIDE             0
#define SW_SHOWNORMAL       1
#define SW_NORMAL           1
#define SW_SHOWMINIMIZED    2
#define SW_SHOWMAXIMIZED    3
#define SW_MAXIMIZE         3
#define SW_SHOW             5
#define SW_MINIMIZE         6
#define SW_RESTORE          9

/* SC */
#define SC_SIZE 0xF000
#define SC_MOVE 0xF010
#define SC_MINIMIZE 0xF020
#define SC_ICON 0xf020
#define SC_MAXIMIZE 0xF030
#define SC_ZOOM 0xF030
#define SC_NEXTWINDOW 0xF040
#define SC_PREVWINDOW 0xF050
#define SC_CLOSE 0xF060
#define SC_VSCROLL 0xF070
#define SC_HSCROLL 0xF080
#define SC_MOUSEMENU 0xF090
#define SC_KEYMENU 0xF100
#define SC_ARRANGE 0xF110
#define SC_RESTORE 0xF120
#define SC_TASKLIST 0xF130
#define SC_SCREENSAVE 0xF140
#define SC_HOTKEY 0xF150
#define SC_DEFAULT 0xF160
#define SC_MONITORPOWER 0xF170
#define SC_CONTEXTHELP 0xF180
#define SC_SEPARATOR 0xF00F

#define LOWORD(l)           ((USHORT)((ULONG_PTR)(l) & 0xFFFF))
#define HIWORD(l)           ((USHORT)(((ULONG_PTR)(l) >> 16) & 0xFFFF))
#define MAKEWPARAM(l, h)    ((WPARAM)(((USHORT)(l)) | ((ULONG)((USHORT)(h)) << 16)))

#ifndef MAKEINTRESOURCEA
#define MAKEINTRESOURCEA(i) ((PSTR)((ULONG_PTR)((USHORT)(i))))
#endif

#ifndef IS_INTRESOURCE
#define IS_INTRESOURCE(r)   ((((ULONG_PTR)(r)) >> 16) == 0)
#endif

#define IDC_ARROW           MAKEINTRESOURCEA(32512)
#define IDC_IBEAM           MAKEINTRESOURCEA(32513)
#define IDC_WAIT            MAKEINTRESOURCEA(32514)
#define IDC_CROSS           MAKEINTRESOURCEA(32515)
#define IDC_UPARROW         MAKEINTRESOURCEA(32516)
#define IDC_SIZENWSE        MAKEINTRESOURCEA(32642)
#define IDC_SIZENESW        MAKEINTRESOURCEA(32643)
#define IDC_SIZEWE          MAKEINTRESOURCEA(32644)
#define IDC_SIZENS          MAKEINTRESOURCEA(32645)
#define IDC_SIZEALL         MAKEINTRESOURCEA(32646)
#define IDC_NO              MAKEINTRESOURCEA(32648)
#define IDC_HAND            MAKEINTRESOURCEA(32649)
#define IDC_APPSTARTING     MAKEINTRESOURCEA(32650)
#define IDC_HELP            MAKEINTRESOURCEA(32651)

#define MAX_THREAD_MESSAGES     4096

/* Wake bitmask standar Win32k */
#define QS_KEY              0x0001
#define QS_MOUSEMOVE        0x0002
#define QS_MOUSEBUTTON      0x0004
#define QS_POSTMESSAGE      0x0008
#define QS_TIMER            0x0010
#define QS_PAINT            0x0020
#define QS_SENDMESSAGE      0x0040
#define QS_HOTKEY           0x0080
#define QS_POSTQUIT         0x0100

#define QS_MOUSE            (QS_MOUSEMOVE | QS_MOUSEBUTTON)
#define QS_INPUT            (QS_MOUSE | QS_KEY)
#define QS_ALLEVENTS        (QS_INPUT | QS_POSTMESSAGE | QS_TIMER | QS_PAINT | QS_HOTKEY)
#define QS_ALLINPUT         (QS_ALLEVENTS | QS_SENDMESSAGE | QS_POSTQUIT)

#define PM_NOREMOVE 0
#define PM_REMOVE 1
#define PM_NOYIELD 2

#define DCX_WINDOW              0x00000001UL
#define DCX_CACHE               0x00000002UL
#define DCX_NORESETATTRS        0x00000004UL
#define DCX_CLIPCHILDREN        0x00000008UL
#define DCX_CLIPSIBLINGS        0x00000010UL
#define DCX_PARENTCLIP          0x00000020UL
#define DCX_EXCLUDERGN          0x00000040UL
#define DCX_INTERSECTRGN        0x00000080UL
#define DCX_EXCLUDEUPDATE       0x00000100UL
#define DCX_INTERSECTUPDATE     0x00000200UL
#define DCX_LOCKWINDOWUPDATE    0x00000400UL
#define DCX_USESTYLE            0x00010000UL
#define DCX_VALIDATE            0x00200000UL
#define DCX_INTERNAL_RELEASE    0x80000000UL

#define GW_HWNDFIRST        0
#define GW_HWNDLAST         1
#define GW_HWNDNEXT         2
#define GW_HWNDPREV         3
#define GW_OWNER            4
#define GW_CHILD            5
#define GW_ENABLEDPOPUP     6

#define HTNOWHERE           0
#define HTCLIENT            1
#define HTCAPTION           2
#define HTMINBUTTON         8
#define HTMAXBUTTON         9
#define HTCLOSE             20

#define WS_MAXIMIZEBOX      0x00010000L
#define WS_MINIMIZEBOX      0x00020000L
#define WS_MAXIMIZE         0x01000000L
#define WS_MINIMIZE         0x20000000L

/* GetSystemMetrics index */
#ifndef SM_CXSCREEN
#define SM_CXSCREEN         0
#define SM_CYSCREEN         1
#define SM_CXBORDER         5
#define SM_CYBORDER         6
#define SM_CYCAPTION        4
#define SM_CXFULLSCREEN     16
#define SM_CYFULLSCREEN     17
#endif

/* GetWindowLong / SetWindowLong index */
#ifndef GWL_WNDPROC
#define GWL_WNDPROC         (-4)
#define GWL_HINSTANCE       (-6)
#define GWL_HWNDPARENT      (-8)
#define GWL_STYLE           (-16)
#define GWL_EXSTYLE         (-20)
#define GWL_USERDATA        (-21)
#define GWL_ID              (-12)
#endif

/* GetClassLong / SetClassLong Indices */
#ifndef GCL_MENUNAME
#define GCL_MENUNAME        (-8)
#define GCL_HBRBACKGROUND   (-10)
#define GCL_HCURSOR         (-12)
#define GCL_HICON           (-14)
#define GCL_HMODULE         (-16)
#define GCL_CBWNDEXTRA      (-18)
#define GCL_CBCLSEXTRA      (-20)
#define GCL_WNDPROC         (-24)
#define GCL_STYLE           (-26)
#define GCW_ATOM            (-32)
#define GCL_HICONSM         (-34)
#endif

/* SetWindowPos flags & insert-after handles */
#ifndef SWP_NOSIZE
#define SWP_NOSIZE          0x0001
#define SWP_NOMOVE          0x0002
#define SWP_NOZORDER        0x0004
#define SWP_NOREDRAW        0x0008
#define SWP_NOACTIVATE      0x0010
#define SWP_FRAMECHANGED    0x0020
#define SWP_SHOWWINDOW      0x0040
#define SWP_HIDEWINDOW      0x0080
#define SWP_NOOWNERZORDER   0x0200
#endif

#ifndef HWND_TOP
#define HWND_TOP            ((HWND)0)
#define HWND_BOTTOM         ((HWND)1)
#define HWND_TOPMOST        ((HWND)-1)
#define HWND_NOTOPMOST      ((HWND)-2)
#endif

#define SIZE_RESTORED       0
#define SIZE_MINIMIZED      1
#define SIZE_MAXIMIZED      2

/* Brush Styles */
#define BS_SOLID                0
#define BS_NULL                 1
#define BS_HOLLOW               BS_NULL

/* Pen Styles */
#define PS_SOLID                0
#define PS_DASH                 1
#define PS_DOT                  2
#define PS_NULL                 5

/* Stock Objects (Win32 Standard) */
#define WHITE_BRUSH             0
#define LTGRAY_BRUSH            1
#define GRAY_BRUSH              2
#define DKGRAY_BRUSH            3
#define BLACK_BRUSH             4
#define NULL_BRUSH              5
#define HOLLOW_BRUSH            NULL_BRUSH
#define WHITE_PEN               6
#define BLACK_PEN               7
#define NULL_PEN                8
#define STOCK_OBJECT_MAX        9

/* System Color Indices (untuk (HBRUSH)(COLOR_* + 1)) */
#define COLOR_SCROLLBAR         0
#define COLOR_BACKGROUND        1
#define COLOR_ACTIVECAPTION     2
#define COLOR_INACTIVECAPTION   3
#define COLOR_MENU              4
#define COLOR_WINDOW            5
#define COLOR_WINDOWFRAME       6
#define COLOR_MENUTEXT          7
#define COLOR_WINDOWTEXT        8
#define COLOR_CAPTIONTEXT       9
#define COLOR_ACTIVEBORDER      10
#define COLOR_INACTIVEBORDER    11
#define COLOR_APPWORKSPACE      12
#define COLOR_HIGHLIGHT         13
#define COLOR_HIGHLIGHTTEXT     14
#define COLOR_BTNFACE           15
#define COLOR_BTNSHADOW         16
#define COLOR_GRAYTEXT          17
#define COLOR_BTNTEXT           18
#define COLOR_3DFACE            COLOR_BTNFACE
#define COLOR_DESKTOP           COLOR_BACKGROUND

#define FR_PRIVATE  0x10
#define FR_NOT_ENUM 0x20

#define FW_DONTCARE	0
#define FW_THIN	100
#define FW_EXTRALIGHT	200
#define FW_ULTRALIGHT	FW_EXTRALIGHT
#define FW_LIGHT	300
#define FW_NORMAL	400
#define FW_REGULAR	400
#define FW_MEDIUM	500
#define FW_SEMIBOLD	600
#define FW_DEMIBOLD	FW_SEMIBOLD
#define FW_BOLD	700
#define FW_EXTRABOLD	800
#define FW_ULTRABOLD	FW_EXTRABOLD
#define FW_HEAVY	900
#define FW_BLACK	FW_HEAVY

/* EOF */