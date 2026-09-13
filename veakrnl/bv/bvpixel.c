/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : bvpixel.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Putting pixel for Bv module
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>
#include "../../osi386/ldr/font8x16_vgabasic.h"

/* Revision History ------------------------------------------------------
 * DATE       : 01-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file bvpixel.c
 * --------------------------------------------------------------------- */

//
// Bv Macro
//

#define LOG_LINE_HEIGHT   20   // Tinggi baris per log (16px font + 4px padding)

// 
// Static machine variable 
//

static ULONG BvScrollLeft   = 0;
static ULONG BvScrollTop    = 0;
static ULONG BvScrollRight  = 0;
static ULONG BvScrollBottom = 0;

static ULONG BvLogCursorX = 0;
static ULONG BvLogCursorY = 0;

ULONG COLOR_BG = 0;
ULONG COLOR_TEXT = 0;

//
// Font extern
//

extern const UCHAR Font8x16[256][16];

//
// Getting Cursor Position
//
ULONG
VEAPI
BvGetLogCursorY(VOID)
{
    return BvLogCursorY;
}

ULONG
VEAPI
BvGetLogCursorX(VOID)
{
    return BvLogCursorX;
}

static
VOID
VEAPI
BvResetLogCursorState(VOID)
{
    BvLogCursorX = BvScrollLeft;
    BvLogCursorY = BvScrollTop;
}

VOID
VEAPI
BvpPutPixel(
    IN ULONG Xi,
    IN ULONG Y2,
    IN ULONG Color);

static KTIMER BvFadeTimer;
static KDPC BvFadeDpc;
static ULONG BvFadeCurrentStep = 0;
static ULONG BvFadeTotalSteps = 30;
static ULONG BvFadeIntervalMs = 100;
static ULONG BvFadeStartX = 0;
static ULONG BvFadeStartY = 0;
static ULONG BvFadeWidth = 0;
static ULONG BvFadeHeight = 0;
static ULONG BvFadeRowStride = 0;
static USHORT BvFadeBpp = 0;
static BOOLEAN BvFadeBottomUp = TRUE;
static const UCHAR *BvFadePixelData = NULL;
static const UCHAR *BvFadeColorTable = NULL;
static BOOLEAN BvFadeActive = FALSE;

static
ULONG
VEAPI
BvBlendPixelToBackground(
    IN ULONG Foreground,
    IN ULONG Background,
    IN UCHAR Alpha)
{
    ULONG InvAlpha = 255 - Alpha;
    ULONG Fr = (Foreground >> 16) & 0xFF;
    ULONG Fg = (Foreground >> 8) & 0xFF;
    ULONG Fb = Foreground & 0xFF;
    ULONG Br = (Background >> 16) & 0xFF;
    ULONG Bg = (Background >> 8) & 0xFF;
    ULONG Bb = Background & 0xFF;

    ULONG R = (Fr * Alpha + Br * InvAlpha + 127) / 255;
    ULONG G = (Fg * Alpha + Bg * InvAlpha + 127) / 255;
    ULONG B = (Fb * Alpha + Bb * InvAlpha + 127) / 255;

    return (0xFFu << 24) | (R << 16) | (G << 8) | B;
}

static
ULONG
VEAPI
BvGetBmpPixelColor(
    IN const UCHAR *Row,
    IN ULONG X,
    IN USHORT Bpp,
    IN const UCHAR *ColorTable)
{
    if (Bpp == 32) {
        const UCHAR *Pixel = Row + (X * 4);
        return (Pixel[3] << 24) | (Pixel[2] << 16) | (Pixel[1] << 8) | Pixel[0];
    }

    if (Bpp == 24) {
        const UCHAR *Pixel = Row + (X * 3);
        return (0xFFu << 24) | (Pixel[2] << 16) | (Pixel[1] << 8) | Pixel[0];
    }

    if (Bpp == 8) {
        UCHAR Index = Row[X];
        const UCHAR *Entry = ColorTable + (Index * 4);
        return (0xFFu << 24) | (Entry[2] << 16) | (Entry[1] << 8) | Entry[0];
    }

    if (Bpp == 4) {
        UCHAR Byte = Row[X >> 1];
        UCHAR Index = (X & 1) ? (Byte & 0x0F) : (Byte >> 4);
        const UCHAR *Entry = ColorTable + (Index * 4);
        return (0xFFu << 24) | (Entry[2] << 16) | (Entry[1] << 8) | Entry[0];
    }

    return 0xFF000000;
}

static
VOID
VEAPI
BvDrawLogoWithAlpha(IN UCHAR Alpha)
{
    ULONG Background = COLOR_BG;

    if (!BvFadePixelData || BvFadeWidth == 0 || BvFadeHeight == 0) {
        return;
    }

    for (ULONG y = 0; y < BvFadeHeight; y++)
    {
        LONG TargetY = BvFadeBottomUp ? ((LONG)BvFadeHeight - 1 - (LONG)y) : (LONG)y;
        const UCHAR *Row = BvFadePixelData + (TargetY * BvFadeRowStride);

        for (ULONG x = 0; x < BvFadeWidth; x++)
        {
            ULONG Color = BvGetBmpPixelColor(Row, x, BvFadeBpp, BvFadeColorTable);
            BvpPutPixel(BvFadeStartX + x, BvFadeStartY + y,
                BvBlendPixelToBackground(Color, Background, Alpha));
        }
    }
}

static
VOID
VEAPI
BvFadeLogoDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2)
{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(DeferredContext);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    if (!BvDoWeHaveOwnership() || !BvFadeActive) {
        return;
    }

    if (BvFadeCurrentStep < BvFadeTotalSteps)
    {
        BvFadeCurrentStep++;
    }

    UCHAR Alpha = (UCHAR)((BvFadeCurrentStep * 255) / BvFadeTotalSteps);
    BvDrawLogoWithAlpha(Alpha);

    if (BvFadeCurrentStep >= BvFadeTotalSteps)
    {
        KsCancelTimer(&BvFadeTimer);
        BvFadeActive = FALSE;
    }
}

static
VOID
VEAPI
BvStartLogoFade(VOID)
{
    KsInitializeTimer(&BvFadeTimer);
    KsInitializeDpc(&BvFadeDpc, BvFadeLogoDpcRoutine, NULL);

    LARGE_INTEGER DueTime;
    DueTime.QuadPart = -((ULONGLONG)BvFadeIntervalMs * 30000ULL);

    KsSetTimer(&BvFadeTimer, DueTime, (LONG)BvFadeIntervalMs, &BvFadeDpc);
}

//
// Our Heart function. Putting pixel here
//
VOID
VEAPI
BvpPutPixel(
    IN ULONG Xi, 
    IN ULONG Y2, 
    IN ULONG Color)
{
    if (!BvDoWeHaveOwnership()) return;
    if (Xi >= BvInfoBlock.W || Y2 >= BvInfoBlock.H) return;

    // AdditionalInformation[0] berisi BytesPerScanLine (Pitch)
    ULONG Pitch = (ULONG_PTR)BvInfoBlock.AdditionalInformation[0];
    ULONG_PTR PixelAddr = (ULONG_PTR)BvInfoBlock.VideoBootAddress + (Y2 * Pitch) + (Xi * 4);

    *(volatile ULONG*)PixelAddr = Color;
}

VOID
VEAPI
BvFillRect(
    IN ULONG Xi, 
    IN ULONG Y2, 
    IN ULONG W, 
    IN ULONG H, 
    IN ULONG Color
)
{
    for (ULONG y = Y2; y < Y2 + H; y++)
    {
        for (ULONG x = Xi; x < Xi + W; x++)
        {
            BvpPutPixel(x, y, Color);
        }
    }
}

VOID
VEAPI
BvDrawRect(
    IN ULONG Xi, 
    IN ULONG Y2, 
    IN ULONG W, 
    IN ULONG H, 
    IN ULONG Color)
{
    for (ULONG x = Xi; x < Xi + W; x++)
    {
        BvpPutPixel(x, Y2, Color);         // Top Border
        BvpPutPixel(x, Y2 + H - 1, Color); // Bottom Border
    }
    for (ULONG y = Y2; y < Y2 + H; y++)
    {
        BvpPutPixel(Xi, y, Color);         // Left Border
        BvpPutPixel(Xi + W - 1, y, Color); // Right Border
    }
}

//
// Our TEXT Engine. Same used at OSI386
//

VOID
VEAPI
BvpDrawChar(IN ULONG Xi, IN ULONG Y2, IN CHAR C, IN ULONG Color)
{
    UCHAR CharIndex = (UCHAR)C;

    for (ULONG row = 0; row < 16; row++)
    {
        UCHAR Bits = Font8x16[CharIndex][row];
        for (ULONG col = 0; col < 8; col++)
        {
            if (Bits & (0x80 >> col))
            {
                BvpPutPixel(Xi + col, Y2 + row, Color);
            }
        }
    }
}

VOID
VEAPI
BvpDrawString(
    IN ULONG Xi, 
    IN ULONG Y2, 
    IN const char *Text, 
    IN ULONG Color
)
{
    ULONG CurrentX = Xi;
    while (*Text)
    {
        BvpDrawChar(CurrentX, Y2, *Text, Color);
        CurrentX += 8;
        Text++;
    }
}

//
// Setting our scrolling region
//

VOID
VEAPI
BvpSetScrollRegion(
    IN ULONG Left,
    IN ULONG Top,
    IN ULONG Right,
    IN ULONG Bottom
)
{
    if (!BvDoWeHaveOwnership()) return;

    // Boundary Protection (Mencegah out-of-bounds)
    if (Right > BvInfoBlock.W) Right = BvInfoBlock.W;
    if (Bottom > BvInfoBlock.H) Bottom = BvInfoBlock.H;

    BvScrollLeft   = Left;
    BvScrollTop    = Top;
    BvScrollRight  = Right;
    BvScrollBottom = Bottom;

    // Setel kursor log awal tepat di pojok kiri-atas Scroll Region
    BvLogCursorX = Left;
    BvLogCursorY = Top;
}

VOID
VEAPI
BvpScrollRegion(VOID)
{
    if (!BvDoWeHaveOwnership()) return;

    ULONG Pitch = (ULONG_PTR)BvInfoBlock.AdditionalInformation[0];
    ULONG RegionWidthBytes = (BvScrollRight - BvScrollLeft) * 4; // 32 bpp = 4 bytes/pixel

    //
    // 1. Copy baris piksel dari Bawah ke Atas HANYA di dalam Scroll Region
    //
    for (ULONG y = BvScrollTop; y < BvScrollBottom - LOG_LINE_HEIGHT; y++)
    {
        ULONG_PTR DstAddr = (ULONG_PTR)BvInfoBlock.VideoBootAddress + (y * Pitch) + (BvScrollLeft * 4);
        ULONG_PTR SrcAddr = (ULONG_PTR)BvInfoBlock.VideoBootAddress + ((y + LOG_LINE_HEIGHT) * Pitch) + (BvScrollLeft * 4);

        RtlCopyMemory((PVOID)DstAddr, (PVOID)SrcAddr, RegionWidthBytes);
    }

    //
    // 2. Bersihkan baris paling bawah Scroll Region saja (Isi Warna Background Console)
    //
    BvFillRect(
        BvScrollLeft, 
        BvScrollBottom - LOG_LINE_HEIGHT, 
        BvScrollRight - BvScrollLeft, 
        LOG_LINE_HEIGHT, 
        COLOR_BG
    );
}


//
// Used to reserving ROW
//
VOID
VEAPI
BvSetReservedRows(
    IN ULONG TopReservedPixels,
    IN ULONG BottomReservedPixels)
{
    if (!BvDoWeHaveOwnership()) return;

    ULONG LeftMargin  = 35; // Default margin kiri
    ULONG RightMargin = BvInfoBlock.W - 35;

    ULONG Top    = TopReservedPixels;
    ULONG Bottom = BvInfoBlock.H - BottomReservedPixels;

    BvpSetScrollRegion(LeftMargin, Top, RightMargin, Bottom);
}

//
// Printing LOG comes here
//

VOID
VEAPI
BvPrintLog(const char *Text)
{
    if (!BvDoWeHaveOwnership()) return;

    while (*Text)
    {
        // Handle Karakter Newline '\n'
        if (*Text == '\n')
        {
            BvLogCursorX = BvScrollLeft;
            BvLogCursorY += LOG_LINE_HEIGHT;
            Text++;
            continue;
        }

        // Jika Kursor Mencapai Batas Kanan Scroll Region -> Word Wrap
        if (BvLogCursorX + 8 > BvScrollRight)
        {
            BvLogCursorX = BvScrollLeft;
            BvLogCursorY += LOG_LINE_HEIGHT;
        }

        // Jika Kursor Mencapai Batas Bawah Scroll Region (Reserve Row Bawah) -> SCROLL!
        if (BvLogCursorY + LOG_LINE_HEIGHT > BvScrollBottom)
        {
            BvpScrollRegion();
            BvLogCursorY = BvScrollBottom - LOG_LINE_HEIGHT;
        }

        // Draw Karakter tunggal
        BvpDrawChar(BvLogCursorX, BvLogCursorY, *Text, COLOR_TEXT);
        BvLogCursorX += 8;

        Text++;
    }

    // Cek lagi setelah pindah baris
    if (BvLogCursorY + LOG_LINE_HEIGHT > BvScrollBottom)
    {
        BvpScrollRegion();
        BvLogCursorY = BvScrollBottom - LOG_LINE_HEIGHT;
    }
}

//
// Clear our screen by WIPE it
//

VOID
VEAPI
BvClearScreen(VOID)
{
    if (!BvDoWeHaveOwnership()) return;

    ULONG Pitch = (ULONG_PTR)BvInfoBlock.AdditionalInformation[0];
    ULONG Height = BvInfoBlock.H;
    ULONG Width = BvInfoBlock.W;

    // Tulis warna langsung ke memori Framebuffer agar jauh lebih cepat
    for (ULONG y = 0; y < Height; y++)
    {
        // Hitung alamat awal untuk baris ke-y
        ULONG_PTR RowAddr = (ULONG_PTR)BvInfoBlock.VideoBootAddress + (y * Pitch);
        ULONG *Pixel = (ULONG *)RowAddr;
        
        for (ULONG x = 0; x < Width; x++)
        {
            Pixel[x] = COLOR_BG;
        }
    }

    BvResetLogCursorState();
}

//
// Set our BG and COLOR_TEXT
//

VOID
VEAPI
BvSetColorText(ULONG Color)
{
    COLOR_TEXT = Color;
}

VOID
VEAPI
BvSetColorBackground(ULONG Color)
{
    COLOR_BG = Color;
}

VOID
VEAPI
BvBitBlt(
    IN ULONG DestX,
    IN ULONG DestY,
    IN ULONG Width,
    IN ULONG Height,
    IN const ULONG *SrcBuffer,
    IN ULONG SrcPitch
)
{
    if (!BvDoWeHaveOwnership()) return;

    ULONG DstPitch = (ULONG_PTR)BvInfoBlock.AdditionalInformation[0];
    
    // Kliping sederhana biar nggak out-of-bounds
    if (DestX >= BvInfoBlock.W || DestY >= BvInfoBlock.H) return;
    if (DestX + Width > BvInfoBlock.W) Width = BvInfoBlock.W - DestX;
    if (DestY + Height > BvInfoBlock.H) Height = BvInfoBlock.H - DestY;

    for (ULONG y = 0; y < Height; y++)
    {
        ULONG_PTR DstRow = (ULONG_PTR)BvInfoBlock.VideoBootAddress + ((DestY + y) * DstPitch) + (DestX * 4);
        // Hitung baris di source buffer (SrcPitch dalam satuan byte atau piksel? Biasanya byte, jadi bagi 4 jika pointer ULONG)
        // Asumsi SrcPitch dihitung dalam byte:
        const ULONG *SrcRow = (const ULONG *)((const UCHAR*)SrcBuffer + (y * SrcPitch));

        // Copy satu baris penuh menggunakan RtlCopyMemory (lebih cepat daripada loop manual)
        RtlCopyMemory((PVOID)DstRow, (const PVOID)SrcRow, Width * 4);
    }
}

VOID
VEAPI
BvDisplayBootLogoFadeIn(VOID)
{
    if (!BvDoWeHaveOwnership()) return;

    PBMP_FILE_HEADER FileHeader = (PBMP_FILE_HEADER)BvLogoBitmap;
    if (FileHeader->BfType != 0x4D42) {
        DPRINT("BV: Invalid BMP File Magic!\n");
        return;
    }

    PBMP_INFO_HEADER InfoHeader = (PBMP_INFO_HEADER)((ULONG_PTR)BvLogoBitmap + sizeof(BMP_FILE_HEADER));
    LONG Width = InfoHeader->BiWidth;
    LONG Height = InfoHeader->BiHeight;
    USHORT Bpp = InfoHeader->BiBitCount;

    if (Height < 0) {
        Height = -Height;
        BvFadeBottomUp = FALSE;
    } else {
        BvFadeBottomUp = TRUE;
    }

    if ((Bpp != 32 && Bpp != 24 && Bpp != 8 && Bpp != 4) || InfoHeader->BiCompression != 0) {
        DPRINT("BV: Unsupported BMP format (BPP: %u, Compression: %lu)!\n", Bpp, InfoHeader->BiCompression);
        return;
    }

    BvFadeBpp = Bpp;
    BvFadeWidth = (ULONG)Width;
    BvFadeHeight = (ULONG)Height;
    BvFadeRowStride = ((Width * Bpp + 31) / 32) * 4;
    BvFadePixelData = (const UCHAR *)BvLogoBitmap + FileHeader->BfOffBits;
    BvFadeColorTable = (Bpp < 24) ? ((const UCHAR *)BvLogoBitmap + 14 + InfoHeader->BiSize) : NULL;

    BvFadeStartX = (BvInfoBlock.W > (ULONG)Width) ? (BvInfoBlock.W - Width) / 2 : 0;
    BvFadeStartY = (BvInfoBlock.H > (ULONG)Height) ? (BvInfoBlock.H - Height) / 2 : 0;

    BvFadeCurrentStep = 0;
    BvFadeActive = TRUE;

    BvClearScreen();
    BvStartLogoFade();
}