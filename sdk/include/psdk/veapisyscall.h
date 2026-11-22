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
    IN CONST CHAR *FontPath,
    IN ULONG PixelHeight
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


/* EOF */