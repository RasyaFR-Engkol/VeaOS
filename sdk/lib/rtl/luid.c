/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : luid.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Security LUID Module
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <rtl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 17-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file luid.c
 * --------------------------------------------------------------------- */

BOOLEAN
VEAPI
RtlEqualLuid(
    IN PLUID Luid1,
    IN PLUID Luid2
)
{
    if (!Luid1 || !Luid2) return FALSE;
    return (Luid1->LowPart == Luid2->LowPart && Luid1->HighPart == Luid2->HighPart);
}

LUID
VEAPI
RtlConvertUlongToLuid(
    IN ULONG UlongValue
)
{
    LUID TempLuid;
    TempLuid.LowPart  = UlongValue;
    TempLuid.HighPart = 0;
    return TempLuid;
}

/* EOF */