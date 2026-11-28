/* VeaOS SPDX License ----------------------------------------------------
 * Auto-generated User-Mode Vz* Syscall Declarations
 * DO NOT EDIT MANUALLY
 * ----------------------------------------------------------------------- */

#ifndef _VZ_USER_H
#define _VZ_USER_H

/* kernel essential */
#include <procbind.h>
#include <ldrtypes.h>
#include <ul.h>
#include <kdp.h>
#include <mmtype.h>
#include <mm.h>
#include <rtl.h>
#include <ks.h>
#include <hct.h>
#include <obtype.h>
#include <obfunc.h>
#include <veastatus.h>
#include <tag.h>
#include <sftypes.h>
#include <sffunc.h>
#include <pttypes.h>
#include <ptfunc.h>
#include <vktypes.h>
#include <vkfunc.h>
#include <guid.h>
#include <potypes.h>
#include <pofunc.h>
#include <../psdk/veaseh.h>

#ifdef __cplusplus
extern "C" {
#endif

VEASTATUS VEAPI VzUserInitialize(VOID);
VEASTATUS VEAPI VzUserGetMessage(OUT PMESSAGE_QUEUE UserMsg, IN PVOID hWnd, IN ULONG MsgMin, IN ULONG MsgMax);
SHORT VEAPI VzUserGetAsyncKeyState(IN INT vKey);
BOOLEAN VEAPI VzUserTranslateMessage(IN const PMESSAGE_QUEUE UserMsg);
UINT VEAPI VzUserMapVirtualKeyEx(IN UINT uCode, IN UINT uType, IN ULONG dwhkl);
BOOLEAN VEAPI VzUserRegisterClass(IN const PWNDCLASS UserWndClass);
HWND VEAPI VzUserCreateWindow(IN PCHAR ClassName, IN PCHAR WindowName, IN ULONG Style);
LRESULT VEAPI VzUserDispatchMessage(IN const PMESSAGE_QUEUE UserMsg);
HDC VEAPI VzUserBeginPaint(IN HWND hWnd, OUT PPAINTSTRUCT lpPaint);
BOOLEAN VEAPI VzUserEndPaint(IN HWND hWnd, IN CONST PAINTSTRUCT *lpPaint);
BOOLEAN VEAPI VzUserInvalidateRect(IN HWND hWnd, IN CONST PRECT lpRect OPTIONAL, IN BOOLEAN bErase);
BOOLEAN VEAPI VzGdiFillRect(IN HDC hdc, IN CONST PRECT lprc, IN ULONG ColorARGB);
HWND VEAPI VzUserCreateWindowEx(IN ULONG ExStyle, IN PCHAR ClassName, IN PCHAR WindowName, IN ULONG Style, IN INT X, IN INT Y, IN INT Width, IN INT Height, IN HWND hWndParent, IN HMENU hMenu, IN HINSTANCE hInstance, IN PVOID lpParam);
BOOLEAN VEAPI VzGdiDrawImage(IN HDC hdc, IN INT DstX, IN INT DstY, IN ULONG Width, IN ULONG Height, IN CONST PULONG Pixels, IN UCHAR Alpha);
ULONG VEAPI VzGdiSetPixel(IN HDC hdc, IN INT X, IN INT Y, IN ULONG ColorARGB);
ULONG VEAPI VzGdiGetPixel(IN HDC hdc, IN INT X, IN INT Y);
BOOLEAN VEAPI VzGdiLineTo(IN HDC hdc, IN INT X1, IN INT Y1, IN INT X2, IN INT Y2, IN ULONG ColorARGB);
BOOLEAN VEAPI VzGdiInvertRect(IN HDC hdc, IN CONST PRECT lprc);
BOOLEAN VEAPI VzGdiGradientFill(IN HDC hdc, IN CONST PRECT lprc, IN ULONG ColorStartARGB, IN ULONG ColorEndARGB, IN BOOLEAN Vertical);
BOOLEAN VEAPI VzGdiBitBlt(IN HDC hdc, IN INT DstX, IN INT DstY, IN ULONG Width, IN ULONG Height, IN CONST PULONG Pixels, IN ULONG SourceWidth, IN ULONG SourceHeight, IN INT SrcX, IN INT SrcY);
BOOLEAN VEAPI VzGdiTransparentBlt(IN HDC hdc, IN INT DstX, IN INT DstY, IN ULONG Width, IN ULONG Height, IN CONST PULONG Pixels, IN ULONG SourceWidth, IN ULONG SourceHeight, IN INT SrcX, IN INT SrcY, IN ULONG TransparentColorRGB);
BOOLEAN VEAPI VzGdiAlphaBlend(IN HDC hdc, IN INT DstX, IN INT DstY, IN ULONG Width, IN ULONG Height, IN CONST PULONG Pixels, IN ULONG SourceWidth, IN ULONG SourceHeight, IN INT SrcX, IN INT SrcY, IN UCHAR Alpha);
BOOLEAN VEAPI VzGdiDrawGlyph(IN HDC hdc, IN INT X, IN INT Y, IN CONST UCHAR GlyphBitmap[16], IN ULONG TextColorARGB, IN ULONG BkColorARGB, IN BOOLEAN TransparentBk);
LRESULT VEAPI VzUserSendMessage(IN HWND hWnd, IN UINT Msg, IN WPARAM wParam, IN LPARAM lParam);
BOOLEAN VEAPI VzGdiDrawText(IN HDC hdc, IN INT X, IN INT Y, IN CONST CHAR *Text, IN ULONG ColorARGB);
BOOLEAN VEAPI VzGdiGetTextExtent(IN CONST CHAR *Text, OUT PULONG OutWidth, OUT PULONG OutHeight);
HWND VEAPI VzUserSetFocus(IN HWND hWnd);
HFONT VEAPI VzGdiCreateFont(IN INT nHeight, IN INT fnWeight, IN BOOLEAN bItalic, IN CONST CHAR *lpszFace);
HFONT VEAPI VzGdiSelectFont(IN HDC hdc, IN HFONT hFont);
BOOLEAN VEAPI VzGdiDeleteFont(IN HFONT hFont);
BOOLEAN VEAPI VzGdiGetTextExtentFont(IN HDC hdc, IN CONST CHAR *Text, OUT PULONG OutWidth, OUT PULONG OutHeight);
BOOLEAN VEAPI VzUserDestroyWindow(IN HWND hWnd);
BOOLEAN VEAPI VzUserShowWindow(IN HWND hWnd, IN INT nCmdShow);
VOID VEAPI VzUserPostQuitMessage(IN INT ExitCode);
BOOLEAN VEAPI VzUserSetCursorShape(IN ULONG Width, IN ULONG Height, IN LONG HotX, IN LONG HotY, IN CONST ULONG *PixelsARGB);
INT VEAPI VzUserShowCursor(IN BOOLEAN bShow);
BOOLEAN VEAPI VzUserGetCursorPos(OUT PPOINT lpPoint);
BOOLEAN VEAPI VzUserSetCursorPos(IN LONG X, IN LONG Y);
HWND VEAPI VzUserSetCapture(IN HWND hWnd);
BOOLEAN VEAPI VzUserReleaseCapture(VOID);
VEASTATUS VEAPI VzUserPeekMessage(OUT PMESSAGE_QUEUE UserMsg, IN PVOID hWnd, IN ULONG MsgMin, IN ULONG MsgMax, IN UINT wRemoveMsg);
BOOLEAN VEAPI VzUserPostMessage(IN HWND hWnd, IN UINT Msg, IN WPARAM wParam, IN LPARAM lParam);
HDC VEAPI VzUserGetDCEx(IN HWND hWnd, IN HANDLE hrgnClip, IN ULONG Flags);
BOOLEAN VEAPI VzGdiDeleteDC(IN HDC hdc);
HDESK VEAPI VzUserCreateDesktop(IN CONST CHAR *lpszDesktop, IN CONST CHAR *lpszDevice, IN PVOID pDevmode, IN ULONG dwFlags, IN ACCESS_MASK dwDesiredAccess, IN BOOLEAN bInheritHandle);
HDESK VEAPI VzUserOpenDesktop(IN CONST CHAR *lpszDesktop, IN ULONG dwFlags, IN BOOLEAN fInherit, IN ACCESS_MASK dwDesiredAccess);
BOOLEAN VEAPI VzUserSetThreadDesktop(IN HDESK hDesktop);
BOOLEAN VEAPI VzUserSwitchDesktop(IN HDESK hDesktop);
BOOLEAN VEAPI VzUserSetLogonNotifyWindow(IN HWND hWnd);
VEASTATUS VEAPI VzUserBuildHwndList(IN HDESK hDesktop, IN HWND hwndParent, IN BOOLEAN bChildren, IN ULONG dwThreadId, IN ULONG cHwndMax, OUT HWND *phwndFirst, OUT PULONG pcHwndNeeded);
HWND VEAPI VzUserGetWindow(IN HWND hWnd, IN UINT uCmd);
HWND VEAPI VzUserFindWindowEx(IN HWND hwndParent, IN HWND hwndChildAfter, IN CONST CHAR *lpszClass, IN CONST CHAR *lpszWindow);
HWND VEAPI VzUserGetForegroundWindow(VOID);
BOOLEAN VEAPI VzUserSetForegroundWindow(IN HWND hWnd);
INT VEAPI VzUserGetWindowText(IN HWND hWnd, OUT PCHAR lpString, IN INT nMaxCount);
BOOLEAN VEAPI VzUserSetWindowText(IN HWND hWnd, IN CONST CHAR *lpString);
BOOLEAN VEAPI VzUserIsWindow(IN HWND hWnd);
BOOLEAN VEAPI VzUserIsWindowVisible(IN HWND hWnd);
BOOLEAN VEAPI VzUserGetWindowRect(IN HWND hWnd, OUT PRECT lpRect);
BOOLEAN VEAPI VzUserGetClientRect(IN HWND hWnd, OUT PRECT lpRect);
BOOLEAN VEAPI VzUserClientToScreen(IN HWND hWnd, IN OUT PPOINT lpPoint);
BOOLEAN VEAPI VzUserScreenToClient(IN HWND hWnd, IN OUT PPOINT lpPoint);
HWND VEAPI VzUserWindowFromPoint(IN LONG ScreenX, IN LONG ScreenY);
INT VEAPI VzUserGetSystemMetrics(IN INT nIndex);
LONG VEAPI VzUserGetWindowLong(IN HWND hWnd, IN INT nIndex);
LONG VEAPI VzUserSetWindowLong(IN HWND hWnd, IN INT nIndex, IN LONG dwNewLong);
BOOLEAN VEAPI VzUserSetWindowPos(IN HWND hWnd, IN HWND hWndInsertAfter, IN INT X, IN INT Y, IN INT cx, IN INT cy, IN UINT uFlags);
HBRUSH VEAPI VzGdiCreateSolidBrush(IN ULONG crColor);
HPEN VEAPI VzGdiCreatePen(IN INT fnPenStyle, IN INT nWidth, IN ULONG crColor);
HGDIOBJ VEAPI VzGdiGetStockObject(IN INT fnObject);
HGDIOBJ VEAPI VzGdiSelectObject(IN HDC hdc, IN HGDIOBJ hgdiobj);
BOOLEAN VEAPI VzGdiDeleteObject(IN HGDIOBJ hObject);
BOOLEAN VEAPI VzGdiMoveToEx(IN HDC hdc, IN INT X, IN INT Y, OUT LPPOINT lpPoint OPTIONAL);
BOOLEAN VEAPI VzGdiLineToCurrent(IN HDC hdc, IN INT nXEnd, IN INT nYEnd);
BOOLEAN VEAPI VzGdiRectangle(IN HDC hdc, IN INT nLeftRect, IN INT nTopRect, IN INT nRightRect, IN INT nBottomRect);
BOOLEAN VEAPI VzGdiFillRectBrush(IN HDC hdc, IN CONST PRECT lprc, IN HBRUSH hbr);
BOOLEAN VEAPI VzGdiFrameRect(IN HDC hdc, IN CONST PRECT lprc, IN HBRUSH hbr);
ULONG VEAPI VzUserGetClassLong(IN HWND hWnd, IN INT nIndex);
ULONG VEAPI VzUserSetClassLong(IN HWND hWnd, IN INT nIndex, IN LONG dwNewLong);
INT VEAPI VzGdiAddFontResourceEx(IN CONST CHAR *FilePath, IN ULONG Flags, IN PVOID Reserved OPTIONAL);
BOOLEAN VEAPI VzGdiRemoveFontResourceEx(IN CONST CHAR *FilePath, IN ULONG Flags, IN PVOID Reserved OPTIONAL);
HANDLE VEAPI VzGdiAddFontMemResourceEx(IN PVOID FontData, IN ULONG FontSize, IN PVOID Reserved OPTIONAL, OUT PULONG OutNumFonts OPTIONAL);
BOOLEAN VEAPI VzGdiRemoveFontMemResourceEx(IN HANDLE hFontResource);
UINT_PTR VEAPI VzUserSetTimer(IN HWND hWnd, IN UINT_PTR nIDEvent, IN UINT uElapse, IN TIMERPROC lpTimerFunc);
BOOLEAN VEAPI VzUserKillTimer(IN HWND hWnd, IN UINT_PTR uIDEvent);
VOID VEAPI VzUserSetCaptionButtonState(IN HWND hWnd, IN ULONG HotBtn, IN ULONG PressedBtn);
BOOLEAN VEAPI VzUserSetTaskmanWindow(IN HWND hWnd);
BOOLEAN VEAPI VzUserRegisterShellHookWindow(IN HWND hWnd);
BOOLEAN VEAPI VzUserDeregisterShellHookWindow(IN HWND hWnd);
#ifdef __cplusplus
}
#endif

#endif /* _VZ_USER_H */
