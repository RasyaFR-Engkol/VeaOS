/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : bvinit.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Initialize BootVideo module
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>
#include "bv.h"

/* Revision History ------------------------------------------------------
 * DATE       : 01-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file bvinit.c
 * --------------------------------------------------------------------- */

/* State */
VIDEO_BOOT BvInfoBlock;
BOOLEAN BvVideoDriverInstalled;

VOID
VEAPI
BvInitializeDriver(IN PBLOCK_BOOT_2 BlockBoot2)
{
    /* Make sure it's exist */
    if(!BlockBoot2)
    {
        return;
    }

    /* it's VBE. and it is currently default VBE that show on screen
       because we init them in Bv LDR, and just give it to kernel  */
    RtlCopyMemory(&BvInfoBlock, &BlockBoot2->VideoBoot, sizeof(VIDEO_BOOT));

    /* Earn it */
    BvAcquireDisplayState(TRUE);

    /* Set our DEFAULT scrolling Region */
    BvpSetScrollRegion(BvInfoBlock.X, BvInfoBlock.Y, BvInfoBlock.W, BvInfoBlock.H);

    /* Setting our BG Color (Default) */
    COLOR_BG = 0xFF000000; // AARRGGBB
    COLOR_TEXT = 0xFF808080; 

    BvVideoDriverInstalled = TRUE;
}

VEASTATUS
VEAPI
BvGetVideoInfo(
    OUT PVOID *FrameBufferBase,
    OUT PULONG Width,
    OUT PULONG Height,
    OUT PULONG Pitch,
    OUT PULONG BitsPerPixel,
    OUT PSIZE_T FrameBufferSize OPTIONAL
)
{
    ULONG ScanlinePitch;

    if (!BvVideoDriverInstalled || !BvInfoBlock.VideoBootAddress)
    {
        return STATUS_DEVICE_NOT_READY;
    }

    if (!FrameBufferBase || !Width || !Height || !Pitch || !BitsPerPixel)
    {
        return STATUS_INVALID_PARAMETER;
    }

    ScanlinePitch = (ULONG)(ULONG_PTR)BvInfoBlock.AdditionalInformation[0];
    if (ScanlinePitch == 0)
    {
        ScanlinePitch = BvInfoBlock.W * 4;
    }

    *FrameBufferBase = BvInfoBlock.VideoBootAddress;
    *Width           = BvInfoBlock.W;
    *Height          = BvInfoBlock.H;
    *Pitch           = ScanlinePitch;
    *BitsPerPixel    = 32;

    if (FrameBufferSize)
    {
        *FrameBufferSize = (SIZE_T)(ScanlinePitch * BvInfoBlock.H);
    }

    return STATUS_SUCCESS;
}

/* EOF */