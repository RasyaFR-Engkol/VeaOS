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
    PEPROCESS OwnerProcess;
} WND_CLASS_OBJECT, *PWND_CLASS_OBJECT;

typedef struct _WND_OBJECT
{
    ULONG           Magic;
    LIST_ENTRY      GlobalListEntry;
    volatile LONG   RefCount;
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
    PULONG          ShadowBackup;
    ULONG           ShadowBackupCapacity;
    LONG            LastShadowX;
    LONG            LastShadowY;
    ULONG           LastShadowW;
    ULONG           LastShadowH;
    BOOLEAN         HasShadowDrawn;
    LONG            NormalX;
    LONG            NormalY;
    ULONG           NormalWidth;
    ULONG           NormalHeight;
    ULONG_PTR       UserData;
} WND_OBJECT, *PWND_OBJECT;

extern HWND IptFocusWindow;

typedef struct _QUEUED_MESSAGE {
    LIST_ENTRY    ListEntry;
    MESSAGE_QUEUE Msg;
} QUEUED_MESSAGE, *PQUEUED_MESSAGE;

struct _ETHREAD;

typedef struct _USER_MESSAGE_QUEUE {
    LIST_ENTRY    ThreadListEntry;
    struct _ETHREAD *Thread;
    volatile LONG RefCount;
    BOOLEAN       Destroyed;
    KSPIN_LOCK    Lock;
    KEVENT        WakeEvent;        /* NotificationEvent (manual-reset) */
    LIST_ENTRY    PostListHead;     /* Antrean PostMessage & WM_CHAR */
    LIST_ENTRY    InputListHead;    /* Antrean Hardware Input (Mouse & Keyboard) */
    LIST_ENTRY    SentListHead;
    ULONG         MessageCount;
    ULONG         WakeBits;         /* Bitmask pesan yang sedang tersedia (QS_*) */
    BOOLEAN       QuitPosted;
    INT           QuitExitCode;
} USER_MESSAGE_QUEUE, *PUSER_MESSAGE_QUEUE;

#include <psdk/w32thread.h>

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

extern UCHAR IptAsyncKeyState[256];
extern BOOLEAN UserSubsystemInitialized;

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

VOID
VEAPI
WndCleanupProcessClasses(
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

BOOLEAN
VEAPI
VeaUserSetCursorShape(
    IN ULONG        Width,
    IN ULONG        Height,
    IN LONG         HotX,
    IN LONG         HotY,
    IN CONST ULONG *PixelsARGB
);

INT
VEAPI
VeaUserShowCursor(
    IN BOOLEAN bShow
);

BOOLEAN
VEAPI
VeaUserGetCursorPos(
    OUT PPOINT lpPoint
);

BOOLEAN
VEAPI
VeaUserSetCursorPos(
    IN LONG X,
    IN LONG Y
);

HWND
VEAPI
VeaUserSetCapture(
    IN HWND hWnd
);

BOOLEAN
VEAPI
VeaUserReleaseCapture(VOID);

PWND_OBJECT
WndLookupHandleLocked(
    IN HWND hWnd
);

VOID
VEAPI
MsgDestroyThreadQueue(
    IN PETHREAD Thread
);

VOID
WndDereference(
    IN PWND_OBJECT Wnd
);

BOOLEAN
VEAPI
MsgPostToWindow(
    IN HWND      hWnd,
    IN ULONG     Message,
    IN ULONG_PTR wParam,
    IN LONG_PTR  lParam,
    IN BOOLEAN   IsInput
);

VOID
VEAPI
MsgNotifyPaint(
    IN PETHREAD TargetThread
);

VOID
VEAPI
MsgNotifyPaintAll(VOID);

PUSER_MESSAGE_QUEUE
MsgGetThreadQueue(
    IN PETHREAD Thread,
    IN BOOLEAN  CreateIfMissing
);

VEASTATUS
VEAPI
VeaUserPeekMessage(
    OUT PMESSAGE_QUEUE UserMsg,
    IN  PVOID          hWnd,
    IN  ULONG          MsgMin,
    IN  ULONG          MsgMax,
    IN  UINT           wRemoveMsg
);

BOOLEAN
VEAPI
VeaUserPostMessage(
    IN HWND   hWnd,
    IN UINT   Msg,
    IN WPARAM wParam,
    IN LPARAM lParam
);

PVOID
VEAPI
PtGetCurrentThreadVeapiThread(VOID);

HDESK
VEAPI
VeaUserCreateDesktop(
    IN CONST CHAR  *lpszDesktop,
    IN CONST CHAR  *lpszDevice,
    IN PVOID        pDevmode,
    IN ULONG        dwFlags,
    IN ACCESS_MASK  dwDesiredAccess,
    IN BOOLEAN      bInheritHandle
);

HDESK
VEAPI
VeaUserOpenDesktop(
    IN CONST CHAR  *lpszDesktop,
    IN ULONG        dwFlags,
    IN BOOLEAN      fInherit,
    IN ACCESS_MASK  dwDesiredAccess
);

BOOLEAN
VEAPI
VeaUserSetThreadDesktop(
    IN HDESK hDesktop
);

BOOLEAN
VEAPI
VeaUserSwitchDesktop(
    IN HDESK hDesktop
);

BOOLEAN
VEAPI
VeaUserSetLogonNotifyWindow(
    IN HWND hWnd
);

extern HWND      GpLogonNotifyWindow;

VOID
WndGetClippedClientRect(
    IN  PWND_OBJECT Wnd,
    OUT PLONG OriginX,
    OUT PLONG OriginY,
    OUT PLONG ClipLeft,
    OUT PLONG ClipTop,
    OUT PLONG ClipRight,
    OUT PLONG ClipBottom
);

BOOLEAN
VEAPI
GdipGetTextExtentFontInternal(
    IN  HDC         hdc,
    IN  CONST CHAR *Text,
    OUT PULONG      OutWidth,
    OUT PULONG      OutHeight,
    IN  BOOLEAN     FromUser
);

BOOLEAN
VEAPI
GdipDrawTextInternal(
    IN HDC         hdc,
    IN INT         X,
    IN INT         Y,
    IN CONST CHAR *Text,
    IN ULONG       ColorARGB,
    IN BOOLEAN     FromUser
);

VOID
VEAPI
EngFillDesktopRect(
    IN LONG  X,
    IN LONG  Y,
    IN ULONG Width,
    IN ULONG Height,
    IN ULONG ColorARGB
);

ULONG
WndGetDesktopBgColor(
    IN PDESKTOP_OBJECT Desktop
);

typedef struct _SENT_MESSAGE
{
    LIST_ENTRY ListEntry;
    HWND       hWnd;
    ULONG      Message;
    ULONG_PTR  wParam;
    LONG_PTR   lParam;
    LRESULT    Result;
    KEVENT     ReplyEvent;
    LONG       RefCount;         /* 2 = dipegang sender + target queue (anti use-after-free) */
    BOOLEAN    Completed;
} SENT_MESSAGE, *PSENT_MESSAGE;

LRESULT
MsgSendMessageToThread(
    IN PETHREAD  TargetThread,
    IN HWND      hWnd,
    IN ULONG     Message,
    IN ULONG_PTR wParam,
    IN LONG_PTR  lParam
);

LRESULT
WndInvokeUserProc(
    IN PWND_OBJECT Wnd,
    IN ULONG Msg,
    IN WPARAM wParam,
    IN LPARAM lParam
);

LRESULT
MsgSendMessageToThread(
    IN PETHREAD  TargetThread,
    IN HWND      hWnd,
    IN ULONG     Message,
    IN ULONG_PTR wParam,
    IN LONG_PTR  lParam
);

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
);

HWND
VEAPI
VeaUserGetWindow(
    IN HWND hWnd,
    IN UINT uCmd
);

HWND
VEAPI
VeaUserFindWindowEx(
    IN HWND        hwndParent,
    IN HWND        hwndChildAfter,
    IN CONST CHAR *lpszClass,
    IN CONST CHAR *lpszWindow
);

HWND
VEAPI
VeaUserGetForegroundWindow(VOID);

BOOLEAN
VEAPI
VeaUserSetForegroundWindow(
    IN HWND hWnd
);

INT
VEAPI
VeaUserGetWindowText(
    IN  HWND  hWnd,
    OUT PCHAR lpString,
    IN  INT   nMaxCount
);

BOOLEAN
VEAPI
VeaUserSetWindowText(
    IN HWND        hWnd,
    IN CONST CHAR *lpString
);

BOOLEAN
VEAPI
VeaUserIsWindow(
    IN HWND hWnd
);

BOOLEAN
VEAPI
VeaUserIsWindowVisible(
    IN HWND hWnd
);

BOOLEAN
VEAPI
VeaUserGetWindowRect(
    IN  HWND  hWnd,
    OUT PRECT lpRect
);

BOOLEAN
VEAPI
VeaUserGetClientRect(
    IN  HWND  hWnd,
    OUT PRECT lpRect
);

BOOLEAN
VEAPI
VeaUserClientToScreen(
    IN     HWND   hWnd,
    IN OUT PPOINT lpPoint
);

BOOLEAN
VEAPI
VeaUserScreenToClient(
    IN     HWND   hWnd,
    IN OUT PPOINT lpPoint
);

HWND
VEAPI
VeaUserWindowFromPoint(
    IN LONG ScreenX,
    IN LONG ScreenY
);

PDESKTOP_OBJECT
VEAPI
DskEnsureThreadDesktop(
    IN PTHREADINFO pti
);

BOOLEAN
VEAPI
WndIsEffectivelyVisible(
    IN PWND_OBJECT Wnd
);

BOOLEAN
VEAPI
WndIsEffectivelyVisibleLocked(
    IN PWND_OBJECT Wnd
);

#define MAX_VISIBLE_CLIP_RECTS 64

ULONG
VEAPI
WndComputeVisibleRects(
    IN  PWND_OBJECT Wnd,
    IN  CONST RECT *ClipBounds,
    OUT PRECT       OutRects,
    IN  ULONG       MaxRects
);

VOID
WndDrawTitleBarText(
    IN PWND_OBJECT Wnd,
    IN LONG TitleX,
    IN LONG TitleY,
    IN ULONG TitleW,
    IN ULONG TitleH
);

VOID
WndFillClippedFrameRect(
    IN PVEAGDI_SURFACE Target,
    IN CONST RECT     *VisRects,
    IN ULONG           VisCount,
    IN LONG            X,
    IN LONG            Y,
    IN ULONG           Width,
    IN ULONG           Height,
    IN ULONG           ColorARGB
);

VOID
WndDrawLunaWindowFrame(
    IN PVEAGDI_SURFACE Target,
    IN PWND_OBJECT     Wnd,
    IN CONST RECT     *FrameVis,
    IN ULONG           FrameVisCount,
    IN LONG            SnapX,
    IN LONG            SnapY,
    IN ULONG           SnapW,
    IN ULONG           SnapH,
    IN BOOLEAN         IsActive
);

VOID
VEAPI
WndDrawWindowBottomShadow(
    IN PWND_OBJECT Wnd,
    IN BOOLEAN     PresentToScreen
);

VOID
VEAPI
WndRestoreWindowBottomShadow(
    IN PWND_OBJECT Wnd,
    IN BOOLEAN     PresentToScreen
);

VOID
WndReference(
    IN PWND_OBJECT Wnd
);

#define CAPTION_BTN_NONE    0
#define CAPTION_BTN_MIN     1
#define CAPTION_BTN_MAX     2
#define CAPTION_BTN_CLOSE   3

#define CAPTION_STATE_NORMAL   0
#define CAPTION_STATE_HOVER    1
#define CAPTION_STATE_PRESSED  2

ULONG
WndHitTestCaptionButton(
    IN PWND_OBJECT Wnd,
    IN LONG        ScreenX,
    IN LONG        ScreenY
);

VOID
WndRedrawCaptionButtons(
    IN PWND_OBJECT Wnd
);

ULONG
WndGetCaptionButtonVisualState(
    IN HWND  hWnd,
    IN ULONG BtnId
);

ULONG
WndLerpColorARGB(
    IN ULONG C1,
    IN ULONG C2,
    IN LONG  Num,
    IN LONG  Denom
);

INT
VEAPI
VeaUserGetSystemMetrics(
    IN INT nIndex
);

LONG
VEAPI
VeaUserGetWindowLong(
    IN HWND hWnd,
    IN INT nIndex
);

LONG
VEAPI
VeaUserSetWindowLong(
    IN HWND hWnd,
    IN INT  nIndex,
    IN LONG dwNewLong
);

BOOLEAN
VEAPI
VeaUserSetWindowPos(
    IN HWND hWnd,
    IN HWND hWndInsertAfter,
    IN INT  X,
    IN INT  Y,
    IN INT  cx,
    IN INT  cy,
    IN UINT uFlags
);

VOID
WndBringToFront(
    IN PWND_OBJECT TopWnd
);

ULONG
VEAPI
VeaUserSetClassLong(
    IN HWND  hWnd,
    IN INT   nIndex,
    IN LONG  dwNewLong
);

ULONG
VEAPI
VeaUserGetClassLong(
    IN HWND hWnd,
    IN INT  nIndex
);

#endif