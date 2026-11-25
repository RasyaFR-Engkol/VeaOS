/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : gdiobj.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : GDI Object Management
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 24-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file gdiobj.c
 * --------------------------------------------------------------------- */

static GDI_BRUSH_OBJECT gStockBrushes[6];
static GDI_PEN_OBJECT   gStockPens[3];
static BOOLEAN          gStockInitialized = FALSE;

VOID
VEAPI
GdipInitializeGdiObjects(VOID)
{
    if (gStockInitialized)
    {
        return;
    }

    RtlZeroMemory(gStockBrushes, sizeof(gStockBrushes));
    RtlZeroMemory(gStockPens, sizeof(gStockPens));

    ULONG BrushColors[6] = {
        0xFFFFFFFF, /* WHITE_BRUSH */
        0xFFC0C0C0, /* LTGRAY_BRUSH */
        0xFF808080, /* GRAY_BRUSH */
        0xFF404040, /* DKGRAY_BRUSH */
        0xFF000000, /* BLACK_BRUSH */
        0x00000000  /* NULL_BRUSH */
    };

    for (ULONG i = 0; i < 6; i++)
    {
        gStockBrushes[i].Magic       = GDI_BRUSH_MAGIC;
        gStockBrushes[i].Style       = (i == NULL_BRUSH) ? BS_NULL : BS_SOLID;
        gStockBrushes[i].ColorARGB   = BrushColors[i];
        gStockBrushes[i].StockObject = TRUE;
    }

    /* WHITE_PEN */
    gStockPens[0].Magic       = GDI_PEN_MAGIC;
    gStockPens[0].Style       = PS_SOLID;
    gStockPens[0].Width       = 1;
    gStockPens[0].ColorARGB   = 0xFFFFFFFF;
    gStockPens[0].StockObject = TRUE;

    /* BLACK_PEN */
    gStockPens[1].Magic       = GDI_PEN_MAGIC;
    gStockPens[1].Style       = PS_SOLID;
    gStockPens[1].Width       = 1;
    gStockPens[1].ColorARGB   = 0xFF000000;
    gStockPens[1].StockObject = TRUE;

    /* NULL_PEN */
    gStockPens[2].Magic       = GDI_PEN_MAGIC;
    gStockPens[2].Style       = PS_NULL;
    gStockPens[2].Width       = 0;
    gStockPens[2].ColorARGB   = 0x00000000;
    gStockPens[2].StockObject = TRUE;

    gStockInitialized = TRUE;
}

HGDIOBJ
VEAPI
VeaGdiGetStockObject(
    IN INT fnObject
)
{
    if (!gStockInitialized)
    {
        GdipInitializeGdiObjects();
    }

    if (fnObject >= WHITE_BRUSH && fnObject <= NULL_BRUSH)
    {
        return (HGDIOBJ)&gStockBrushes[fnObject];
    }

    if (fnObject >= WHITE_PEN && fnObject <= NULL_PEN)
    {
        return (HGDIOBJ)&gStockPens[fnObject - WHITE_PEN];
    }

    return NULL;
}

HBRUSH
VEAPI
VeaGdiCreateSolidBrush(
    IN ULONG crColor
)
{
    PGDI_BRUSH_OBJECT Brush = (PGDI_BRUSH_OBJECT)GDI_POOL_ALLOC(sizeof(GDI_BRUSH_OBJECT));
    if (!Brush)
    {
        return NULL;
    }

    Brush->Magic       = GDI_BRUSH_MAGIC;
    Brush->Style       = BS_SOLID;
    Brush->ColorARGB   = GdipNormalizeArgb(crColor);
    Brush->StockObject = FALSE;

    return (HBRUSH)Brush;
}

HPEN
VEAPI
VeaGdiCreatePen(
    IN INT   fnPenStyle,
    IN INT   nWidth,
    IN ULONG crColor
)
{
    PGDI_PEN_OBJECT Pen = (PGDI_PEN_OBJECT)GDI_POOL_ALLOC(sizeof(GDI_PEN_OBJECT));
    if (!Pen)
    {
        return NULL;
    }

    Pen->Magic       = GDI_PEN_MAGIC;
    Pen->Style       = (fnPenStyle == PS_NULL) ? PS_NULL : PS_SOLID;
    Pen->Width       = (nWidth > 0) ? (ULONG)nWidth : 1;
    Pen->ColorARGB   = GdipNormalizeArgb(crColor);
    Pen->StockObject = FALSE;

    return (HPEN)Pen;
}

BOOLEAN
VEAPI
GdipResolveBrushColor(
    IN  HBRUSH hbr,
    OUT PULONG OutColorARGB
)
{
    if (!hbr || !OutColorARGB)
    {
        return FALSE;
    }

    ULONG_PTR Raw = (ULONG_PTR)hbr;

    /* Tangani konstanta Win32 (HBRUSH)(COLOR_* + 1) bernilai 1..31 */
    if (Raw <= 31)
    {
        ULONG SysIdx = (ULONG)(Raw - 1);
        switch (SysIdx)
        {
            case COLOR_BACKGROUND:
                *OutColorARGB = 0xFF004E98;
                return TRUE;

            case COLOR_WINDOW:
                *OutColorARGB = 0xFFFFFFFF;
                return TRUE;

            case COLOR_BTNFACE:
            case COLOR_MENU:
                *OutColorARGB = 0xFFECE9D8;
                return TRUE;

            case COLOR_BTNSHADOW:
                *OutColorARGB = 0xFFACA899;
                return TRUE;

            case COLOR_WINDOWTEXT:
            case COLOR_BTNTEXT:
                *OutColorARGB = 0xFF000000;
                return TRUE;

            case COLOR_HIGHLIGHT:
                *OutColorARGB = 0xFF316AC5;
                return TRUE;

            default:
                *OutColorARGB = 0xFFFFFFFF;
                return TRUE;
        }
    }

    if (MI_IS_USER_ADDRESS(Raw))
    {
        return FALSE;
    }

    PGDI_BRUSH_OBJECT Brush = (PGDI_BRUSH_OBJECT)hbr;
    if (Brush->Magic != GDI_BRUSH_MAGIC || Brush->Style == BS_NULL)
    {
        return FALSE;
    }

    *OutColorARGB = Brush->ColorARGB;
    return TRUE;
}

BOOLEAN
VEAPI
GdipResolvePenInfo(
    IN  HPEN   hpen,
    OUT PULONG OutColorARGB,
    OUT PULONG OutWidth
)
{
    if (!hpen || !OutColorARGB || !OutWidth)
    {
        return FALSE;
    }

    ULONG_PTR Raw = (ULONG_PTR)hpen;
    if (MI_IS_USER_ADDRESS(Raw))
    {
        return FALSE;
    }

    PGDI_PEN_OBJECT Pen = (PGDI_PEN_OBJECT)hpen;
    if (Pen->Magic != GDI_PEN_MAGIC || Pen->Style == PS_NULL || Pen->Width == 0)
    {
        return FALSE;
    }

    *OutColorARGB = Pen->ColorARGB;
    *OutWidth     = Pen->Width;
    return TRUE;
}

HGDIOBJ
VEAPI
VeaGdiSelectObject(
    IN HDC     hdc,
    IN HGDIOBJ hgdiobj
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    if (!Dc || !hgdiobj || MI_IS_USER_ADDRESS((ULONG_PTR)hgdiobj))
    {
        return NULL;
    }

    ULONG Magic = *(PULONG)hgdiobj;

    if (Magic == GDI_BRUSH_MAGIC)
    {
        HGDIOBJ Old = (HGDIOBJ)Dc->hBrush;
        Dc->hBrush  = (HBRUSH)hgdiobj;
        return Old;
    }

    if (Magic == GDI_PEN_MAGIC)
    {
        HGDIOBJ Old = (HGDIOBJ)Dc->hPen;
        Dc->hPen    = (HPEN)hgdiobj;
        return Old;
    }

    HGDIOBJ OldFont = (HGDIOBJ)Dc->hFont;
    Dc->hFont       = (HFONT)hgdiobj;
    return OldFont;
}

BOOLEAN
VEAPI
VeaGdiDeleteObject(
    IN HGDIOBJ hObject
)
{
    if (!hObject || MI_IS_USER_ADDRESS((ULONG_PTR)hObject))
    {
        return FALSE;
    }

    ULONG Magic = *(PULONG)hObject;

    if (Magic == GDI_BRUSH_MAGIC)
    {
        PGDI_BRUSH_OBJECT Brush = (PGDI_BRUSH_OBJECT)hObject;
        if (Brush->StockObject)
        {
            /* Sesuai spek MSDN: DeleteObject pada stock object aman & return TRUE */
            return TRUE;
        }
        Brush->Magic = 0;
        GDI_POOL_FREE(Brush);
        return TRUE;
    }

    if (Magic == GDI_PEN_MAGIC)
    {
        PGDI_PEN_OBJECT Pen = (PGDI_PEN_OBJECT)hObject;
        if (Pen->StockObject)
        {
            return TRUE;
        }
        Pen->Magic = 0;
        GDI_POOL_FREE(Pen);
        return TRUE;
    }

    /* Jika bukan brush/pen, teruskan ke penghapus font lo (misal GdipDeleteFont) */
    return FALSE;
}

/* EOF */