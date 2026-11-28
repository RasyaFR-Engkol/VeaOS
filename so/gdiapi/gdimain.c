/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : gdimain.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : GDI Main
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "gdi.h"

/* Revision History ------------------------------------------------------
 * DATE       : 26-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file gdimain.c
 * --------------------------------------------------------------------- */

HBRUSH
WINAPI
CreateSolidBrush(
    IN ULONG crColor
)
{
    return VeaGdiCreateSolidBrush(crColor);
}

HPEN
WINAPI
CreatePen(
    IN INT   fnPenStyle,
    IN INT   nWidth,
    IN ULONG crColor
)
{
    return VeaGdiCreatePen(fnPenStyle, nWidth, crColor);
}

HGDIOBJ
WINAPI
GetStockObject(
    IN INT fnObject
)
{
    return VeaGdiGetStockObject(fnObject);
}

BOOL
WINAPI
MoveToEx(
    IN  HDC     hdc,
    IN  INT     X,
    IN  INT     Y,
    OUT LPPOINT lpPoint
)
{
    if (!hdc)
    {
        return FALSE;
    }

    return VeaGdiMoveToEx(hdc, X, Y, lpPoint) ? TRUE : FALSE;
}

BOOL
WINAPI
LineTo(
    IN HDC hdc,
    IN INT nXEnd,
    IN INT nYEnd
)
{
    if (!hdc)
    {
        return FALSE;
    }

    return VeaGdiLineToCurrent(hdc, nXEnd, nYEnd) ? TRUE : FALSE;
}

BOOL
WINAPI
Rectangle(
    IN HDC hdc,
    IN INT nLeftRect,
    IN INT nTopRect,
    IN INT nRightRect,
    IN INT nBottomRect
)
{
    if (!hdc)
    {
        return FALSE;
    }

    return VeaGdiRectangle(hdc, nLeftRect, nTopRect, nRightRect, nBottomRect) ? TRUE : FALSE;
}

INT
WINAPI
FrameRect(
    IN HDC        hdc,
    IN CONST RECT *lprc,
    IN HBRUSH     hbr
)
{
    if (!hdc || !lprc || !hbr)
    {
        return 0;
    }

    return VeaGdiFrameRect(hdc, (PRECT)lprc, hbr) ? 1 : 0;
}

int WINAPI AddFontResourceExA(PSTR name, DWORD fl, PVOID res)
{
    return (int)VeaGdiAddFontResourceEx(name, fl, res);
}

BOOL WINAPI RemoveFontResourceExA(PSTR name, DWORD fl, PVOID res)
{
    return VeaGdiRemoveFontResourceEx(name, fl, res) ? TRUE : FALSE;
}

HANDLE WINAPI AddFontMemResourceEx(PVOID pFileView, DWORD cjSize, PVOID pvResrved, DWORD *pNumFonts)
{
    return VeaGdiAddFontMemResourceEx(pFileView, cjSize, pvResrved, (PULONG)pNumFonts);
}

BOOL WINAPI RemoveFontMemResourceEx(HANDLE h)
{
    return VeaGdiRemoveFontMemResourceEx(h) ? TRUE : FALSE;
}

HFONT WINAPI CreateFontA(
    int    cHeight,
    int    cWidth,
    int    cEscapement,
    int    cOrientation,
    int    cWeight,
    DWORD  bItalic,
    DWORD  bUnderline,
    DWORD  bStrikeOut,
    DWORD  iCharSet,
    DWORD  iOutPrecision,
    DWORD  iClipPrecision,
    DWORD  iQuality,
    DWORD  iPitchAndFamily,
    PSTR pszFaceName
)
{
    LOGFONTA lf;
    RtlZeroMemory(&lf, sizeof(lf));

    lf.lfHeight         = cHeight;
    lf.lfWidth          = cWidth;
    lf.lfEscapement     = cEscapement;
    lf.lfOrientation    = cOrientation;
    lf.lfWeight         = cWeight;
    lf.lfItalic         = (BYTE)(bItalic != 0);
    lf.lfUnderline      = (BYTE)(bUnderline != 0);
    lf.lfStrikeOut      = (BYTE)(bStrikeOut != 0);
    lf.lfCharSet        = (BYTE)iCharSet;
    lf.lfOutPrecision   = (BYTE)iOutPrecision;
    lf.lfClipPrecision  = (BYTE)iClipPrecision;
    lf.lfQuality        = (BYTE)iQuality;
    lf.lfPitchAndFamily = (BYTE)iPitchAndFamily;

    if (pszFaceName)
    {
        for (int i = 0; i < LF_FACESIZE - 1 && pszFaceName[i] != '\0'; i++)
        {
            lf.lfFaceName[i] = pszFaceName[i];
        }
    }

    return CreateFontIndirectA(&lf);
}

BOOL
WINAPI
PtInRect(
    IN CONST RECT *lprc,
    IN POINT       pt
)
{
    if (!lprc)
    {
        return FALSE;
    }

    return (pt.x >= lprc->left &&
            pt.x <  lprc->right &&
            pt.y >= lprc->top &&
            pt.y <  lprc->bottom) ? TRUE : FALSE;
}

BOOL
WINAPI
DllMain(
    IN HINSTANCE hinstDLL,
    IN ULONG     fdwReason,
    IN PVOID     lpvReserved
)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            return TRUE;
        }

        case DLL_PROCESS_DETACH:
        {
            return TRUE;
        }

        case DLL_THREAD_ATTACH:
        {
            return TRUE;
        }

        case DLL_THREAD_DETACH:
        {
            return TRUE;
        }
    }

    return FALSE;
}