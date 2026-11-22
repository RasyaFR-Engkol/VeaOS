/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : dbg.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Rtl Debug Purpose
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "rtlp.h"
#include "vz_wrappers.h"

/* Revision History ------------------------------------------------------
 * DATE       : 10-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file dbg.c
 * --------------------------------------------------------------------- */

BOOLEAN RtlSerialMode = TRUE;

#if !defined(BUILDING_VEAKRNL)

VOID
KdPrintf(const char *Format, ...)
{
    CHAR Buffer[512];
    va_list Args;

    if (!Format)
    {
        return;
    }

    va_start(Args, Format);
    RtlStringCbVPrintfA(Buffer, sizeof(Buffer), Format, Args);
    va_end(Args);

    VzKdPrint(Buffer);
}

#endif