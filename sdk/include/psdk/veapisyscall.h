#pragma once

BOOLEAN
VEAPI
VeaUserRegisterClass(
    IN CONST PWNDCLASS UserWndClass
);

HWND
VEAPI
VeaUserCreateWindow(
    IN PCHAR ClassName,
    IN PCHAR WindowName,
    IN ULONG Style
);

VEASTATUS
VEAPI
VeaUserGetMessage(
    OUT PMESSAGE_QUEUE UserMsg,
    IN PVOID hWnd,
    IN ULONG MsgMin,
    IN ULONG MsgMax
);

BOOLEAN
VEAPI
VeaUserTranslateMessage(
    IN const PMESSAGE_QUEUE UserMsg
);

LRESULT
VEAPI
VeaUserDispatchMessage(
    IN CONST PMESSAGE_QUEUE UserMsg
);

HDC     VEAPI VeaUserBeginPaint(IN HWND hWnd, OUT PPAINTSTRUCT lpPaint);
BOOLEAN VEAPI VeaUserEndPaint(IN HWND hWnd, IN CONST PAINTSTRUCT *lpPaint);
BOOLEAN VEAPI VeaUserInvalidateRect(IN HWND hWnd, IN CONST PRECT lpRect, IN BOOLEAN bErase);
BOOLEAN VEAPI VeaGdiFillRect(IN HDC hdc, IN CONST PRECT lprc, IN ULONG ColorARGB);
ULONG VEAPI VeaGdiSetPixel(IN HDC hdc, IN INT X, IN INT Y, IN ULONG ColorARGB);
ULONG VEAPI VeaGdiGetPixel(IN HDC hdc, IN INT X, IN INT Y);
BOOLEAN VEAPI VeaGdiLineTo(IN HDC hdc, IN INT X1, IN INT Y1, IN INT X2, IN INT Y2, IN ULONG ColorARGB);
BOOLEAN VEAPI VeaGdiInvertRect(IN HDC hdc, IN CONST PRECT lprc);
BOOLEAN VEAPI VeaGdiGradientFill(IN HDC hdc, IN CONST PRECT lprc, IN ULONG ColorStartARGB, IN ULONG ColorEndARGB, IN BOOLEAN Vertical);
BOOLEAN VEAPI VeaGdiBitBlt(IN HDC hdc, IN INT DstX, IN INT DstY, IN ULONG Width, IN ULONG Height, IN CONST PULONG Pixels, IN ULONG SourceWidth, IN ULONG SourceHeight, IN INT SrcX, IN INT SrcY);
BOOLEAN VEAPI VeaGdiTransparentBlt(IN HDC hdc, IN INT DstX, IN INT DstY, IN ULONG Width, IN ULONG Height, IN CONST PULONG Pixels, IN ULONG SourceWidth, IN ULONG SourceHeight, IN INT SrcX, IN INT SrcY, IN ULONG TransparentColorRGB);
BOOLEAN VEAPI VeaGdiAlphaBlend(IN HDC hdc, IN INT DstX, IN INT DstY, IN ULONG Width, IN ULONG Height, IN CONST PULONG Pixels, IN ULONG SourceWidth, IN ULONG SourceHeight, IN INT SrcX, IN INT SrcY, IN UCHAR Alpha);
BOOLEAN VEAPI VeaGdiDrawGlyph(IN HDC hdc, IN INT X, IN INT Y, IN CONST UCHAR GlyphBitmap[16], IN ULONG TextColorARGB, IN ULONG BkColorARGB, IN BOOLEAN TransparentBk);

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

BOOLEAN
VEAPI
VeaGdiDrawImage(
    IN HDC          hdc,
    IN INT          DstX,
    IN INT          DstY,
    IN ULONG        Width,
    IN ULONG        Height,
    IN CONST PULONG Pixels,
    IN UCHAR        Alpha
);

LRESULT
VEAPI
VeaUserSendMessage(
    IN HWND   hWnd,
    IN UINT   Msg,
    IN WPARAM wParam,
    IN LPARAM lParam
);

BOOLEAN
VEAPI
VeaGdiDrawText(
    IN HDC hdc,
    IN INT X,
    IN INT Y,
    IN CONST CHAR *Text,
    IN ULONG ColorARGB
);

BOOLEAN
VEAPI
VeaGdiGetTextExtent(
    IN CONST CHAR *Text,
    OUT PULONG OutWidth,
    OUT PULONG OutHeight
);

HWND
VEAPI
VeaUserSetFocus(
    IN HWND hWnd
);

HFONT
VEAPI
VeaGdiCreateFont(
    IN INT         nHeight,
    IN INT         fnWeight,
    IN BOOLEAN     bItalic,
    IN CONST CHAR *lpszFace
);

HFONT
VEAPI
VeaGdiSelectFont(
    IN HDC hdc,
    IN HFONT hFont
);

BOOLEAN
VEAPI
VeaGdiDeleteFont(
    IN HFONT hFont
);

BOOLEAN
VEAPI
VeaGdiGetTextExtentFont(
    IN HDC hdc,
    IN CONST CHAR *Text,
    OUT PULONG OutWidth,
    OUT PULONG OutHeight
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

HDC
VEAPI
VeaUserGetDCEx(
    IN HWND   hWnd,
    IN HANDLE hrgnClip,
    IN ULONG  Flags
);

BOOLEAN
VEAPI
VeaGdiDeleteDC(
    IN HDC hdc
);

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

HBRUSH
VEAPI
VeaGdiCreateSolidBrush(
    IN ULONG crColor
);

HPEN
VEAPI
VeaGdiCreatePen(
    IN INT   fnPenStyle,
    IN INT   nWidth,
    IN ULONG crColor
);

HGDIOBJ
VEAPI
VeaGdiGetStockObject(
    IN INT fnObject
);

HGDIOBJ
VEAPI
VeaGdiSelectObject(
    IN HDC     hdc,
    IN HGDIOBJ hgdiobj
);

BOOLEAN
VEAPI
VeaGdiDeleteObject(
    IN HGDIOBJ hObject
);

BOOLEAN
VEAPI
VeaGdiMoveToEx(
    IN  HDC     hdc,
    IN  INT     X,
    IN  INT     Y,
    OUT LPPOINT lpPoint OPTIONAL
);

BOOLEAN
VEAPI
VeaGdiLineToCurrent(
    IN HDC hdc,
    IN INT nXEnd,
    IN INT nYEnd
);

BOOLEAN
VEAPI
VeaGdiRectangle(
    IN HDC hdc,
    IN INT nLeftRect,
    IN INT nTopRect,
    IN INT nRightRect,
    IN INT nBottomRect
);

BOOLEAN
VEAPI
VeaGdiFillRectBrush(
    IN HDC         hdc,
    IN CONST PRECT lprc,
    IN HBRUSH      hbr
);

BOOLEAN
VEAPI
VeaGdiFrameRect(
    IN HDC         hdc,
    IN CONST PRECT lprc,
    IN HBRUSH      hbr
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

INT
VEAPI
VeaGdiAddFontResourceEx(
    IN CONST CHAR *FilePath,
    IN ULONG       Flags,
    IN PVOID       Reserved OPTIONAL
);

BOOLEAN
VEAPI
VeaGdiRemoveFontResourceEx(
    IN CONST CHAR *FilePath,
    IN ULONG       Flags,
    IN PVOID       Reserved OPTIONAL
);

HANDLE
VEAPI
VeaGdiAddFontMemResourceEx(
    IN  PVOID  FontData,
    IN  ULONG  FontSize,
    IN  PVOID  Reserved OPTIONAL,
    OUT PULONG OutNumFonts OPTIONAL
);

BOOLEAN
VEAPI
VeaGdiRemoveFontMemResourceEx(
    IN HANDLE hFontResource
);

UINT_PTR
VEAPI
VeaUserSetTimer(
    IN HWND      hWnd,
    IN UINT_PTR  nIDEvent,
    IN UINT      uElapse,
    IN TIMERPROC lpTimerFunc
);

BOOLEAN
VEAPI
VeaUserKillTimer(
    IN HWND     hWnd,
    IN UINT_PTR uIDEvent
);

VOID
VEAPI
VeaUserSetCaptionButtonState(
    IN HWND  hWnd,
    IN ULONG HotBtn,
    IN ULONG PressedBtn
);

BOOLEAN
VEAPI
VeaUserSetTaskmanWindow(
    IN HWND hWnd
);

BOOLEAN
VEAPI
VeaUserRegisterShellHookWindow(
    IN HWND hWnd
);

BOOLEAN
VEAPI
VeaUserDeregisterShellHookWindow(
    IN HWND hWnd
);

/* EOF */