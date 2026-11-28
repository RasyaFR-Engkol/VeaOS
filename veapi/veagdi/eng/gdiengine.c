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
    Dc->hBrush = (HBRUSH)VeaGdiGetStockObject(WHITE_BRUSH);
    Dc->hPen   = (HPEN)VeaGdiGetStockObject(BLACK_PEN);
    Dc->PenX   = 0;
    Dc->PenY   = 0;

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

VOID
VEAPI
GdiAccumulateDirtyRect(
    IN PGDI_DC_OBJECT Dc,
    IN LONG           Left,
    IN LONG           Top,
    IN LONG           Right,
    IN LONG           Bottom
)
{
    if (!Dc || Left >= Right || Top >= Bottom)
    {
        return;
    }

    if (!Dc->HasDirty)
    {
        Dc->DirtyLeft   = Left;
        Dc->DirtyTop    = Top;
        Dc->DirtyRight  = Right;
        Dc->DirtyBottom = Bottom;
        Dc->HasDirty    = TRUE;
    }
    else
    {
        if (Left   < Dc->DirtyLeft)   Dc->DirtyLeft   = Left;
        if (Top    < Dc->DirtyTop)    Dc->DirtyTop    = Top;
        if (Right  > Dc->DirtyRight)  Dc->DirtyRight  = Right;
        if (Bottom > Dc->DirtyBottom) Dc->DirtyBottom = Bottom;
    }
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

ULONG
VeaGdiCreateSurfaceViews(
    IN  PGDI_DC_OBJECT  Dc,
    OUT PVEAGDI_SURFACE Views,
    OUT PLONG           ViewOriginsX,
    OUT PLONG           ViewOriginsY,
    IN  ULONG           MaxViews
)
{
    PVEAGDI_SURFACE Primary = EngGetTargetSurface();

    if (!Dc || !Views || !ViewOriginsX || !ViewOriginsY || MaxViews == 0 ||
        !Primary || !Primary->Scan0 || Primary->BytesPerPixel != sizeof(ULONG) ||
        Primary->Pitch < Primary->Width * sizeof(ULONG))
    {
        return 0;
    }

    RECT ClipRect;
    ClipRect.left   = Dc->ClipLeft;
    ClipRect.top    = Dc->ClipTop;
    ClipRect.right  = Dc->ClipRight;
    ClipRect.bottom = Dc->ClipBottom;

    RECT VisRects[MAX_VISIBLE_CLIP_RECTS];
    ULONG VisCount = 0;

    if (Dc->hWnd != NULL)
    {
        PWND_OBJECT Wnd = WndLookupHandle(Dc->hWnd);
        if (!Wnd)
        {
            return 0;
        }

        BOOLEAN Allowed = WndIsEffectivelyVisible(Wnd) &&
                          (GpActiveDesktop == NULL || Wnd->Desktop == GpActiveDesktop);
        if (Allowed)
        {
            ULONG Cap = (MaxViews < MAX_VISIBLE_CLIP_RECTS) ? MaxViews : MAX_VISIBLE_CLIP_RECTS;
            VisCount = WndComputeVisibleRects(Wnd, &ClipRect, VisRects, Cap);
        }
        WndDereference(Wnd);

        if (!Allowed || VisCount == 0)
        {
            return 0;
        }
    }
    else
    {
        if (ClipRect.left < 0) ClipRect.left = 0;
        if (ClipRect.top < 0)  ClipRect.top  = 0;
        if (ClipRect.right > (LONG)Primary->Width)   ClipRect.right  = (LONG)Primary->Width;
        if (ClipRect.bottom > (LONG)Primary->Height) ClipRect.bottom = (LONG)Primary->Height;

        if (ClipRect.left >= ClipRect.right || ClipRect.top >= ClipRect.bottom)
        {
            return 0;
        }

        VisRects[0] = ClipRect;
        VisCount = 1;
    }

    for (ULONG i = 0; i < VisCount; i++)
    {
        LONG L = VisRects[i].left;
        LONG T = VisRects[i].top;
        LONG R = VisRects[i].right;
        LONG B = VisRects[i].bottom;

        Views[i] = *Primary;
        Views[i].Scan0      = (PUCHAR)Primary->Scan0 + (SIZE_T)T * Primary->Pitch + (SIZE_T)L * sizeof(ULONG);
        Views[i].Width      = (ULONG)(R - L);
        Views[i].Height     = (ULONG)(B - T);
        Views[i].BufferSize = (SIZE_T)(Views[i].Height - 1) * Views[i].Pitch +
                              (SIZE_T)Views[i].Width * sizeof(ULONG);
        ViewOriginsX[i] = L;
        ViewOriginsY[i] = T;
    }

    return VisCount;
}

BOOLEAN
VeaGdiCreateSurfaceView(
    IN PGDI_DC_OBJECT Dc,
    OUT PVEAGDI_SURFACE View,
    OUT PLONG ViewOriginX,
    OUT PLONG ViewOriginY
)
{
    PVEAGDI_SURFACE Primary = EngGetTargetSurface();
    LONGLONG Left;
    LONGLONG Top;
    LONGLONG Right;
    LONGLONG Bottom;

    if (!Dc || !View || !Primary->Scan0 || Primary->BytesPerPixel != sizeof(ULONG) ||
        Primary->Pitch < Primary->Width * sizeof(ULONG))
    {
        return FALSE;
    }

    /* Cegah window di desktop non-aktif / hidden menggambar ke layar aktif */
    if (Dc->hWnd != NULL)
    {
        PWND_OBJECT Wnd = WndLookupHandle(Dc->hWnd);
        if (!Wnd)
        {
            return FALSE;
        }

        BOOLEAN Allowed = WndIsEffectivelyVisible(Wnd) &&
                  (GpActiveDesktop == NULL || Wnd->Desktop == GpActiveDesktop);
        WndDereference(Wnd);

        if (!Allowed)
        {
            return FALSE;
        }
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
    IN  PGDI_DC_OBJECT  Dc,
    IN  PVEAGDI_SURFACE View,
    IN  LONG            ViewOriginX,
    IN  LONG            ViewOriginY,
    IN  INT             X,
    IN  INT             Y,
    OUT PLONG           ViewX,
    OUT PLONG           ViewY
)
{
    LONGLONG MappedX = (LONGLONG)Dc->OriginX + X - ViewOriginX;
    LONGLONG MappedY = (LONGLONG)Dc->OriginY + Y - ViewOriginY;

    if (MappedX < 0 || MappedY < 0 ||
        MappedX >= (LONG)View->Width ||
        MappedY >= (LONG)View->Height)
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
ULONG
VeaGdiInterpolateColor(
    IN ULONG ColorStartARGB,
    IN ULONG ColorEndARGB,
    IN LONG  Num,
    IN LONG  Denom
)
{
    if (Denom <= 0 || Num <= 0) return ColorStartARGB;
    if (Num >= Denom) return ColorEndARGB;

    LONG A1 = (ColorStartARGB >> 24) & 0xFF, R1 = (ColorStartARGB >> 16) & 0xFF;
    LONG G1 = (ColorStartARGB >> 8)  & 0xFF, B1 = (ColorStartARGB)       & 0xFF;

    LONG A2 = (ColorEndARGB >> 24) & 0xFF, R2 = (ColorEndARGB >> 16) & 0xFF;
    LONG G2 = (ColorEndARGB >> 8)  & 0xFF, B2 = (ColorEndARGB)       & 0xFF;

    ULONG A = (ULONG)(A1 + ((A2 - A1) * Num) / Denom);
    ULONG R = (ULONG)(R1 + ((R2 - R1) * Num) / Denom);
    ULONG G = (ULONG)(G1 + ((G2 - G1) * Num) / Denom);
    ULONG B = (ULONG)(B1 + ((B2 - B1) * Num) / Denom);

    return (A << 24) | (R << 16) | (G << 8) | B;
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
    VEAGDI_SURFACE Views[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsX[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsY[MAX_VISIBLE_CLIP_RECTS];
    VEAGDI_SURFACE SrcView;

    if (!Dc || Width == 0 || Height == 0 ||
        Width > 1920 || Height > 1080 ||
        !VeaGdiCreateSourceSurface(Pixels, SourceWidth, SourceHeight, &SrcView))
    {
        return FALSE;
    }

    if ((LONGLONG)SrcX >= (LONGLONG)SourceWidth || (LONGLONG)SrcY >= (LONGLONG)SourceHeight ||
        (LONGLONG)SrcX + (LONGLONG)Width <= 0 || (LONGLONG)SrcY + (LONGLONG)Height <= 0)
    {
        return TRUE;
    }

    ULONG ViewCount = VeaGdiCreateSurfaceViews(
        Dc, Views, OriginsX, OriginsY, MAX_VISIBLE_CLIP_RECTS
    );
    if (ViewCount == 0)
    {
        return (Dc->hWnd != NULL);
    }

    _VEA_TRY
    {
        for (ULONG i = 0; i < ViewCount; i++)
        {
            LONGLONG ViewX = (LONGLONG)Dc->OriginX + DstX - OriginsX[i];
            LONGLONG ViewY = (LONGLONG)Dc->OriginY + DstY - OriginsY[i];

            if (ViewX >= (LONGLONG)Views[i].Width || ViewY >= (LONGLONG)Views[i].Height ||
                ViewX + (LONGLONG)Width <= 0 || ViewY + (LONGLONG)Height <= 0)
            {
                continue;
            }

            switch (Kind)
            {
                case VeaGdiBlitCopy:
                    EngBitBlt32(&Views[i], (LONG)ViewX, (LONG)ViewY, Width, Height,
                                &SrcView, SrcX, SrcY);
                    break;

                case VeaGdiBlitTransparent:
                    EngTransparentBlt32(&Views[i], (LONG)ViewX, (LONG)ViewY, Width, Height,
                                        &SrcView, SrcX, SrcY, Value);
                    break;

                case VeaGdiBlitAlpha:
                    EngAlphaBlend32(&Views[i], (LONG)ViewX, (LONG)ViewY, Width, Height,
                                    &SrcView, SrcX, SrcY, (UCHAR)Value);
                    break;
            }

            GdiAccumulateDirtyRect(
                Dc,
                OriginsX[i] + ViewX,
                OriginsY[i] + ViewY,
                OriginsX[i] + ViewX + 1,
                OriginsY[i] + ViewY + 1
            );
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
    PGDI_DC_OBJECT Dc = GdipAllocateDc();
    if (!Dc)
    {
        return NULL;
    }

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
    Dc->HasDirty   = FALSE;

    return Dc->hDC;
}

BOOLEAN
VEAPI
GdiReleaseWindowDC(
    IN HDC hdc
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    if (!Dc || Dc == gScreenDc)
    {
        return FALSE;
    }

    return GdipFreeDc(hdc);
}

BOOLEAN
VEAPI
VeaGdiFillRectInternal(
    IN HDC hdc,
    IN CONST RECT *Rect,
    IN ULONG ColorARGB
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);
    VEAGDI_SURFACE Views[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsX[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsY[MAX_VISIBLE_CLIP_RECTS];

    if (!Dc || !Rect)
    {
        return FALSE;
    }

    if (Rect->left > Rect->right || Rect->top > Rect->bottom)
    {
        return FALSE;
    }

    ULONG ViewCount = VeaGdiCreateSurfaceViews(
        Dc, Views, OriginsX, OriginsY, MAX_VISIBLE_CLIP_RECTS
    );
    if (ViewCount == 0)
    {
        return (Dc->hWnd != NULL);
    }

    for (ULONG i = 0; i < ViewCount; i++)
    {
        LONG X, Y;
        ULONG Width, Height;
        if (!VeaGdiMapRect(Dc, OriginsX[i], OriginsY[i], Rect, &Views[i],
                           &X, &Y, &Width, &Height))
        {
            return FALSE;
        }

        if (Width != 0 && Height != 0)
        {
            EngFillRect32(&Views[i], (ULONG)X, (ULONG)Y, Width, Height, ColorARGB);

            GdiAccumulateDirtyRect(
                Dc,
                OriginsX[i] + X,
                OriginsY[i] + Y,
                OriginsX[i] + X + (LONG)Width,
                OriginsY[i] + Y + (LONG)Height
            );
        }
    }

    return TRUE;
}

BOOLEAN
VEAPI
VeaGdiFillRect(
    IN HDC hdc,
    IN CONST PRECT lprc,
    IN ULONG ColorARGB
)
{
    RECT LocalRect;

    if (!VeaGdiGetDc(hdc) ||
        !VeaGdiCopyFromCaller(&LocalRect, lprc, sizeof(LocalRect)))
    {
        return FALSE;
    }

    return VeaGdiFillRectInternal(hdc, &LocalRect, ColorARGB);
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
    VEAGDI_SURFACE Views[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsX[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsY[MAX_VISIBLE_CLIP_RECTS];

    if (!Dc)
    {
        return 0;
    }

    ULONG ViewCount = VeaGdiCreateSurfaceViews(
        Dc, Views, OriginsX, OriginsY, MAX_VISIBLE_CLIP_RECTS
    );

    for (ULONG i = 0; i < ViewCount; i++)
    {
        LONG ViewX, ViewY;
        if (VeaGdiMapPoint(Dc, &Views[i], OriginsX[i], OriginsY[i], X, Y, &ViewX, &ViewY))
        {
            EngSetPixel32(&Views[i], ViewX, ViewY, ColorARGB);
            GdiAccumulateDirtyRect(
                Dc,
                OriginsX[i] + ViewX,
                OriginsY[i] + ViewY,
                OriginsX[i] + ViewX + 1,
                OriginsY[i] + ViewY + 1
            );
            return ColorARGB;
        }
    }

    return 0;
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
    VEAGDI_SURFACE Views[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsX[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsY[MAX_VISIBLE_CLIP_RECTS];

    if (!Dc)
    {
        return 0;
    }

    ULONG ViewCount = VeaGdiCreateSurfaceViews(
        Dc, Views, OriginsX, OriginsY, MAX_VISIBLE_CLIP_RECTS
    );

    for (ULONG i = 0; i < ViewCount; i++)
    {
        LONG ViewX, ViewY;
        if (VeaGdiMapPoint(Dc, &Views[i], OriginsX[i], OriginsY[i], X, Y, &ViewX, &ViewY))
        {
            return EngGetPixel32(&Views[i], ViewX, ViewY);
        }
    }

    return 0;
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
    VEAGDI_SURFACE Views[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsX[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsY[MAX_VISIBLE_CLIP_RECTS];

    if (!Dc)
    {
        return FALSE;
    }

    if (X1 < -1000000 || X1 > 1000000 || X2 < -1000000 || X2 > 1000000 ||
        Y1 < -1000000 || Y1 > 1000000 || Y2 < -1000000 || Y2 > 1000000)
    {
        return FALSE;
    }

    ULONG ViewCount = VeaGdiCreateSurfaceViews(
        Dc, Views, OriginsX, OriginsY, MAX_VISIBLE_CLIP_RECTS
    );
    if (ViewCount == 0)
    {
        return (Dc->hWnd != NULL);
    }

    for (ULONG i = 0; i < ViewCount; i++)
    {
        LONGLONG ViewX1 = (LONGLONG)Dc->OriginX + X1 - OriginsX[i];
        LONGLONG ViewY1 = (LONGLONG)Dc->OriginY + Y1 - OriginsY[i];
        LONGLONG ViewX2 = (LONGLONG)Dc->OriginX + X2 - OriginsX[i];
        LONGLONG ViewY2 = (LONGLONG)Dc->OriginY + Y2 - OriginsY[i];

        if (VeaGdiClipLine((LONG)Views[i].Width, (LONG)Views[i].Height,
                           &ViewX1, &ViewY1, &ViewX2, &ViewY2))
        {
            EngLineTo32(&Views[i], (LONG)ViewX1, (LONG)ViewY1, (LONG)ViewX2, (LONG)ViewY2, ColorARGB);

            LONG MinX = (LONG)((ViewX1 < ViewX2) ? ViewX1 : ViewX2);
            LONG MinY = (LONG)((ViewY1 < ViewY2) ? ViewY1 : ViewY2);
            LONG MaxX = (LONG)((ViewX1 > ViewX2) ? ViewX1 : ViewX2);
            LONG MaxY = (LONG)((ViewY1 > ViewY2) ? ViewY1 : ViewY2);

            GdiAccumulateDirtyRect(
                Dc,
                OriginsX[i] + MinX,
                OriginsY[i] + MinY,
                OriginsX[i] + MaxX + 1,
                OriginsY[i] + MaxY + 1
            );
        }
    }

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
    VEAGDI_SURFACE Views[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsX[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsY[MAX_VISIBLE_CLIP_RECTS];
    RECT Rect;

    if (!Dc || !VeaGdiCopyFromCaller(&Rect, lprc, sizeof(Rect)))
    {
        return FALSE;
    }

    if (Rect.left > Rect.right || Rect.top > Rect.bottom)
    {
        return FALSE;
    }

    ULONG ViewCount = VeaGdiCreateSurfaceViews(
        Dc, Views, OriginsX, OriginsY, MAX_VISIBLE_CLIP_RECTS
    );
    if (ViewCount == 0)
    {
        return (Dc->hWnd != NULL);
    }

    for (ULONG i = 0; i < ViewCount; i++)
    {
        LONG X, Y;
        ULONG Width, Height;
        if (!VeaGdiMapRect(Dc, OriginsX[i], OriginsY[i], &Rect, &Views[i],
                           &X, &Y, &Width, &Height))
        {
            return FALSE;
        }

        if (Width != 0 && Height != 0)
        {
            EngInvertRect32(&Views[i], X, Y, Width, Height);
        }
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
    VEAGDI_SURFACE Views[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsX[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsY[MAX_VISIBLE_CLIP_RECTS];
    RECT Rect;

    if (!Dc || !VeaGdiCopyFromCaller(&Rect, lprc, sizeof(Rect)))
    {
        return FALSE;
    }

    if (Rect.left > Rect.right || Rect.top > Rect.bottom)
    {
        return FALSE;
    }

    ULONG ViewCount = VeaGdiCreateSurfaceViews(
        Dc, Views, OriginsX, OriginsY, MAX_VISIBLE_CLIP_RECTS
    );
    if (ViewCount == 0)
    {
        return (Dc->hWnd != NULL);
    }

    LONG FullW = Rect.right - Rect.left;
    LONG FullH = Rect.bottom - Rect.top;

    for (ULONG i = 0; i < ViewCount; i++)
    {
        LONG X, Y;
        ULONG Width, Height;
        if (!VeaGdiMapRect(Dc, OriginsX[i], OriginsY[i], &Rect, &Views[i],
                           &X, &Y, &Width, &Height))
        {
            return FALSE;
        }

        if (Width != 0 && Height != 0)
        {
            ULONG SubStart = ColorStartARGB;
            ULONG SubEnd   = ColorEndARGB;

            if (Vertical && FullH > 1)
            {
                LONG OffY = (OriginsY[i] + Y) - (Dc->OriginY + Rect.top);
                SubStart = VeaGdiInterpolateColor(ColorStartARGB, ColorEndARGB, OffY, FullH - 1);
                SubEnd   = VeaGdiInterpolateColor(ColorStartARGB, ColorEndARGB, OffY + (LONG)Height - 1, FullH - 1);
            }
            else if (!Vertical && FullW > 1)
            {
                LONG OffX = (OriginsX[i] + X) - (Dc->OriginX + Rect.left);
                SubStart = VeaGdiInterpolateColor(ColorStartARGB, ColorEndARGB, OffX, FullW - 1);
                SubEnd   = VeaGdiInterpolateColor(ColorStartARGB, ColorEndARGB, OffX + (LONG)Width - 1, FullW - 1);
            }

            EngGradientFill32(&Views[i], X, Y, Width, Height,
                              SubStart, SubEnd, Vertical);
        }
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
    VEAGDI_SURFACE Views[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsX[MAX_VISIBLE_CLIP_RECTS];
    LONG OriginsY[MAX_VISIBLE_CLIP_RECTS];
    UCHAR LocalGlyph[16];

    if (!Dc || !VeaGdiCopyFromCaller(LocalGlyph, GlyphBitmap, sizeof(LocalGlyph)))
    {
        return FALSE;
    }

    ULONG ViewCount = VeaGdiCreateSurfaceViews(
        Dc, Views, OriginsX, OriginsY, MAX_VISIBLE_CLIP_RECTS
    );
    if (ViewCount == 0)
    {
        return (Dc->hWnd != NULL);
    }

    for (ULONG i = 0; i < ViewCount; i++)
    {
        LONGLONG ViewX = (LONGLONG)Dc->OriginX + X - OriginsX[i];
        LONGLONG ViewY = (LONGLONG)Dc->OriginY + Y - OriginsY[i];

        if (ViewX >= (LONGLONG)Views[i].Width || ViewY >= (LONGLONG)Views[i].Height ||
            ViewX + 8 <= 0 || ViewY + 16 <= 0)
        {
            continue;
        }

        EngDrawGlyph8x16(&Views[i], (LONG)ViewX, (LONG)ViewY, LocalGlyph,
                         TextColorARGB, BkColorARGB, TransparentBk);
    }

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

/* DC GOES HERE */

BOOLEAN
VEAPI
VeaGdiDeleteDC(
    IN HDC hdc
)
{
    PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);

    /* Tolak NULL, pointer user-mode palsu, Screen DC global, maupun Window DC
       (Window DC wajib dilepas lewat ReleaseDC, bukan DeleteDC) */
    if (!Dc || Dc == gScreenDc || Dc->hWnd != NULL)
    {
        return FALSE;
    }

    return GdipFreeDc(hdc);
}

HDC
VEAPI
VeaUserGetDCEx(
    IN HWND   hWnd,
    IN HANDLE hrgnClip,
    IN ULONG  Flags
)
{
    /* Jalur ReleaseDC(hWnd, hDC) lewat syscall yang sama */
    if (Flags & DCX_INTERNAL_RELEASE)
    {
        HDC hdc = (HDC)hrgnClip;
        PGDI_DC_OBJECT Dc = VeaGdiGetDc(hdc);

        if (!Dc || Dc == gScreenDc)
        {
            return NULL;
        }

        if (hWnd != NULL && Dc->hWnd != hWnd)
        {
            return NULL;
        }

        if (Dc->HasDirty)
        {
            LONG ScrW = (LONG)GpPrimaryDisplay.PrimarySurface.Width;
            LONG ScrH = (LONG)GpPrimaryDisplay.PrimarySurface.Height;
            LONG L = (Dc->DirtyLeft   < 0)    ? 0    : Dc->DirtyLeft;
            LONG T = (Dc->DirtyTop    < 0)    ? 0    : Dc->DirtyTop;
            LONG R = (Dc->DirtyRight  > ScrW) ? ScrW : Dc->DirtyRight;
            LONG B = (Dc->DirtyBottom > ScrH) ? ScrH : Dc->DirtyBottom;

            if (L < R && T < B)
            {
                EngPresentRect(L, T, (ULONG)(R - L), (ULONG)(B - T));
            }
            Dc->HasDirty = FALSE;
        }

        return GdiReleaseWindowDC(hdc) ? (HDC)1 : NULL;
    }

    UNREFERENCED_PARAMETER(hrgnClip);

    /* GetDC(NULL) / GetWindowDC(NULL) -> DC seluruh layar */
    if (hWnd == NULL)
    {
        ULONG ScrW = GpPrimaryDisplay.PrimarySurface.Width;
        ULONG ScrH = GpPrimaryDisplay.PrimarySurface.Height;

        if (ScrW == 0 || ScrH == 0)
        {
            return NULL;
        }

        return GdiCreateWindowDC(NULL, 0, 0, ScrW, ScrH);
    }

    PWND_OBJECT Wnd = WndLookupHandle(hWnd);
    if (!Wnd)
    {
        return NULL;
    }

    BOOLEAN IsChild = ((Wnd->Style & WS_CHILD) != 0);
    LONG    OriginX;
    LONG    OriginY;
    LONG    ClipL, ClipT, ClipR, ClipB;
    ULONG   Width;
    ULONG   Height;

    if (IsChild)
    {
        WndGetClippedClientRect(Wnd, &OriginX, &OriginY, &ClipL, &ClipT, &ClipR, &ClipB);
        Width  = Wnd->Width;
        Height = Wnd->Height;
    }
    else if (Flags & DCX_WINDOW)
    {
        OriginX = Wnd->X;
        OriginY = Wnd->Y;
        Width   = Wnd->Width;
        Height  = Wnd->Height;
        ClipL   = OriginX;
        ClipT   = OriginY;
        ClipR   = OriginX + (LONG)Width;
        ClipB   = OriginY + (LONG)Height;
    }
    else
    {
        BOOLEAN Framed = !((Wnd->Style & WS_POPUP) && ((Wnd->Style & WS_CAPTION) != WS_CAPTION));
        OriginX = Framed ? (Wnd->X + WND_BORDER_WIDTH)    : Wnd->X;
        OriginY = Framed ? (Wnd->Y + WND_TITLEBAR_HEIGHT) : Wnd->Y;
        Width   = Framed
                  ? ((Wnd->Width > (2 * WND_BORDER_WIDTH)) ? (Wnd->Width - (2 * WND_BORDER_WIDTH)) : 0)
                  : Wnd->Width;
        Height  = Framed
                  ? ((Wnd->Height > (WND_TITLEBAR_HEIGHT + WND_BORDER_WIDTH))
                     ? (Wnd->Height - WND_TITLEBAR_HEIGHT - WND_BORDER_WIDTH) : 0)
                  : Wnd->Height;
        ClipL   = OriginX;
        ClipT   = OriginY;
        ClipR   = OriginX + (LONG)Width;
        ClipB   = OriginY + (LONG)Height;
    }

    WndDereference(Wnd);

    if (Width == 0 || Height == 0 || ClipL >= ClipR || ClipT >= ClipB)
    {
        return NULL;
    }

    HDC hdc = GdiCreateWindowDC(hWnd, OriginX, OriginY, Width, Height);
    if (hdc)
    {
        PGDI_DC_OBJECT DcObj = VeaGdiGetDc(hdc);
        if (DcObj)
        {
            DcObj->ClipLeft   = ClipL;
            DcObj->ClipTop    = ClipT;
            DcObj->ClipRight  = ClipR;
            DcObj->ClipBottom = ClipB;
        }
    }

    return hdc;
}

/* EOF */