#pragma once

VOID
VEAPI
GdiInitDcAttr(
    IN HDC hdc
);

VOID
VEAPI
GdiResetDcAttr(
    IN HDC hdc
);

BOOL
WINAPI
DeleteDC(
    IN HDC hdc
);

HFONT
WINAPI
CreateFontIndirectA(
    IN CONST LOGFONTA *lplf
);

BOOL
WINAPI
TextOutA(
    IN HDC  hdc,
    IN INT  x,
    IN INT  y,
    IN PSTR lpString,
    IN INT  c
);

/* Background Modes (SetBkMode / GetBkMode) */
#define TRANSPARENT             1
#define OPAQUE                  2
#define BKMODE_LAST             2

/* Error Return untuk SetTextColor / SetBkColor / GetPixel */
#define CLR_INVALID             0xFFFFFFFF

/* Brush & Hatch Styles (CreateBrushIndirect / CreateHatchBrush) */
#define BS_SOLID                0
#define BS_NULL                 1
#define BS_HOLLOW               BS_NULL
#define BS_HATCHED              2
#define BS_PATTERN              3

#define HS_HORIZONTAL           0       /* ----- */
#define HS_VERTICAL             1       /* ||||| */
#define HS_FDIAGONAL            2       /* \\\\\ */
#define HS_BDIAGONAL            3       /* ///// */
#define HS_CROSS                4       /* +++++ */
#define HS_DIAGCROSS            5       /* xxxxx */

/* Stock Logical Objects (GetStockObject) */
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
#define OEM_FIXED_FONT          10
#define ANSI_FIXED_FONT         11
#define ANSI_VAR_FONT           12
#define SYSTEM_FONT             13
#define DEVICE_DEFAULT_FONT     14
#define DEFAULT_PALETTE         15
#define SYSTEM_FIXED_FONT       16
#define DEFAULT_GUI_FONT        17
#define DC_BRUSH                18
#define DC_PEN                  19

/* Binary Raster Ops (SetROP2 / GetROP2) */
#define R2_BLACK                1       /*  0       */
#define R2_NOTMERGEPEN          2       /* DPon     */
#define R2_MASKNOTPEN           3       /* DPna     */
#define R2_NOTCOPYPEN           4       /* PN       */
#define R2_MASKPENNOT           5       /* PDna     */
#define R2_NOT                  6       /* Dn       */
#define R2_XORPEN               7       /* DPx      */
#define R2_NOTMASKPEN           8       /* DPan     */
#define R2_MASKPEN              9       /* DPa      */
#define R2_NOTXORPEN            10      /* DPxn     */
#define R2_NOP                  11      /* D        */
#define R2_MERGENOTPEN          12      /* DPno     */
#define R2_COPYPEN              13      /* P        */
#define R2_MERGEPENNOT          14      /* PDno     */
#define R2_MERGEPEN             15      /* DPo      */
#define R2_WHITE                16      /*  1       */

/* Ternary Raster Ops (BitBlt / StretchBlt) */
#define SRCCOPY                 (DWORD)0x00CC0020 /* dest = source               */
#define SRCPAINT                (DWORD)0x00EE0086 /* dest = source OR dest       */
#define SRCAND                  (DWORD)0x008800C6 /* dest = source AND dest      */
#define SRCINVERT               (DWORD)0x00660046 /* dest = source XOR dest      */
#define SRCERASE                (DWORD)0x00440328 /* dest = source AND (NOT dest)*/
#define NOTSRCCOPY              (DWORD)0x00330008 /* dest = (NOT source)         */
#define NOTSRCERASE             (DWORD)0x001100A6 /* dest = (NOT src) AND (NOT dst) */
#define MERGECOPY               (DWORD)0x00C000CA /* dest = (source AND pattern) */
#define MERGEPAINT              (DWORD)0x00BB0226 /* dest = (NOT source) OR dest */
#define PATCOPY                 (DWORD)0x00F00021 /* dest = pattern              */
#define PATPAINT                (DWORD)0x00FB0A09 /* dest = DPSnoo               */
#define PATINVERT               (DWORD)0x005A0049 /* dest = pattern XOR dest     */
#define DSTINVERT               (DWORD)0x00550009 /* dest = (NOT dest)           */
#define BLACKNESS               (DWORD)0x00000042 /* dest = BLACK                */
#define WHITENESS               (DWORD)0x00FF0062 /* dest = WHITE                */

INT
WINAPI
SetBkMode(
    IN HDC hdc,
    IN INT mode
);

BOOL
WINAPI
PtInRect(
    IN CONST RECT *lprc,
    IN POINT       pt
);

/* EOF */