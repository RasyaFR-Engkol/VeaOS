/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : bv.h
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Header file for Bv
 ----- Effective since 08-2026 til FOREVER  ------------------------------- */

#pragma once

#include "ldrtypes.h"
#include "procbind.h"

/* Revision History ------------------------------------------------------
 * DATE       : 01-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file bvstate.c
 * --------------------------------------------------------------------- */

typedef enum _BV_DISPLAY_STATE
{
    BvDisplayAcquired,
    BvDisplayLost
} BV_DISPLAY_STATE, *PBV_DISPLAY_STATE;

BV_DISPLAY_STATE
VEAPI
BvAcquireDisplayState(BOOLEAN WeAcquired);

VOID
VEAPI
BvInitializeDriver(IN PBLOCK_BOOT_2 BlockBoot2);

extern VIDEO_BOOT BvInfoBlock;
extern ULONG COLOR_BG;
extern ULONG COLOR_TEXT;

BOOLEAN
VEAPI
BvDoWeHaveOwnership(VOID);

VOID
VEAPI
BvpScrollRegion(VOID);

VOID
VEAPI
BvSetColorText(ULONG Color);

VOID
VEAPI
BvSetColorBackground(ULONG Color);

VOID
VEAPI
BvPrintLog(const char *Text);

VOID
VEAPI
BvpSetScrollRegion(
    IN ULONG Left,
    IN ULONG Top,
    IN ULONG Right,
    IN ULONG Bottom
);

VOID
VEAPI
BvBitBlt(
    IN ULONG DestX,
    IN ULONG DestY,
    IN ULONG Width,
    IN ULONG Height,
    IN const ULONG *SrcBuffer,
    IN ULONG SrcPitch
);

VOID
VEAPI
BvDisplayBootLogo(VOID);

VOID
VEAPI
BvDisplayBootLogoFadeIn(VOID);

VOID
VEAPI
BvClearScreen(VOID);

extern const UCHAR BvLogoBitmap[];
extern const UCHAR BvLogoBitmapEnd[];

typedef struct _BMP_FILE_HEADER {
    USHORT BfType;          // Harus 'BM' (0x4D42)
    ULONG  BfSize;
    USHORT BfReserved1;
    USHORT BfReserved2;
    ULONG  BfOffBits;       // Offset ke data pixel (biasanya 54)
} __attribute__((packed)) BMP_FILE_HEADER, *PBMP_FILE_HEADER;

typedef struct _BMP_INFO_HEADER {
    ULONG  BiSize;          // 40
    LONG   BiWidth;         // Lebar
    LONG   BiHeight;        // Tinggi (Jika positif = Bottom-Up BMP)
    USHORT BiPlanes;
    USHORT BiBitCount;      // Harus 24 atau 32 BPP
    ULONG  BiCompression;   // 0 = Uncompressed (BI_RGB)
    ULONG  BiSizeImage;
    LONG   BiXPelsPerMeter;
    LONG   BiYPelsPerMeter;
    ULONG  BiClrUsed;
    ULONG  BiClrImportant;
} __attribute__((packed)) BMP_INFO_HEADER, *PBMP_INFO_HEADER;