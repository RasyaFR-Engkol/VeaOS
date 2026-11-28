/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : gdiobj.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : GDI Object
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "gdi.h"

/* Revision History ------------------------------------------------------
 * DATE       : 26-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file gdiobj.c
 * --------------------------------------------------------------------- */

typedef struct _GDI_DC_ATTR {
    LIST_ENTRY ListEntry;
    HDC        hDC;
    ULONG      TextColor;
    ULONG      BkColor;
    INT        BkMode;
} GDI_DC_ATTR, *PGDI_DC_ATTR;

static LIST_ENTRY gGdiDcAttrListHead;
static BOOLEAN    gGdiDcAttrInitialized = FALSE;

static
VOID
GdipEnsureDcAttrList(VOID)
{
    if (!gGdiDcAttrInitialized)
    {
        InitializeListHead(&gGdiDcAttrListHead);
        gGdiDcAttrInitialized = TRUE;
    }
}

static
PGDI_DC_ATTR
GdipFindDcAttr(
    IN HDC hDC
)
{
    PLIST_ENTRY Entry;

    if (!hDC || !gGdiDcAttrInitialized)
    {
        return NULL;
    }

    for (Entry = gGdiDcAttrListHead.Flink;
         Entry != &gGdiDcAttrListHead;
         Entry = Entry->Flink)
    {
        PGDI_DC_ATTR Attr = CONTAINING_RECORD(Entry, GDI_DC_ATTR, ListEntry);
        if (Attr->hDC == hDC)
        {
            return Attr;
        }
    }

    return NULL;
}

static
PGDI_DC_ATTR
GdipGetOrCreateDcAttr(
    IN HDC hDC
)
{
    PGDI_DC_ATTR Attr;

    if (!hDC)
    {
        return NULL;
    }

    GdipEnsureDcAttrList();

    Attr = GdipFindDcAttr(hDC);
    if (Attr)
    {
        return Attr;
    }

    Attr = (PGDI_DC_ATTR)RtlAllocateHeap(RtlGetProcessHeap(), 0, sizeof(GDI_DC_ATTR));
    if (!Attr)
    {
        return NULL;
    }

    RtlZeroMemory(Attr, sizeof(GDI_DC_ATTR));
    Attr->hDC       = hDC;
    Attr->TextColor = 0xFF000000;
    Attr->BkColor   = 0xFFFFFFFF;
    Attr->BkMode    = OPAQUE;
    InsertTailList(&gGdiDcAttrListHead, &Attr->ListEntry);

    return Attr;
}

VOID
VEAPI
GdiInitDcAttr(
    IN HDC hdc
)
{
    PGDI_DC_ATTR Attr = GdipGetOrCreateDcAttr(hdc);
    if (Attr)
    {
        Attr->TextColor = 0xFF000000;
        Attr->BkColor   = 0xFFFFFFFF;
        Attr->BkMode    = OPAQUE;
    }
}

VOID
VEAPI
GdiResetDcAttr(
    IN HDC hdc
)
{
    PGDI_DC_ATTR Attr = GdipFindDcAttr(hdc);
    if (Attr)
    {
        RemoveEntryList(&Attr->ListEntry);
        RtlFreeHeap(RtlGetProcessHeap(), 0, Attr);
    }
}

BOOL
WINAPI
DeleteDC(
    IN HDC hdc
)
{
    if (!hdc)
    {
        return FALSE;
    }

    GdiResetDcAttr(hdc);
    return VeaGdiDeleteDC(hdc) ? TRUE : FALSE;
}

HFONT
WINAPI
CreateFontIndirectA(
    IN CONST LOGFONTA *lplf
)
{
    if (!lplf)
    {
        return NULL;
    }

    return VeaGdiCreateFont(
        lplf->lfHeight,
        lplf->lfWeight,
        (lplf->lfItalic != 0),
        (lplf->lfFaceName[0] != '\0') ? lplf->lfFaceName : NULL
    );
}

HFONT
WINAPI
SelectFont(
    IN HDC   hdc,
    IN HFONT hFont
)
{
    if (!hdc)
    {
        return NULL;
    }

    /* hFont == NULL diizinkan untuk reset kembali ke font default sistem */
    return VeaGdiSelectFont(hdc, hFont);
}

BOOL
WINAPI
DeleteFont(
    IN HFONT hFont
)
{
    if (!hFont)
    {
        return FALSE;
    }

    return VeaGdiDeleteFont(hFont) ? TRUE : FALSE;
}

HGDIOBJ
WINAPI
SelectObject(
    IN HDC     hdc,
    IN HGDIOBJ h
)
{
    if (!hdc)
    {
        return NULL;
    }

    /* h == NULL diizinkan untuk reset font ke default sistem */
    if (h == NULL)
    {
        return (HGDIOBJ)VeaGdiSelectFont(hdc, NULL);
    }

    return VeaGdiSelectObject(hdc, h);
}

BOOL
WINAPI
DeleteObject(
    IN HGDIOBJ ho
)
{
    if (!ho)
    {
        return FALSE;
    }

    if (VeaGdiDeleteObject(ho))
    {
        return TRUE;
    }

    return VeaGdiDeleteFont((HFONT)ho) ? TRUE : FALSE;
}

ULONG
WINAPI
SetTextColor(
    IN HDC   hdc,
    IN ULONG color
)
{
    if (!hdc)
    {
        return 0xFFFFFFFF;
    }

    ULONG Argb = ((color & 0xFF000000) == 0) ? (0xFF000000 | color) : color;
    PGDI_DC_ATTR Attr = GdipGetOrCreateDcAttr(hdc);
    if (!Attr)
    {
        return 0xFF000000;
    }

    ULONG Prev = Attr->TextColor;
    Attr->TextColor = Argb;
    return Prev;
}

ULONG
WINAPI
GetTextColor(
    IN HDC hdc
)
{
    PGDI_DC_ATTR Attr = GdipFindDcAttr(hdc);
    return Attr ? Attr->TextColor : 0xFF000000;
}

ULONG
WINAPI
SetBkColor(
    IN HDC   hdc,
    IN ULONG color
)
{
    if (!hdc)
    {
        return 0xFFFFFFFF;
    }

    ULONG Argb = ((color & 0xFF000000) == 0) ? (0xFF000000 | color) : color;
    PGDI_DC_ATTR Attr = GdipGetOrCreateDcAttr(hdc);
    if (!Attr)
    {
        return 0xFFFFFFFF;
    }

    ULONG Prev = Attr->BkColor;
    Attr->BkColor = Argb;
    return Prev;
}

ULONG
WINAPI
GetBkColor(
    IN HDC hdc
)
{
    PGDI_DC_ATTR Attr = GdipFindDcAttr(hdc);
    return Attr ? Attr->BkColor : 0xFFFFFFFF;
}

INT
WINAPI
SetBkMode(
    IN HDC hdc,
    IN INT mode
)
{
    PGDI_DC_ATTR Attr = GdipGetOrCreateDcAttr(hdc);
    if (!Attr)
    {
        return 0;
    }

    INT Prev = Attr->BkMode;
    Attr->BkMode = mode;
    return Prev;
}

INT
WINAPI
GetBkMode(
    IN HDC hdc
)
{
    PGDI_DC_ATTR Attr = GdipFindDcAttr(hdc);
    return Attr ? Attr->BkMode : OPAQUE;
}

BOOL
WINAPI
GetTextExtentPoint32A(
    IN  HDC    hdc,
    IN  PSTR   lpString,
    IN  INT    c,
    OUT LPSIZE psizl
)
{
    CHAR  LocalBuf[256];
    ULONG Len    = 0;
    ULONG Width  = 0;
    ULONG Height = 0;

    if (!hdc || !lpString || !psizl || c < 0)
    {
        return FALSE;
    }

    if (c == 0)
    {
        psizl->cx = 0;
        psizl->cy = 0;
        return TRUE;
    }

    ULONG Limit = (ULONG)c;
    if (Limit >= sizeof(LocalBuf))
    {
        Limit = sizeof(LocalBuf) - 1;
    }

    while (Len < Limit && lpString[Len] != '\0')
    {
        LocalBuf[Len] = lpString[Len];
        Len++;
    }
    LocalBuf[Len] = '\0';

    if (!VeaGdiGetTextExtentFont(hdc, LocalBuf, &Width, &Height))
    {
        return FALSE;
    }

    psizl->cx = (LONG)Width;
    psizl->cy = (LONG)Height;
    return TRUE;
}

BOOL
WINAPI
TextOutA(
    IN HDC  hdc,
    IN INT  x,
    IN INT  y,
    IN PSTR lpString,
    IN INT  c
)
{
    CHAR  LocalBuf[256];
    ULONG Len = 0;

    if (!hdc || !lpString || c < 0)
    {
        return FALSE;
    }

    if (c == 0)
    {
        return TRUE;
    }

    ULONG Limit = (ULONG)c;
    if (Limit >= sizeof(LocalBuf))
    {
        Limit = sizeof(LocalBuf) - 1;
    }

    while (Len < Limit && lpString[Len] != '\0')
    {
        LocalBuf[Len] = lpString[Len];
        Len++;
    }
    LocalBuf[Len] = '\0';

    ULONG Color = GetTextColor(hdc);
    return VeaGdiDrawText(hdc, x, y, LocalBuf, Color) ? TRUE : FALSE;
}

/* EOF */