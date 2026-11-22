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
HFONT VEAPI VzGdiCreateFont(IN CONST CHAR *FontPath, IN ULONG PixelHeight);
HFONT VEAPI VzGdiSelectFont(IN HDC hdc, IN HFONT hFont);
BOOLEAN VEAPI VzGdiDeleteFont(IN HFONT hFont);
BOOLEAN VEAPI VzGdiGetTextExtentFont(IN HDC hdc, IN CONST CHAR *Text, OUT PULONG OutWidth, OUT PULONG OutHeight);
BOOLEAN VEAPI VzUserDestroyWindow(IN HWND hWnd);
BOOLEAN VEAPI VzUserShowWindow(IN HWND hWnd, IN INT nCmdShow);
VOID VEAPI VzUserPostQuitMessage(IN INT ExitCode);
#ifdef __cplusplus
}
#endif

#endif /* _VZ_USER_H */
