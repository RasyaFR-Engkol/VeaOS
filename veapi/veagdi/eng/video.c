/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : video.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PURPOSE
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file video.c
 * --------------------------------------------------------------------- */

VEAPI_DISPLAY_DEVICE GpPrimaryDisplay; 
BOOLEAN UserSubsystemInitialized = FALSE;

VOID
VEAPI
EngFillRect32(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X,
    IN LONG Y,
    IN ULONG Width,
    IN ULONG Height,
    IN ULONG ColorARGB
)
/*++

Routine Description:

    This routine fills a rectangular region on the target 32-bit-per-pixel
    surface with a solid ARGB color, clipping the rectangle against the
    surface bounds.

Arguments:

    Surface - Supplies a pointer to the destination GDI surface.

    X - Supplies the starting horizontal coordinate, in pixels, of the
        rectangle.

    Y - Supplies the starting vertical coordinate, in pixels, of the
        rectangle.

    Width - Supplies the width, in pixels, of the rectangle to fill.

    Height - Supplies the height, in pixels, of the rectangle to fill.

    ColorARGB - Supplies the 32-bit ARGB fill color value.

Return Value:

    None.

--*/
{
    if (!Surface || !Surface->Scan0 || Width == 0 || Height == 0) return;

    if (X < 0)
    {
        if ((LONG)Width + X <= 0) return;
        Width = (ULONG)((LONG)Width + X);
        X = 0;
    }
    if (Y < 0)
    {
        if ((LONG)Height + Y <= 0) return;
        Height = (ULONG)((LONG)Height + Y);
        Y = 0;
    }

    if (X >= (LONG)Surface->Width || Y >= (LONG)Surface->Height) return;

    if (X + (LONG)Width > (LONG)Surface->Width)   Width  = Surface->Width - (ULONG)X;
    if (Y + (LONG)Height > (LONG)Surface->Height) Height = Surface->Height - (ULONG)Y;

    for (ULONG Row = 0; Row < Height; Row++)
    {
        PULONG PixelRow = (PULONG)((PUCHAR)Surface->Scan0 + (((ULONG)Y + Row) * Surface->Pitch));
        for (ULONG Col = 0; Col < Width; Col++)
        {
            PixelRow[(ULONG)X + Col] = ColorARGB;
        }
    }
}

VOID
VEAPI
EngSetPixel32(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X,
    IN LONG Y,
    IN ULONG ColorARGB
)
/*++

Routine Description:

    This routine sets the 32-bit ARGB color of a single pixel on the target
    surface, ignoring coordinates that fall outside the surface bounds.

Arguments:

    Surface - Supplies a pointer to the destination GDI surface.

    X - Supplies the horizontal coordinate, in pixels, of the pixel.

    Y - Supplies the vertical coordinate, in pixels, of the pixel.

    ColorARGB - Supplies the 32-bit ARGB color value to write.

Return Value:

    None.

--*/
{
    if (!Surface || !Surface->Scan0) return;
    if (X < 0 || Y < 0 || X >= (LONG)Surface->Width || Y >= (LONG)Surface->Height) return;

    PULONG PixelRow = (PULONG)((PUCHAR)Surface->Scan0 + (Y * Surface->Pitch));
    PixelRow[X] = ColorARGB;
}

ULONG
VEAPI
EngGetPixel32(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X,
    IN LONG Y
)
/*++

Routine Description:

    This routine retrieves the 32-bit ARGB color value of a single pixel from
    the target surface.

Arguments:

    Surface - Supplies a pointer to the source GDI surface.

    X - Supplies the horizontal coordinate, in pixels, of the pixel.

    Y - Supplies the vertical coordinate, in pixels, of the pixel.

Return Value:

    ULONG - Returns the 32-bit ARGB color value at the specified coordinates,
            or 0 if the surface is invalid or the coordinates are out of
            bounds.

--*/
{
    if (!Surface || !Surface->Scan0) return 0;
    if (X < 0 || Y < 0 || X >= (LONG)Surface->Width || Y >= (LONG)Surface->Height) return 0;

    PULONG PixelRow = (PULONG)((PUCHAR)Surface->Scan0 + (Y * Surface->Pitch));
    return PixelRow[X];
}

VOID
VEAPI
EngLineTo32(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X1,
    IN LONG Y1,
    IN LONG X2,
    IN LONG Y2,
    IN ULONG ColorARGB
)
/*++

Routine Description:

    This routine draws a 32-bit-per-pixel solid line between two endpoints on
    the target surface, using fast paths for horizontal and vertical lines and
    Bresenham's algorithm for diagonal lines, with bounds clipping.

Arguments:

    Surface - Supplies a pointer to the destination GDI surface.

    X1 - Supplies the starting horizontal coordinate, in pixels.

    Y1 - Supplies the starting vertical coordinate, in pixels.

    X2 - Supplies the ending horizontal coordinate, in pixels.

    Y2 - Supplies the ending vertical coordinate, in pixels.

    ColorARGB - Supplies the 32-bit ARGB color value used to draw the line.

Return Value:

    None.

--*/
{
    if (!Surface || !Surface->Scan0) return;

    /* Fast-path garis horizontal (sering dipakai border atas/bawah kontrol) */
    if (Y1 == Y2)
    {
        if (Y1 < 0 || Y1 >= (LONG)Surface->Height) return;
        LONG StartX = (X1 < X2) ? X1 : X2;
        LONG EndX   = (X1 < X2) ? X2 : X1;
        if (StartX < 0) StartX = 0;
        if (EndX >= (LONG)Surface->Width) EndX = (LONG)Surface->Width - 1;

        PULONG PixelRow = (PULONG)((PUCHAR)Surface->Scan0 + (Y1 * Surface->Pitch));
        for (LONG x = StartX; x <= EndX; x++)
        {
            PixelRow[x] = ColorARGB;
        }
        return;
    }

    /* Fast-path garis vertikal (sering dipakai border kiri/kanan kontrol & caret) */
    if (X1 == X2)
    {
        if (X1 < 0 || X1 >= (LONG)Surface->Width) return;
        LONG StartY = (Y1 < Y2) ? Y1 : Y2;
        LONG EndY   = (Y1 < Y2) ? Y2 : Y1;
        if (StartY < 0) StartY = 0;
        if (EndY >= (LONG)Surface->Height) EndY = (LONG)Surface->Height - 1;

        for (LONG y = StartY; y <= EndY; y++)
        {
            PULONG PixelRow = (PULONG)((PUCHAR)Surface->Scan0 + (y * Surface->Pitch));
            PixelRow[X1] = ColorARGB;
        }
        return;
    }

    /* Algoritma Bresenham untuk garis diagonal */
    LONG Dx  = (X2 > X1) ? (X2 - X1) : (X1 - X2);
    LONG Dy  = (Y2 > Y1) ? (Y2 - Y1) : (Y1 - Y2);
    LONG Sx  = (X1 < X2) ? 1 : -1;
    LONG Sy  = (Y1 < Y2) ? 1 : -1;
    LONG Err = Dx - Dy;

    while (TRUE)
    {
        EngSetPixel32(Surface, X1, Y1, ColorARGB);
        if (X1 == X2 && Y1 == Y2) break;

        LONG E2 = 2 * Err;
        if (E2 > -Dy)
        {
            Err -= Dy;
            X1  += Sx;
        }
        if (E2 < Dx)
        {
            Err += Dx;
            Y1  += Sy;
        }
    }
}

VOID
VEAPI
EngInvertRect32(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X,
    IN LONG Y,
    IN ULONG Width,
    IN ULONG Height
)
/*++

Routine Description:

    This routine inverts the 24-bit RGB color components of every pixel within
    a rectangular region on the target surface while preserving each pixel's
    alpha channel, clipping the region to the surface bounds.

Arguments:

    Surface - Supplies a pointer to the target GDI surface.

    X - Supplies the starting horizontal coordinate, in pixels, of the
        rectangle.

    Y - Supplies the starting vertical coordinate, in pixels, of the
        rectangle.

    Width - Supplies the width, in pixels, of the rectangle to invert.

    Height - Supplies the height, in pixels, of the rectangle to invert.

Return Value:

    None.

--*/
{
    if (!Surface || !Surface->Scan0) return;
    if (X < 0) { if ((LONG)Width + X <= 0) return; Width += X; X = 0; }
    if (Y < 0) { if ((LONG)Height + Y <= 0) return; Height += Y; Y = 0; }
    if (X >= (LONG)Surface->Width || Y >= (LONG)Surface->Height) return;

    if (X + (LONG)Width > (LONG)Surface->Width)   Width  = Surface->Width - X;
    if (Y + (LONG)Height > (LONG)Surface->Height) Height = Surface->Height - Y;

    for (ULONG Row = 0; Row < Height; Row++)
    {
        PULONG PixelRow = (PULONG)((PUCHAR)Surface->Scan0 + ((Y + Row) * Surface->Pitch));
        for (ULONG Col = 0; Col < Width; Col++)
        {
            PixelRow[X + Col] ^= 0x00FFFFFF;
        }
    }
}

VOID
VEAPI
EngGradientFill32(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X,
    IN LONG Y,
    IN ULONG Width,
    IN ULONG Height,
    IN ULONG ColorStartARGB,
    IN ULONG ColorEndARGB,
    IN BOOLEAN Vertical
)
/*++

Routine Description:

    This routine fills a rectangular region on the target surface with a
    linear 32-bit ARGB color gradient interpolated either vertically or
    horizontally, clipping pixels that fall outside the surface bounds.

Arguments:

    Surface - Supplies a pointer to the destination GDI surface.

    X - Supplies the starting horizontal coordinate, in pixels, of the
        rectangle.

    Y - Supplies the starting vertical coordinate, in pixels, of the
        rectangle.

    Width - Supplies the width, in pixels, of the rectangle to fill.

    Height - Supplies the height, in pixels, of the rectangle to fill.

    ColorStartARGB - Supplies the starting 32-bit ARGB color value (top edge
                     for vertical gradients, left edge for horizontal).

    ColorEndARGB - Supplies the ending 32-bit ARGB color value (bottom edge
                   for vertical gradients, right edge for horizontal).

    Vertical - Supplies TRUE to interpolate the gradient from top to bottom,
               or FALSE to interpolate from left to right.

Return Value:

    None.

--*/
{
    if (!Surface || !Surface->Scan0 || Width == 0 || Height == 0) return;

    LONG A1 = (ColorStartARGB >> 24) & 0xFF, R1 = (ColorStartARGB >> 16) & 0xFF;
    LONG G1 = (ColorStartARGB >> 8)  & 0xFF, B1 = (ColorStartARGB)       & 0xFF;

    LONG A2 = (ColorEndARGB >> 24) & 0xFF, R2 = (ColorEndARGB >> 16) & 0xFF;
    LONG G2 = (ColorEndARGB >> 8)  & 0xFF, B2 = (ColorEndARGB)       & 0xFF;

    if (Vertical)
    {
        ULONG Denom = (Height > 1) ? (Height - 1) : 1;
        for (ULONG Row = 0; Row < Height; Row++)
        {
            LONG Py = Y + (LONG)Row;
            if (Py < 0 || Py >= (LONG)Surface->Height) continue;

            ULONG A = (ULONG)(A1 + ((A2 - A1) * (LONG)Row) / (LONG)Denom);
            ULONG R = (ULONG)(R1 + ((R2 - R1) * (LONG)Row) / (LONG)Denom);
            ULONG G = (ULONG)(G1 + ((G2 - G1) * (LONG)Row) / (LONG)Denom);
            ULONG B = (ULONG)(B1 + ((B2 - B1) * (LONG)Row) / (LONG)Denom);
            ULONG RowColor = (A << 24) | (R << 16) | (G << 8) | B;

            PULONG PixelRow = (PULONG)((PUCHAR)Surface->Scan0 + (Py * Surface->Pitch));
            for (ULONG Col = 0; Col < Width; Col++)
            {
                LONG Px = X + (LONG)Col;
                if (Px >= 0 && Px < (LONG)Surface->Width)
                {
                    PixelRow[Px] = RowColor;
                }
            }
        }
    }
    else
    {
        ULONG Denom = (Width > 1) ? (Width - 1) : 1;
        for (ULONG Col = 0; Col < Width; Col++)
        {
            LONG Px = X + (LONG)Col;
            if (Px < 0 || Px >= (LONG)Surface->Width) continue;

            ULONG A = (ULONG)(A1 + ((A2 - A1) * (LONG)Col) / (LONG)Denom);
            ULONG R = (ULONG)(R1 + ((R2 - R1) * (LONG)Col) / (LONG)Denom);
            ULONG G = (ULONG)(G1 + ((G2 - G1) * (LONG)Col) / (LONG)Denom);
            ULONG B = (ULONG)(B1 + ((B2 - B1) * (LONG)Col) / (LONG)Denom);
            ULONG ColColor = (A << 24) | (R << 16) | (G << 8) | B;

            for (ULONG Row = 0; Row < Height; Row++)
            {
                LONG Py = Y + (LONG)Row;
                if (Py >= 0 && Py < (LONG)Surface->Height)
                {
                    PULONG PixelRow = (PULONG)((PUCHAR)Surface->Scan0 + (Py * Surface->Pitch));
                    PixelRow[Px] = ColColor;
                }
            }
        }
    }
}

VOID
VEAPI
EngPresentRect(
    IN LONG X,
    IN LONG Y,
    IN ULONG Width,
    IN ULONG Height
)
/*++

Routine Description:

    This routine presents a rectangular region from the primary display's back
    buffer to the primary surface and composites the cursor over the updated
    region.

Arguments:

    X - Supplies the starting horizontal coordinate, in pixels, of the region
        to present.

    Y - Supplies the starting vertical coordinate, in pixels, of the region
        to present.

    Width - Supplies the width, in pixels, of the region to present.

    Height - Supplies the height, in pixels, of the region to present.

Return Value:

    None.

--*/
{
    if (!GpPrimaryDisplay.IsInitialized || !GpPrimaryDisplay.BackBuffer)
    {
        return;
    }

    EngBitBlt32(
        &GpPrimaryDisplay.PrimarySurface,
        X, Y, Width, Height,
        GpPrimaryDisplay.BackBuffer,
        X, Y
    );

    CurCompositeToPrimaryRect(X, Y, Width, Height);
}

VOID
VEAPI
EngFillDesktopRect(
    IN LONG  X,
    IN LONG  Y,
    IN ULONG Width,
    IN ULONG Height,
    IN ULONG ColorARGB
)
/*++

Routine Description:

    This routine fills a rectangular region on the current desktop target
    surface with a solid 32-bit ARGB color, temporarily hiding the cursor
    during the draw and presenting the updated region if rendering to an
    off-screen target.

Arguments:

    X - Supplies the starting horizontal coordinate, in pixels, of the
        rectangle.

    Y - Supplies the starting vertical coordinate, in pixels, of the
        rectangle.

    Width - Supplies the width, in pixels, of the rectangle to fill.

    Height - Supplies the height, in pixels, of the rectangle to fill.

    ColorARGB - Supplies the 32-bit ARGB fill color value.

Return Value:

    None.

--*/
{
    PVEAGDI_SURFACE Target = EngGetTargetSurface();
    if (!Target || !Target->Scan0 || Width == 0 || Height == 0)
    {
        return;
    }

    EngHidePointer();
    EngFillRect32(Target, X, Y, Width, Height, ColorARGB);
    EngShowPointer();

    if (Target != &GpPrimaryDisplay.PrimarySurface)
    {
        EngPresentRect(X, Y, Width, Height);
    }
}

VOID
VEAPI
EngBitBlt32(
    IN PVEAGDI_SURFACE DstSurface,
    IN LONG DstX,
    IN LONG DstY,
    IN ULONG Width,
    IN ULONG Height,
    IN PVEAGDI_SURFACE SrcSurface,
    IN LONG SrcX,
    IN LONG SrcY
)
/*++

Routine Description:

    This routine performs a 32-bit-per-pixel bit-block transfer from a source
    surface to a destination surface, clipping the transfer rectangle against
    both surfaces and safely handling overlapping regions within the same
    surface.

Arguments:

    DstSurface - Supplies a pointer to the destination GDI surface.

    DstX - Supplies the destination horizontal coordinate, in pixels.

    DstY - Supplies the destination vertical coordinate, in pixels.

    Width - Supplies the width, in pixels, of the region to copy.

    Height - Supplies the height, in pixels, of the region to copy.

    SrcSurface - Supplies a pointer to the source GDI surface.

    SrcX - Supplies the source horizontal coordinate, in pixels.

    SrcY - Supplies the source vertical coordinate, in pixels.

Return Value:

    None.

--*/
{
    if (!DstSurface || !DstSurface->Scan0 || !SrcSurface || !SrcSurface->Scan0) return;

    /* Clipping sisi kiri/atas */
    if (DstX < 0) { SrcX -= DstX; Width  += DstX; DstX = 0; }
    if (DstY < 0) { SrcY -= DstY; Height += DstY; DstY = 0; }
    if (SrcX < 0) { DstX -= SrcX; Width  += SrcX; SrcX = 0; }
    if (SrcY < 0) { DstY -= SrcY; Height += SrcY; SrcY = 0; }

    if ((LONG)Width <= 0 || (LONG)Height <= 0) return;
    if (DstX >= (LONG)DstSurface->Width || DstY >= (LONG)DstSurface->Height) return;
    if (SrcX >= (LONG)SrcSurface->Width || SrcY >= (LONG)SrcSurface->Height) return;

    /* Clipping sisi kanan/bawah */
    if (DstX + (LONG)Width > (LONG)DstSurface->Width)   Width  = DstSurface->Width - DstX;
    if (DstY + (LONG)Height > (LONG)DstSurface->Height) Height = DstSurface->Height - DstY;
    if (SrcX + (LONG)Width > (LONG)SrcSurface->Width)   Width  = SrcSurface->Width - SrcX;
    if (SrcY + (LONG)Height > (LONG)SrcSurface->Height) Height = SrcSurface->Height - SrcY;

    SIZE_T RowBytes = Width * sizeof(ULONG);

    /* Jika menyalin di dalam surface yang sama ke arah bawah (misal scroll), salin dari bawah ke atas */
    if (DstSurface->Scan0 == SrcSurface->Scan0 && DstY > SrcY)
    {
        for (LONG Row = (LONG)Height - 1; Row >= 0; Row--)
        {
            PULONG DstRow = (PULONG)((PUCHAR)DstSurface->Scan0 + ((DstY + Row) * DstSurface->Pitch)) + DstX;
            PULONG SrcRow = (PULONG)((PUCHAR)SrcSurface->Scan0 + ((SrcY + Row) * SrcSurface->Pitch)) + SrcX;
            RtlMoveMemory(DstRow, SrcRow, RowBytes);
        }
    }
    else
    {
        for (ULONG Row = 0; Row < Height; Row++)
        {
            PULONG DstRow = (PULONG)((PUCHAR)DstSurface->Scan0 + ((DstY + Row) * DstSurface->Pitch)) + DstX;
            PULONG SrcRow = (PULONG)((PUCHAR)SrcSurface->Scan0 + ((SrcY + Row) * SrcSurface->Pitch)) + SrcX;
            RtlMoveMemory(DstRow, SrcRow, RowBytes);
        }
    }
}

VOID
VEAPI
EngTransparentBlt32(
    IN PVEAGDI_SURFACE DstSurface,
    IN LONG DstX,
    IN LONG DstY,
    IN ULONG Width,
    IN ULONG Height,
    IN PVEAGDI_SURFACE SrcSurface,
    IN LONG SrcX,
    IN LONG SrcY,
    IN ULONG TransparentColorRGB
)
/*++

Routine Description:

    This routine performs a 32-bit-per-pixel color-keyed bit-block transfer
    from a source surface to a destination surface with bounds clipping,
    omitting source pixels whose 24-bit RGB value matches the specified
    transparent color key.

Arguments:

    DstSurface - Supplies a pointer to the destination GDI surface.

    DstX - Supplies the destination horizontal coordinate, in pixels.

    DstY - Supplies the destination vertical coordinate, in pixels.

    Width - Supplies the width, in pixels, of the region to transfer.

    Height - Supplies the height, in pixels, of the region to transfer.

    SrcSurface - Supplies a pointer to the source GDI surface.

    SrcX - Supplies the source horizontal coordinate, in pixels.

    SrcY - Supplies the source vertical coordinate, in pixels.

    TransparentColorRGB - Supplies the 24-bit RGB color key value to treat as
                          transparent (upper 8 bits are ignored).

Return Value:

    None.

--*/
{
    if (!DstSurface || !DstSurface->Scan0 || !SrcSurface || !SrcSurface->Scan0) return;

    ULONG KeyRGB = TransparentColorRGB & 0x00FFFFFF;

    for (ULONG Row = 0; Row < Height; Row++)
    {
        LONG Dy = DstY + (LONG)Row;
        LONG Sy = SrcY + (LONG)Row;
        if (Dy < 0 || Dy >= (LONG)DstSurface->Height || Sy < 0 || Sy >= (LONG)SrcSurface->Height) continue;

        PULONG DstRow = (PULONG)((PUCHAR)DstSurface->Scan0 + (Dy * DstSurface->Pitch));
        PULONG SrcRow = (PULONG)((PUCHAR)SrcSurface->Scan0 + (Sy * SrcSurface->Pitch));

        for (ULONG Col = 0; Col < Width; Col++)
        {
            LONG Dx = DstX + (LONG)Col;
            LONG Sx = SrcX + (LONG)Col;
            if (Dx < 0 || Dx >= (LONG)DstSurface->Width || Sx < 0 || Sx >= (LONG)SrcSurface->Width) continue;

            ULONG Pixel = SrcRow[Sx];
            if ((Pixel & 0x00FFFFFF) != KeyRGB)
            {
                DstRow[Dx] = Pixel;
            }
        }
    }
}

VOID
VEAPI
EngAlphaBlend32(
    IN PVEAGDI_SURFACE DstSurface,
    IN LONG DstX,
    IN LONG DstY,
    IN ULONG Width,
    IN ULONG Height,
    IN PVEAGDI_SURFACE SrcSurface,
    IN LONG SrcX,
    IN LONG SrcY,
    IN UCHAR ConstantAlpha
)
/*++

Routine Description:

    This routine performs a 32-bit-per-pixel alpha-blended bit-block transfer
    from a source surface to a destination surface with bounds clipping,
    modulating each source pixel's alpha channel by a constant opacity factor.

Arguments:

    DstSurface - Supplies a pointer to the destination GDI surface.

    DstX - Supplies the destination horizontal coordinate, in pixels.

    DstY - Supplies the destination vertical coordinate, in pixels.

    Width - Supplies the width, in pixels, of the region to blend.

    Height - Supplies the height, in pixels, of the region to blend.

    SrcSurface - Supplies a pointer to the source GDI surface.

    SrcX - Supplies the source horizontal coordinate, in pixels.

    SrcY - Supplies the source vertical coordinate, in pixels.

    ConstantAlpha - Supplies the global opacity multiplier (0 for fully
                    transparent, 255 for full source per-pixel alpha).

Return Value:

    None.

--*/
{
    if (!DstSurface || !DstSurface->Scan0 || !SrcSurface || !SrcSurface->Scan0) return;
    if (ConstantAlpha == 0) return;

    for (ULONG Row = 0; Row < Height; Row++)
    {
        LONG Dy = DstY + (LONG)Row;
        LONG Sy = SrcY + (LONG)Row;
        if (Dy < 0 || Dy >= (LONG)DstSurface->Height || Sy < 0 || Sy >= (LONG)SrcSurface->Height) continue;

        PULONG DstRow = (PULONG)((PUCHAR)DstSurface->Scan0 + (Dy * DstSurface->Pitch));
        PULONG SrcRow = (PULONG)((PUCHAR)SrcSurface->Scan0 + (Sy * SrcSurface->Pitch));

        for (ULONG Col = 0; Col < Width; Col++)
        {
            LONG Dx = DstX + (LONG)Col;
            LONG Sx = SrcX + (LONG)Col;
            if (Dx < 0 || Dx >= (LONG)DstSurface->Width || Sx < 0 || Sx >= (LONG)SrcSurface->Width) continue;

            ULONG SrcPx = SrcRow[Sx];
            ULONG SrcA  = (SrcPx >> 24) & 0xFF;

            /* Gabungkan per-pixel alpha dengan ConstantAlpha */
            ULONG Alpha = (SrcA * (ULONG)ConstantAlpha + 127) / 255;
            if (Alpha == 0) continue;

            if (Alpha == 255)
            {
                DstRow[Dx] = SrcPx | 0xFF000000;
                continue;
            }

            ULONG DstPx = DstRow[Dx];
            ULONG InvA  = 255 - Alpha;

            ULONG SrcR = (SrcPx >> 16) & 0xFF;
            ULONG SrcG = (SrcPx >> 8)  & 0xFF;
            ULONG SrcB = (SrcPx)       & 0xFF;

            ULONG DstR = (DstPx >> 16) & 0xFF;
            ULONG DstG = (DstPx >> 8)  & 0xFF;
            ULONG DstB = (DstPx)       & 0xFF;

            /* Fast exact divide-by-255 tanpa divisi CPU */
            ULONG OutR = (SrcR * Alpha + DstR * InvA);
            OutR = (OutR + 1 + (OutR >> 8)) >> 8;

            ULONG OutG = (SrcG * Alpha + DstG * InvA);
            OutG = (OutG + 1 + (OutG >> 8)) >> 8;

            ULONG OutB = (SrcB * Alpha + DstB * InvA);
            OutB = (OutB + 1 + (OutB >> 8)) >> 8;

            DstRow[Dx] = 0xFF000000 | (OutR << 16) | (OutG << 8) | OutB;
        }
    }
}

VOID
VEAPI
EngDrawGlyph8x16(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X,
    IN LONG Y,
    IN CONST UCHAR GlyphBitmap[16],
    IN ULONG TextColorARGB,
    IN ULONG BkColorARGB,
    IN BOOLEAN TransparentBk
)
/*++

Routine Description:

    This routine renders a 1-bit-per-pixel 8x16 monochrome glyph bitmap onto
    the target surface at the specified coordinates, clipping pixels that fall
    outside the surface bounds.

Arguments:

    Surface - Supplies a pointer to the destination GDI surface.

    X - Supplies the starting horizontal coordinate, in pixels, on the surface.

    Y - Supplies the starting vertical coordinate, in pixels, on the surface.

    GlyphBitmap - Supplies a 16-byte array representing the 8x16 glyph rows,
                  ordered from most significant bit (left) to least significant
                  bit (right).

    TextColorARGB - Supplies the 32-bit ARGB color value used for set glyph
                    pixels.

    BkColorARGB - Supplies the 32-bit ARGB color value used for unset glyph
                  pixels when TransparentBk is FALSE.

    TransparentBk - Supplies TRUE to leave background pixels untouched, or
                    FALSE to fill unset glyph pixels with BkColorARGB.

Return Value:

    None.

--*/
{
    if (!Surface || !Surface->Scan0 || !GlyphBitmap) return;

    for (ULONG Row = 0; Row < 16; Row++)
    {
        LONG Py = Y + (LONG)Row;
        if (Py < 0 || Py >= (LONG)Surface->Height) continue;

        UCHAR RowBits = GlyphBitmap[Row];
        PULONG PixelRow = (PULONG)((PUCHAR)Surface->Scan0 + (Py * Surface->Pitch));

        for (ULONG Col = 0; Col < 8; Col++)
        {
            LONG Px = X + (LONG)Col;
            if (Px < 0 || Px >= (LONG)Surface->Width) continue;

            if (RowBits & (0x80 >> Col))
            {
                PixelRow[Px] = TextColorARGB;
            }
            else if (!TransparentBk)
            {
                PixelRow[Px] = BkColorARGB;
            }
        }
    }
}

VOID
VEAPI
EngDrawGlyphAlpha8(
    IN PVEAGDI_SURFACE Surface,
    IN LONG X,
    IN LONG Y,
    IN ULONG Width,
    IN ULONG Height,
    IN LONG Pitch,
    IN CONST UCHAR *AlphaBitmap,
    IN ULONG TextColorARGB
)
/*++

Routine Description:

    This routine renders an 8-bit alpha-blended glyph bitmap onto the target
    surface at the specified coordinates, blending the text color over the
    existing surface pixels with bounds clipping.

Arguments:

    Surface - Supplies a pointer to the destination GDI surface.

    X - Supplies the starting horizontal coordinate, in pixels, on the surface.

    Y - Supplies the starting vertical coordinate, in pixels, on the surface.

    Width - Supplies the width, in pixels, of the alpha bitmap.

    Height - Supplies the height, in pixels, of the alpha bitmap.

    Pitch - Supplies the stride, in bytes, between consecutive rows in the
            alpha bitmap.

    AlphaBitmap - Supplies a pointer to the 8-bit-per-pixel alpha mask buffer.

    TextColorARGB - Supplies the 32-bit ARGB foreground color value to blend
                    using the alpha mask.

Return Value:

    None.

--*/
{
    if (!Surface || !Surface->Scan0 || !AlphaBitmap || Width == 0 || Height == 0) return;

    ULONG SrcR = (TextColorARGB >> 16) & 0xFF;
    ULONG SrcG = (TextColorARGB >> 8)  & 0xFF;
    ULONG SrcB = (TextColorARGB)       & 0xFF;

    for (ULONG Row = 0; Row < Height; Row++)
    {
        LONG Py = Y + (LONG)Row;
        if (Py < 0 || Py >= (LONG)Surface->Height) continue;

        CONST UCHAR *AlphaRow = AlphaBitmap + ((LONG)Row * Pitch);
        PULONG PixelRow = (PULONG)((PUCHAR)Surface->Scan0 + (Py * Surface->Pitch));

        for (ULONG Col = 0; Col < Width; Col++)
        {
            LONG Px = X + (LONG)Col;
            if (Px < 0 || Px >= (LONG)Surface->Width) continue;

            ULONG Alpha = AlphaRow[Col];
            if (Alpha == 0) continue;

            if (Alpha == 255)
            {
                PixelRow[Px] = 0xFF000000 | (SrcR << 16) | (SrcG << 8) | SrcB;
                continue;
            }

            ULONG DstPx = PixelRow[Px];
            ULONG InvA  = 255 - Alpha;

            ULONG DstR = (DstPx >> 16) & 0xFF;
            ULONG DstG = (DstPx >> 8)  & 0xFF;
            ULONG DstB = (DstPx)       & 0xFF;

            ULONG OutR = (SrcR * Alpha + DstR * InvA);
            OutR = (OutR + 1 + (OutR >> 8)) >> 8;

            ULONG OutG = (SrcG * Alpha + DstG * InvA);
            OutG = (OutG + 1 + (OutG >> 8)) >> 8;

            ULONG OutB = (SrcB * Alpha + DstB * InvA);
            OutB = (OutB + 1 + (OutB >> 8)) >> 8;

            PixelRow[Px] = 0xFF000000 | (OutR << 16) | (OutG << 8) | OutB;
        }
    }
}

static
VEASTATUS
EngSendVideoIoctl(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG IoControlCode,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputLength
)
/*++

Routine Description:

    This routine builds and sends a synchronous device I/O control (IOCTL)
    request to the specified video device object and waits for its completion.

Arguments:

    DeviceObject - Supplies a pointer to the target video device object.

    IoControlCode - Supplies the I/O control code specifying the operation
                    to be performed by the device driver.

    InputBuffer - Supplies an optional pointer to the buffer containing
                  input data for the request.

    InputLength - Supplies the length, in bytes, of the input buffer.

    OutputBuffer - Supplies an optional pointer to the buffer that receives
                   output data from the device driver.

    OutputLength - Supplies the length, in bytes, of the output buffer.

Return Value:

    VEASTATUS - Returns STATUS_INSUFFICIENT_RESOURCES if the OIP could not
                be allocated; otherwise returns the final completion status
                from the I/O status block.

--*/
{
    POIP Oip;
    KEVENT Event;
    OIP_INFORMATION IoStatus;
    VEASTATUS Status;

    RtlZeroMemory(&IoStatus, sizeof(IoStatus));
    KsInitializeEvent(&Event, NotificationEvent, FALSE);

    Oip = ObioBuildDeviceIoControlRequest(
        IoControlCode,
        DeviceObject,
        InputBuffer,
        InputLength,
        OutputBuffer,
        OutputLength,
        FALSE,
        &Event,
        &IoStatus
    );
    if (!Oip)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = ObioCallObject(DeviceObject, Oip);
    if (Status == STATUS_PENDING)
    {
        KsWaitForSingleObject(&Event);
    }

    return IoStatus.Status;
}

VEASTATUS
WINAPI
InitVideo(VOID)
{
    VEASTATUS Status;
    ANSI_STRING DisplayName = RTL_CONSTANT_ANSI_STRING("/Device/Display0");
    VIDEO_MEMORY VideoMemReq;

    Status = ObioGetDeviceObjectPointer(
        &DisplayName,
        FILE_READ_DATA | FILE_WRITE_DATA,
        &GpPrimaryDisplay.DisplayFileObject,
        &GpPrimaryDisplay.DisplayDeviceObject
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[VEAPI] fail to open /Device/Display0: 0x%08X\n", Status);
        return Status;
    }

    Status = EngSendVideoIoctl(
        GpPrimaryDisplay.DisplayDeviceObject,
        IOCTL_VIDEO_QUERY_CURRENT_MODE,
        NULL,
        0,
        &GpPrimaryDisplay.ModeInfo,
        sizeof(VIDEO_MODE_INFORMATION)
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[VEAPI] IOCTL_VIDEO_QUERY_CURRENT_MODE failed: 0x%08X\n", Status);
        return Status;
    }

    RtlZeroMemory(&VideoMemReq, sizeof(VideoMemReq));
    Status = EngSendVideoIoctl(
        GpPrimaryDisplay.DisplayDeviceObject,
        IOCTL_VIDEO_MAP_VIDEO_MEMORY,
        &VideoMemReq,
        sizeof(VIDEO_MEMORY),
        &GpPrimaryDisplay.MemoryInfo,
        sizeof(VIDEO_MEMORY_INFORMATION)
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[VEAPI] IOCTL_VIDEO_MAP_VIDEO_MEMORY failed: 0x%08X\n", Status);
        return Status;
    }

    GpPrimaryDisplay.PrimarySurface.Width         = GpPrimaryDisplay.ModeInfo.VisScreenWidth;
    GpPrimaryDisplay.PrimarySurface.Height        = GpPrimaryDisplay.ModeInfo.VisScreenHeight;
    GpPrimaryDisplay.PrimarySurface.Pitch         = GpPrimaryDisplay.ModeInfo.ScreenStride;
    GpPrimaryDisplay.PrimarySurface.BitsPerPixel  = GpPrimaryDisplay.ModeInfo.BitsPerPlane;
    GpPrimaryDisplay.PrimarySurface.BytesPerPixel = GpPrimaryDisplay.ModeInfo.BitsPerPlane / 8;
    GpPrimaryDisplay.PrimarySurface.Scan0         = GpPrimaryDisplay.MemoryInfo.FrameBufferBase;
    GpPrimaryDisplay.PrimarySurface.BufferSize    = GpPrimaryDisplay.MemoryInfo.FrameBufferLength;
    GpPrimaryDisplay.PrimarySurface.IsPrimary     = TRUE;
    GpPrimaryDisplay.PrimarySurface.RedMask       = GpPrimaryDisplay.ModeInfo.RedMask;
    GpPrimaryDisplay.PrimarySurface.GreenMask     = GpPrimaryDisplay.ModeInfo.GreenMask;
    GpPrimaryDisplay.PrimarySurface.BlueMask      = GpPrimaryDisplay.ModeInfo.BlueMask;
    GpPrimaryDisplay.CursorX       = (LONG)(GpPrimaryDisplay.PrimarySurface.Width / 2);
    GpPrimaryDisplay.CursorY       = (LONG)(GpPrimaryDisplay.PrimarySurface.Height / 2);
    GpPrimaryDisplay.IsInitialized = TRUE;
    GpPrimaryDisplay.ScreenEnabled = TRUE;

    GpPrimaryDisplay.BackBuffer                   = NULL;

    SIZE_T BackBytes = (SIZE_T)GpPrimaryDisplay.PrimarySurface.Pitch *
                       GpPrimaryDisplay.PrimarySurface.Height;
    PVEAGDI_SURFACE BackSurf = (PVEAGDI_SURFACE)GDI_POOL_ALLOC(sizeof(VEAGDI_SURFACE));
    PVOID BackPixels         = BackSurf ? GDI_POOL_ALLOC(BackBytes) : NULL;

    if (BackSurf && BackPixels)
    {
        RtlZeroMemory(BackPixels, BackBytes);
        *BackSurf             = GpPrimaryDisplay.PrimarySurface;
        BackSurf->Scan0       = BackPixels;
        BackSurf->BufferSize  = BackBytes;
        BackSurf->IsPrimary   = FALSE;
        GpPrimaryDisplay.BackBuffer = BackSurf;
    }
    else
    {
        if (BackSurf) GDI_POOL_FREE(BackSurf);
        DPRINT("[VEAPI] BackBuffer alloc failed, fallback to direct VRAM\n");
    }

    DPRINT("[VEAPI] InitVideo OK: %lux%lu @ %lu bpp, Pitch=%lu, Scan0=%p\n",
           GpPrimaryDisplay.PrimarySurface.Width,
           GpPrimaryDisplay.PrimarySurface.Height,
           GpPrimaryDisplay.PrimarySurface.BitsPerPixel,
           GpPrimaryDisplay.PrimarySurface.Pitch,
           GpPrimaryDisplay.PrimarySurface.Scan0);
    
    GdipInitializeDcManager();
    GdipInitializeGdiObjects();

    return STATUS_SUCCESS;
}

/* EOF */