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
    IN ULONG X,
    IN ULONG Y,
    IN ULONG Width,
    IN ULONG Height,
    IN ULONG ColorARGB
)
{
    if (!Surface || !Surface->Scan0) return;
    if (X >= Surface->Width || Y >= Surface->Height) return;

    if (X + Width > Surface->Width)   Width  = Surface->Width - X;
    if (Y + Height > Surface->Height) Height = Surface->Height - Y;

    for (ULONG Row = 0; Row < Height; Row++)
    {
        PULONG PixelRow = (PULONG)((PUCHAR)Surface->Scan0 + ((Y + Row) * Surface->Pitch));
        for (ULONG Col = 0; Col < Width; Col++)
        {
            PixelRow[X + Col] = ColorARGB;
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
            /* Balik komponen RGB (24-bit bawah), pertahankan byte Alpha */
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

    DPRINT("[VEAPI] InitVideo OK: %lux%lu @ %lu bpp, Pitch=%lu, Scan0=%p\n",
           GpPrimaryDisplay.PrimarySurface.Width,
           GpPrimaryDisplay.PrimarySurface.Height,
           GpPrimaryDisplay.PrimarySurface.BitsPerPixel,
           GpPrimaryDisplay.PrimarySurface.Pitch,
           GpPrimaryDisplay.PrimarySurface.Scan0);
    
    GdipInitializeDcManager();

    return STATUS_SUCCESS;
}