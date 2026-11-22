/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : veapi.h
   PURPOSE     : VEAPI Kernel Subsystem Header
 ------------------------------------------------------------------------- */

#ifndef _VEAPI_H_
#define _VEAPI_H_

#include <veakrnl.h>
#include <ddk/keyboard.h>
#include <psdk/virtkey.h>
#include <psdk/msg.h>
#include <psdk/winuser.h>
#include <psdk/video.h>
#include <veapi3.h>
#include <gdi.h>
#include <winsta.h>
#include <userapi2.h>
#include <psdk/veamouse.h>

// FIXME
#define MAX_INPUT_QUEUE 64
#define MAX_WND_CLASSES         32
#define MAX_WINDOWS             64

#define WINAPI __stdcall

#define WND_CLASS_MAGIC   0x534C4357  /* 'WCLS' */
#define WND_OBJECT_MAGIC  0x4F444E57  /* 'WNDO' */

#define WND_POOL_ALLOC(Size)    UlAllocatePoolWithTag(NonPagedPool, (Size), 'WNDC')
#define WND_POOL_FREE(Ptr)      UlFreePoolWithTag((Ptr), 'WNDC')

typedef struct _WND_CLASS_OBJECT {
    ULONG       Magic;
    LIST_ENTRY  ListEntry;
    CHAR      ClassName[64];
    WNDPROC   lpfnWndProc;
    ULONG     Style;
    LONG      cbClsExtra;
    LONG      cbWndExtra;
    HINSTANCE hInstance;
    HBRUSH    hbrBackground;
    BOOLEAN   Used;
} WND_CLASS_OBJECT, *PWND_CLASS_OBJECT;

typedef struct _WND_OBJECT
{
    ULONG           Magic;
    LIST_ENTRY      GlobalListEntry;
    BOOLEAN         Used;
    HWND            hWnd;
    HWND            hWndParent;      /* NULL untuk top-level window */
    HMENU           hMenu;           /* Control ID jika Style & WS_CHILD */
    HINSTANCE       hInstance;
    PVOID           lpCreateParams;
    PVOID           lpfnWndProc;
    ULONG           Style;
    ULONG           ExStyle;
    HBRUSH          hbrBackground;
    CHAR            ClassName[64];
    CHAR            Title[64];
    PDESKTOP_OBJECT Desktop;
    LIST_ENTRY      DesktopListEntry;
    PEPROCESS       OwnerProcess;
    PETHREAD        OwnerThread;
    LONG            X;
    LONG            Y;
    ULONG           Width;
    ULONG           Height;
    BOOLEAN         NeedsPaint;
} WND_OBJECT, *PWND_OBJECT;

extern HWND IptFocusWindow;

BOOLEAN
VEAPI
VeaUserRegisterClass(
    IN const PWNDCLASS UserWndClass
);

HWND
VEAPI
VeaUserCreateWindow(
    IN PCHAR ClassName,
    IN PCHAR WindowName,
    IN ULONG Style
);

LRESULT
VEAPI
VeaUserDispatchMessage(
    IN const PMESSAGE_QUEUE UserMsg
);

VOID
VEAPI
IptUserRawInputThread(
    IN PVOID StartContext
);

VOID
VEAPI
UserProcessKeyPacket(
    IN PKEYBOARD_INPUT_DATA KeyData
);

VOID
VEAPI
MsgInitializeInputQueue(VOID);

VOID
VEAPI
MsgPostInputMessage(
    IN ULONG Message,
    IN ULONG_PTR wParam,
    IN LONG_PTR lParam
);

/* Syscall handlers (Shadow SSDT slot 1) */
VEASTATUS VEAPI VeaUserInitialize(VOID);

SHORT
VEAPI
VeaUserGetAsyncKeyState(
    IN INT vKey
);

VEASTATUS
VEAPI
VeaUserGetMessage(
    OUT PMESSAGE_QUEUE UserMsg,
    IN PVOID hWnd,
    IN ULONG MsgMin,
    IN ULONG MsgMax
);

UINT
VEAPI
VeaUserMapVirtualKeyEx(
    IN UINT uCode,
    IN UINT uType,
    IN ULONG dwhkl
);

BOOLEAN
VEAPI
VeaUserTranslateMessage(
    IN const PMESSAGE_QUEUE UserMsg
);

extern MESSAGE_QUEUE MsgMessageQueue[MAX_INPUT_QUEUE];
extern UCHAR IptAsyncKeyState[256];
extern BOOLEAN UserSubsystemInitialized;
extern GDI_DC_OBJECT gDcTable[MAX_GDI_DCS];
extern KEVENT MsgInputEvent;

VEASTATUS
WINAPI
InitVideo(VOID);

VEASTATUS
WINAPI
UserInitialize(VOID);

VEASTATUS
VEAPI
UserThreadCallout(
    IN PETHREAD Thread,
    IN VEAPI_THREAD_CALLOUT_TYPE Type
);

VEASTATUS
VEAPI
UserProcessCallout(
    IN PEPROCESS Process,
    IN BOOLEAN Create
);

VOID
VEAPI
WndWinStaDelete(
    IN PVOID Object
);

LONG
VEAPI
WndWinStaParse(
    IN PVOID ParseObject,
    IN POB_LOOKUP_CONTEXT LookupContext,
    IN PVOID Context,
    OUT PVOID *ResolvedObject
);

BOOLEAN
VEAPI
WndWinStaOkToClose(
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN HANDLE Handle,
    IN KPROCESSOR_MODE PreviousMode
);

VOID
VEAPI
DskDesktopDelete(
    IN PVOID Object
);

VEASTATUS
VEAPI
DskDesktopOpen(
    IN OB_OPEN_REASON OpenReason,
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG HandleCount
);

VOID
VEAPI
DskDesktopClose(
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG ProcessHandleCount,
    IN ULONG SystemHandleCount
);

BOOLEAN
VEAPI
DskDesktopOkToClose(
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN HANDLE Handle,
    IN KPROCESSOR_MODE PreviousMode
);

VEASTATUS
VEAPI
UserInitializeWindowStations(VOID);

VOID
VEAPI
WndCleanupProcessWindows(
    IN PEPROCESS Process
);

HWND
VEAPI
WndFindPendingPaintWindow(
    IN PETHREAD Thread
);

HDC
VEAPI
VeaUserBeginPaint(
    IN HWND hWnd,
    OUT PPAINTSTRUCT lpPaint
);

BOOLEAN
VEAPI
VeaUserEndPaint(
    IN HWND hWnd,
    IN CONST PAINTSTRUCT *lpPaint
);

BOOLEAN
VEAPI
VeaUserInvalidateRect(
    IN HWND hWnd,
    IN CONST PRECT lpRect OPTIONAL,
    IN BOOLEAN bErase
);

BOOLEAN
VEAPI
VeaUserUpdateWindow(
    IN HWND hWnd
);

BOOLEAN
VEAPI
VeaUserGetClientRect(
    IN HWND hWnd,
    OUT PRECT lpRect
);

PWND_OBJECT
WndLookupHandle(
    IN HWND hWnd
);

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
);

LRESULT
VEAPI
VeaUserSendMessage(
    IN HWND   hWnd,
    IN UINT   Msg,
    IN WPARAM wParam,
    IN LPARAM lParam
);

VOID
VEAPI
UserInvalidateAllWindows(VOID);

HWND
VEAPI
VeaUserSetFocus(
    IN HWND hWnd
);

VOID
VEAPI
WndInvalidateChildWindows(
    IN HWND hWndParent
);

VOID
WndEnsureListsInitialized(VOID);

extern KSPIN_LOCK   gWindowListLock;
extern LIST_ENTRY   gWindowListHead;

VOID
VEAPI
WndAdoptOrphanWindowsToDesktop(
    IN PDESKTOP_OBJECT Desktop
);

BOOLEAN
VEAPI
VeaUserDestroyWindow(
    IN HWND hWnd
);

BOOLEAN
VEAPI
VeaUserShowWindow(
    IN HWND hWnd,
    IN INT  nCmdShow
);

VOID
VEAPI
VeaUserPostQuitMessage(
    IN INT ExitCode
);

VOID
VEAPI
IptMouseRawInputThread(
    IN PVOID StartContext
);

extern LONG gCursorX;
extern LONG gCursorY;
extern HWND gCaptureWindow;

VOID
MsgEnqueueMessage(
    IN HWND      hWnd,
    IN ULONG     Message,
    IN ULONG_PTR wParam,
    IN LONG_PTR  lParam,
    IN BOOLEAN   ForceIfFull
);

VOID
WndGetClientOriginOnScreen(
    IN PWND_OBJECT Wnd,
    OUT PLONG ScreenX,
    OUT PLONG ScreenY
);

VOID
VEAPI
MsgPostMouseMessage(
    IN HWND      hWnd,
    IN ULONG     Message,
    IN ULONG_PTR wParam,
    IN LONG_PTR  lParam
);

extern ULONG  gMouseButtonState;

VOID
VEAPI
UserProcessMousePacket(
    IN PMOUSE_INPUT_DATA Mou
);

#endif