/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : gdiengine.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : GDI Engine
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file gdiengine.c
 * --------------------------------------------------------------------- */

static LIST_ENTRY       gDcListHead;
static KSPIN_LOCK       gDcListLock;
static PGDI_DC_OBJECT   gScreenDc = NULL;

typedef enum _VEA_GDI_BLIT_KIND
{
    VeaGdiBlitCopy,
    VeaGdiBlitTransparent,
    VeaGdiBlitAlpha
} VEA_GDI_BLIT_KIND;

PGDI_DC_OBJECT
VEAPI
GdipAllocateDc(VOID)
{
    KIRQL OldIrql;
    PGDI_DC_OBJECT Dc = (PGDI_DC_OBJECT)GDI_POOL_ALLOC(sizeof(GDI_DC_OBJECT));
    if (!Dc)
    {
        return NULL;
    }

    RtlZeroMemory(Dc, sizeof(GDI_DC_OBJECT));
    Dc->Magic = GDI_DC_MAGIC;
    Dc->hFont = NULL;

    KsAcquireSpinLock(&gDcListLock, &OldIrql);
    InsertTailList(&gDcListHead, &Dc->ListEntry);
    KsReleaseSpinLock(&gDcListLock, OldIrql);

    return Dc;
}

BOOLEAN
VEAPI
GdipFreeDc(
    IN HDC hdc
)
{
    KIRQL OldIrql;
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);

    /* Jangan hapus Screen DC global */
    if (!Dc || Dc == gScreenDc)
    {
        return FALSE;
    }

    KsAcquireSpinLock(&gDcListLock, &OldIrql);
    /* Cek ulang di dalam lock untuk mencegah double-free */
    if (Dc->Magic != GDI_DC_MAGIC)
    {
        KsReleaseSpinLock(&gDcListLock, OldIrql);
        return FALSE;
    }

    RemoveEntryList(&Dc->ListEntry);
    Dc->Magic = 0; /* Invalidate signature supaya use-after-free langsung ketolak */
    KsReleaseSpinLock(&gDcListLock, OldIrql);

    GDI_POOL_FREE(Dc);
    return TRUE;
}

VOID
GdipInitializeDcManager(VOID)
{
    InitializeListHead(&gDcListHead);
    KsInitializeSpinlock(&gDcListLock);

    gScreenDc = GdipAllocateDc();
}

PGDI_DC_OBJECT
VEAPI
VeaGdiGetDc(
    IN HDC hdc
)
{
    if (!hdc)
    {
        return NULL;
    }

    /* Opsional: kompatibilitas kalau ada kode lama yang masih pass (HDC)1 untuk screen DC */
    if (hdc == (HDC)1 && gScreenDc && gScreenDc->Magic == GDI_DC_MAGIC)
    {
        return gScreenDc;
    }

    /* Tolak pointer palsu dari user-mode */
    if (MI_IS_USER_ADDRESS((ULONG_PTR)hdc))
    {
        return NULL;
    }

    PGDI_DC_OBJECT Dc = (PGDI_DC_OBJECT)hdc;
    if (Dc->Magic != GDI_DC_MAGIC)
    {
        return NULL;
    }

    return Dc;
}

static
BOOLEAN
VeaGdiCopyFromCaller(
    OUT PVOID Destination,
    IN CONST VOID *Source,
    IN SIZE_T Size
)
{
    if (!Destination || !Source || Size == 0)
    {
        return FALSE;
    }

    if (KeGetPreviousMode() == UserMode)
    {
        ULONG_PTR Start = (ULONG_PTR)Source;
        ULONG_PTR Highest = (ULONG_PTR)MM_HIGHEST_USER_ADDRESS;

        if (Start >= Highest || Size > Highest - Start)
        {
            return FALSE;
        }
    }

    _VEA_TRY
    {
        RtlCopyMemory(Destination, (PVOID)Source, Size);
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        return FALSE;
    }
    _VEA_END

    return TRUE;
}

BOOLEAN
VeaGdiCreateSurfaceView(
    IN PGDI_DC_OBJECT Dc,
    OUT PVEAGDI_SURFACE View,
    OUT PLONG ViewOriginX,
    OUT PLONG ViewOriginY
)
{
    PVEAGDI_SURFACE Primary = &GpPrimaryDisplay.PrimarySurface;
    LONGLONG Left;
    LONGLONG Top;
    LONGLONG Right;
    LONGLONG Bottom;

    if (!Dc || !View || !Primary->Scan0 || Primary->BytesPerPixel != sizeof(ULONG) ||
        Primary->Pitch < Primary->Width * sizeof(ULONG))
    {
        return FALSE;
    }

    Left = Dc->ClipLeft > 0 ? Dc->ClipLeft : 0;
    Top = Dc->ClipTop > 0 ? Dc->ClipTop : 0;
    Right = Dc->ClipRight < (LONG)Primary->Width ? Dc->ClipRight : Primary->Width;
    Bottom = Dc->ClipBottom < (LONG)Primary->Height ? Dc->ClipBottom : Primary->Height;

    if (Left >= Right || Top >= Bottom)
    {
        return FALSE;
    }

    *View = *Primary;
    View->Scan0 = (PUCHAR)Primary->Scan0 + (SIZE_T)Top * Primary->Pitch + (SIZE_T)Left * sizeof(ULONG);
    View->Width = (ULONG)(Right - Left);
    View->Height = (ULONG)(Bottom - Top);
    View->BufferSize = (SIZE_T)(View->Height - 1) * View->Pitch +
                       (SIZE_T)View->Width * sizeof(ULONG);
    *ViewOriginX = (LONG)Left;
    *ViewOriginY = (LONG)Top;
    return TRUE;
}

static
BOOLEAN
VeaGdiMapPoint(
    IN PGDI_DC_OBJECT Dc,
    IN LONG ViewOriginX,
    IN LONG ViewOriginY,
    IN INT X,
    IN INT Y,
    OUT PLONG ViewX,
    OUT PLONG ViewY
)
{
    LONGLONG MappedX = (LONGLONG)Dc->OriginX + X - ViewOriginX;
    LONGLONG MappedY = (LONGLONG)Dc->OriginY + Y - ViewOriginY;

    if (MappedX < 0 || MappedY < 0 ||
        MappedX >= GpPrimaryDisplay.PrimarySurface.Width ||
        MappedY >= GpPrimaryDisplay.PrimarySurface.Height)
    {
        return FALSE;
    }

    *ViewX = (LONG)MappedX;
    *ViewY = (LONG)MappedY;
    return TRUE;
}

static
BOOLEAN
VeaGdiMapRect(
    IN PGDI_DC_OBJECT Dc,
    IN LONG ViewOriginX,
    IN LONG ViewOriginY,
    IN CONST RECT *Rect,
    IN PVEAGDI_SURFACE View,
    OUT PLONG X,
    OUT PLONG Y,
    OUT PULONG Width,
    OUT PULONG Height
)
{
    LONGLONG Left;
    LONGLONG Top;
    LONGLONG Right;
    LONGLONG Bottom;

    if (Rect->left > Rect->right || Rect->top > Rect->bottom)
    {
        return FALSE;
    }

    Left = (LONGLONG)Dc->OriginX + Rect->left - ViewOriginX;
    Top = (LONGLONG)Dc->OriginY + Rect->top - ViewOriginY;
    Right = (LONGLONG)Dc->OriginX + Rect->right - ViewOriginX;
    Bottom = (LONGLONG)Dc->OriginY + Rect->bottom - ViewOriginY;

    if (Left < 0) Left = 0;
    if (Top < 0) Top = 0;
    if (Right > View->Width) Right = View->Width;
    if (Bottom > View->Height) Bottom = View->Height;

    if (Left >= Right || Top >= Bottom)
    {
        *X = 0;
        *Y = 0;
        *Width = 0;
        *Height = 0;
        return TRUE;
    }

    *X = (LONG)Left;
    *Y = (LONG)Top;
    *Width = (ULONG)(Right - Left);
    *Height = (ULONG)(Bottom - Top);
    return TRUE;
}

static
BOOLEAN
VeaGdiCreateSourceSurface(
    IN CONST PULONG Pixels,
    IN ULONG Width,
    IN ULONG Height,
    OUT PVEAGDI_SURFACE Surface
)
{
    SIZE_T ByteSize;

    if (!Pixels || !Surface || Width == 0 || Height == 0 ||
        Width > 1920 || Height > 1080)
    {
        return FALSE;
    }

    ByteSize = (SIZE_T)Width * Height * sizeof(ULONG);
    if (KeGetPreviousMode() == UserMode)
    {
        ULONG_PTR Start = (ULONG_PTR)Pixels;
        ULONG_PTR Highest = (ULONG_PTR)MM_HIGHEST_USER_ADDRESS;

        if (Start >= Highest || ByteSize > Highest - Start)
        {
            return FALSE;
        }
    }

    RtlZeroMemory(Surface, sizeof(*Surface));
    Surface->Width = Width;
    Surface->Height = Height;
    Surface->Pitch = Width * sizeof(ULONG);
    Surface->BitsPerPixel = 32;
    Surface->BytesPerPixel = sizeof(ULONG);
    Surface->Scan0 = (PVOID)Pixels;
    Surface->BufferSize = ByteSize;
    return TRUE;
}

static
BOOLEAN
VeaGdiBlit(
    IN HDC hdc,
    IN INT DstX,
    IN INT DstY,
    IN ULONG Width,
    IN ULONG Height,
    IN CONST PULONG Pixels,
    IN ULONG SourceWidth,
    IN ULONG SourceHeight,
    IN INT SrcX,
    IN INT SrcY,
    IN ULONG Value,
    IN VEA_GDI_BLIT_KIND Kind
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    VEAGDI_SURFACE DstView;
    VEAGDI_SURFACE SrcView;
    LONG ViewOriginX;
    LONG ViewOriginY;
    LONGLONG ViewX;
    LONGLONG ViewY;

    if (!Dc || Width == 0 || Height == 0 ||
        Width > 1920 || Height > 1080 ||
        !VeaGdiCreateSourceSurface(Pixels, SourceWidth, SourceHeight, &SrcView) ||
        !VeaGdiCreateSurfaceView(Dc, &DstView, &ViewOriginX, &ViewOriginY))
    {
        return FALSE;
    }

    ViewX = (LONGLONG)Dc->OriginX + DstX - ViewOriginX;
    ViewY = (LONGLONG)Dc->OriginY + DstY - ViewOriginY;

    if (ViewX >= DstView.Width || ViewY >= DstView.Height ||
        ViewX + Width <= 0 || ViewY + Height <= 0 ||
        (LONGLONG)SrcX >= SourceWidth || (LONGLONG)SrcY >= SourceHeight ||
        (LONGLONG)SrcX + Width <= 0 || (LONGLONG)SrcY + Height <= 0)
    {
        return TRUE;
    }

    _VEA_TRY
    {
        switch (Kind)
        {
            case VeaGdiBlitCopy:
                EngBitBlt32(&DstView, (LONG)ViewX, (LONG)ViewY, Width, Height,
                            &SrcView, SrcX, SrcY);
                break;

            case VeaGdiBlitTransparent:
                EngTransparentBlt32(&DstView, (LONG)ViewX, (LONG)ViewY, Width, Height,
                                    &SrcView, SrcX, SrcY, Value);
                break;

            case VeaGdiBlitAlpha:
                EngAlphaBlend32(&DstView, (LONG)ViewX, (LONG)ViewY, Width, Height,
                                &SrcView, SrcX, SrcY, (UCHAR)Value);
                break;
        }
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        return FALSE;
    }
    _VEA_END

    return TRUE;
}

HDC
VEAPI
GdiCreateWindowDC(
    IN HWND hWnd,
    IN LONG OriginX,
    IN LONG OriginY,
    IN ULONG Width,
    IN ULONG Height
)
{
    KIRQL OldIrql;

    PGDI_DC_OBJECT Dc = GdipAllocateDc();
    if (!Dc)
    {
        return NULL;
    }

    RtlZeroMemory(Dc, sizeof(GDI_DC_OBJECT));

    Dc->Magic      = GDI_DC_MAGIC;
    Dc->Used       = TRUE;
    Dc->hDC        = (HDC)Dc;
    Dc->hWnd       = hWnd;
    Dc->hFont      = NULL;
    Dc->OriginX    = OriginX;
    Dc->OriginY    = OriginY;
    Dc->ClipLeft   = OriginX;
    Dc->ClipTop    = OriginY;
    Dc->ClipRight  = OriginX + (LONG)Width;
    Dc->ClipBottom = OriginY + (LONG)Height;
    Dc->TextColor  = 0xFF000000;
    Dc->BkColor    = 0xFFFFFFFF;

    KsAcquireSpinLock(&gDcListLock, &OldIrql);
    InsertTailList(&gDcListHead, &Dc->ListEntry);
    KsReleaseSpinLock(&gDcListLock, OldIrql);

    return Dc->hDC;
}

VOID
VEAPI
GdiReleaseWindowDC(
    IN HDC hdc
)
{
    KIRQL OldIrql;

    if (!hdc || MI_IS_USER_ADDRESS((ULONG_PTR)hdc))
    {
        return;
    }

    GdipFreeDc(hdc);
}

BOOLEAN
VEAPI
VeaGdiFillRect(
    IN HDC hdc,
    IN CONST PRECT lprc,
    IN ULONG ColorARGB
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    VEAGDI_SURFACE View;
    RECT LocalRect;
    LONG ViewOriginX;
    LONG ViewOriginY;
    LONG X;
    LONG Y;
    ULONG Width;
    ULONG Height;

    if (!Dc || !VeaGdiCopyFromCaller(&LocalRect, lprc, sizeof(LocalRect)) ||
        !VeaGdiCreateSurfaceView(Dc, &View, &ViewOriginX, &ViewOriginY) ||
        !VeaGdiMapRect(Dc, ViewOriginX, ViewOriginY, &LocalRect, &View,
                       &X, &Y, &Width, &Height))
    {
        return FALSE;
    }

    if (Width != 0 && Height != 0)
    {
        EngFillRect32(&View, (ULONG)X, (ULONG)Y, Width, Height, ColorARGB);
    }

    return TRUE;
}

ULONG
VEAPI
VeaGdiSetPixel(
    IN HDC hdc,
    IN INT X,
    IN INT Y,
    IN ULONG ColorARGB
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    VEAGDI_SURFACE View;
    LONG ViewOriginX;
    LONG ViewOriginY;
    LONG ViewX;
    LONG ViewY;

    if (!Dc || !VeaGdiCreateSurfaceView(Dc, &View, &ViewOriginX, &ViewOriginY) ||
        !VeaGdiMapPoint(Dc, ViewOriginX, ViewOriginY, X, Y, &ViewX, &ViewY) ||
        ViewX >= (LONG)View.Width || ViewY >= (LONG)View.Height)
    {
        return 0;
    }

    EngSetPixel32(&View, ViewX, ViewY, ColorARGB);
    return ColorARGB;
}

ULONG
VEAPI
VeaGdiGetPixel(
    IN HDC hdc,
    IN INT X,
    IN INT Y
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    VEAGDI_SURFACE View;
    LONG ViewOriginX;
    LONG ViewOriginY;
    LONG ViewX;
    LONG ViewY;

    if (!Dc || !VeaGdiCreateSurfaceView(Dc, &View, &ViewOriginX, &ViewOriginY) ||
        !VeaGdiMapPoint(Dc, ViewOriginX, ViewOriginY, X, Y, &ViewX, &ViewY) ||
        ViewX >= (LONG)View.Width || ViewY >= (LONG)View.Height)
    {
        return 0;
    }

    return EngGetPixel32(&View, ViewX, ViewY);
}

static
ULONG
VeaGdiLineOutCode(
    IN LONGLONG X,
    IN LONGLONG Y,
    IN LONG Width,
    IN LONG Height
)
{
    ULONG Code = 0;

    if (X < 0) Code |= 1;
    else if (X >= Width) Code |= 2;
    if (Y < 0) Code |= 4;
    else if (Y >= Height) Code |= 8;
    return Code;
}

static
BOOLEAN
VeaGdiClipLine(
    IN LONG Width,
    IN LONG Height,
    IN OUT PLONGLONG X1,
    IN OUT PLONGLONG Y1,
    IN OUT PLONGLONG X2,
    IN OUT PLONGLONG Y2
)
{
    for (ULONG Iteration = 0; Iteration < 8; Iteration++)
    {
        ULONG Code1 = VeaGdiLineOutCode(*X1, *Y1, Width, Height);
        ULONG Code2 = VeaGdiLineOutCode(*X2, *Y2, Width, Height);
        ULONG CodeOut;
        LONGLONG X;
        LONGLONG Y;

        if ((Code1 | Code2) == 0)
        {
            return TRUE;
        }
        if (Code1 & Code2)
        {
            return FALSE;
        }

        CodeOut = Code1 ? Code1 : Code2;
        if (CodeOut & 4)
        {
            if (*Y2 == *Y1) return FALSE;
            X = *X1 + (*X2 - *X1) * (0 - *Y1) / (*Y2 - *Y1);
            Y = 0;
        }
        else if (CodeOut & 8)
        {
            if (*Y2 == *Y1) return FALSE;
            Y = Height - 1;
            X = *X1 + (*X2 - *X1) * (Y - *Y1) / (*Y2 - *Y1);
        }
        else if (CodeOut & 2)
        {
            if (*X2 == *X1) return FALSE;
            X = Width - 1;
            Y = *Y1 + (*Y2 - *Y1) * (X - *X1) / (*X2 - *X1);
        }
        else
        {
            if (*X2 == *X1) return FALSE;
            X = 0;
            Y = *Y1 + (*Y2 - *Y1) * (X - *X1) / (*X2 - *X1);
        }

        if (CodeOut == Code1)
        {
            *X1 = X;
            *Y1 = Y;
        }
        else
        {
            *X2 = X;
            *Y2 = Y;
        }
    }

    return FALSE;
}

BOOLEAN
VEAPI
VeaGdiLineTo(
    IN HDC hdc,
    IN INT X1,
    IN INT Y1,
    IN INT X2,
    IN INT Y2,
    IN ULONG ColorARGB
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    VEAGDI_SURFACE View;
    LONG ViewOriginX;
    LONG ViewOriginY;
    LONGLONG ViewX1;
    LONGLONG ViewY1;
    LONGLONG ViewX2;
    LONGLONG ViewY2;

    if (!Dc || !VeaGdiCreateSurfaceView(Dc, &View, &ViewOriginX, &ViewOriginY))
    {
        return FALSE;
    }

    ViewX1 = (LONGLONG)Dc->OriginX + X1 - ViewOriginX;
    ViewY1 = (LONGLONG)Dc->OriginY + Y1 - ViewOriginY;
    ViewX2 = (LONGLONG)Dc->OriginX + X2 - ViewOriginX;
    ViewY2 = (LONGLONG)Dc->OriginY + Y2 - ViewOriginY;

    if (ViewX1 < -1000000 || ViewX1 > 1000000 || ViewX2 < -1000000 || ViewX2 > 1000000 ||
        ViewY1 < -1000000 || ViewY1 > 1000000 || ViewY2 < -1000000 || ViewY2 > 1000000)
    {
        return FALSE;
    }

    if (!VeaGdiClipLine((LONG)View.Width, (LONG)View.Height,
                        &ViewX1, &ViewY1, &ViewX2, &ViewY2))
    {
        return TRUE;
    }

    EngLineTo32(&View, (LONG)ViewX1, (LONG)ViewY1, (LONG)ViewX2, (LONG)ViewY2, ColorARGB);
    return TRUE;
}

BOOLEAN
VEAPI
VeaGdiInvertRect(
    IN HDC hdc,
    IN CONST PRECT lprc
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    VEAGDI_SURFACE View;
    RECT Rect;
    LONG ViewOriginX;
    LONG ViewOriginY;
    LONG X;
    LONG Y;
    ULONG Width;
    ULONG Height;

    if (!Dc || !VeaGdiCopyFromCaller(&Rect, lprc, sizeof(Rect)) ||
        !VeaGdiCreateSurfaceView(Dc, &View, &ViewOriginX, &ViewOriginY) ||
        !VeaGdiMapRect(Dc, ViewOriginX, ViewOriginY, &Rect, &View,
                       &X, &Y, &Width, &Height))
    {
        return FALSE;
    }

    if (Width != 0 && Height != 0)
    {
        EngInvertRect32(&View, X, Y, Width, Height);
    }

    return TRUE;
}

BOOLEAN
VEAPI
VeaGdiGradientFill(
    IN HDC hdc,
    IN CONST PRECT lprc,
    IN ULONG ColorStartARGB,
    IN ULONG ColorEndARGB,
    IN BOOLEAN Vertical
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    VEAGDI_SURFACE View;
    RECT Rect;
    LONG ViewOriginX;
    LONG ViewOriginY;
    LONG X;
    LONG Y;
    ULONG Width;
    ULONG Height;

    if (!Dc || !VeaGdiCopyFromCaller(&Rect, lprc, sizeof(Rect)) ||
        !VeaGdiCreateSurfaceView(Dc, &View, &ViewOriginX, &ViewOriginY) ||
        !VeaGdiMapRect(Dc, ViewOriginX, ViewOriginY, &Rect, &View,
                       &X, &Y, &Width, &Height))
    {
        return FALSE;
    }

    if (Width != 0 && Height != 0)
    {
        EngGradientFill32(&View, X, Y, Width, Height,
                          ColorStartARGB, ColorEndARGB, Vertical);
    }

    return TRUE;
}

BOOLEAN
VEAPI
VeaGdiBitBlt(
    IN HDC hdc,
    IN INT DstX,
    IN INT DstY,
    IN ULONG Width,
    IN ULONG Height,
    IN CONST PULONG Pixels,
    IN ULONG SourceWidth,
    IN ULONG SourceHeight,
    IN INT SrcX,
    IN INT SrcY
)
{
    return VeaGdiBlit(hdc, DstX, DstY, Width, Height, Pixels,
                      SourceWidth, SourceHeight, SrcX, SrcY, 0, VeaGdiBlitCopy);
}

BOOLEAN
VEAPI
VeaGdiTransparentBlt(
    IN HDC hdc,
    IN INT DstX,
    IN INT DstY,
    IN ULONG Width,
    IN ULONG Height,
    IN CONST PULONG Pixels,
    IN ULONG SourceWidth,
    IN ULONG SourceHeight,
    IN INT SrcX,
    IN INT SrcY,
    IN ULONG TransparentColorRGB
)
{
    return VeaGdiBlit(hdc, DstX, DstY, Width, Height, Pixels,
                      SourceWidth, SourceHeight, SrcX, SrcY,
                      TransparentColorRGB, VeaGdiBlitTransparent);
}

BOOLEAN
VEAPI
VeaGdiAlphaBlend(
    IN HDC hdc,
    IN INT DstX,
    IN INT DstY,
    IN ULONG Width,
    IN ULONG Height,
    IN CONST PULONG Pixels,
    IN ULONG SourceWidth,
    IN ULONG SourceHeight,
    IN INT SrcX,
    IN INT SrcY,
    IN UCHAR Alpha
)
{
    return VeaGdiBlit(hdc, DstX, DstY, Width, Height, Pixels,
                      SourceWidth, SourceHeight, SrcX, SrcY,
                      Alpha, VeaGdiBlitAlpha);
}

BOOLEAN
VEAPI
VeaGdiDrawGlyph(
    IN HDC hdc,
    IN INT X,
    IN INT Y,
    IN CONST UCHAR GlyphBitmap[16],
    IN ULONG TextColorARGB,
    IN ULONG BkColorARGB,
    IN BOOLEAN TransparentBk
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    VEAGDI_SURFACE View;
    UCHAR LocalGlyph[16];
    LONG ViewOriginX;
    LONG ViewOriginY;
    LONGLONG ViewX;
    LONGLONG ViewY;

    if (!Dc || !VeaGdiCopyFromCaller(LocalGlyph, GlyphBitmap, sizeof(LocalGlyph)) ||
        !VeaGdiCreateSurfaceView(Dc, &View, &ViewOriginX, &ViewOriginY))
    {
        return FALSE;
    }

    ViewX = (LONGLONG)Dc->OriginX + X - ViewOriginX;
    ViewY = (LONGLONG)Dc->OriginY + Y - ViewOriginY;
    if (ViewX >= View.Width || ViewY >= View.Height ||
        ViewX + 8 <= 0 || ViewY + 16 <= 0)
    {
        return TRUE;
    }

    EngDrawGlyph8x16(&View, (LONG)ViewX, (LONG)ViewY, LocalGlyph,
                     TextColorARGB, BkColorARGB, TransparentBk);
    return TRUE;
}

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
)
{
    return VeaGdiBlit(hdc, DstX, DstY, Width, Height, Pixels,
                      Width, Height, 0, 0, Alpha, VeaGdiBlitAlpha);
}

/* EOF */