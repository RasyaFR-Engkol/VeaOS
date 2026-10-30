/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : intrinsic.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Intrinsic for GCC 
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <rtl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 17-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file intrinsic.c
 * --------------------------------------------------------------------- */

ULONGLONG
__udivdi3(
    IN ULONGLONG Dividend,
    IN ULONGLONG Divisor
)
{
    return RtlDivideUnsigned64(Dividend, Divisor, NULL);
}

ULONGLONG
__umoddi3(
    IN ULONGLONG Dividend,
    IN ULONGLONG Divisor
)
{
    ULONGLONG Remainder = 0;
    RtlDivideUnsigned64(Dividend, Divisor, &Remainder);
    return Remainder;
}

void *
memcpy(
    void *dest,
    const void *src,
    SIZE_T count
)
{
    RtlCopyMemory(dest, (PVOID)src, count);
    return dest;
}

void *memset(void *ptr, int x, size_t n)
{
    RtlFillMemory(ptr, n, x);
    return ptr;
}

LONGLONG
__divdi3(
    IN LONGLONG Dividend,
    IN LONGLONG Divisor
)
{
    return RtlDivideSigned64(Dividend, Divisor, NULL);
}

LONGLONG
__moddi3(
    IN LONGLONG Dividend,
    IN LONGLONG Divisor
)
{
    LONGLONG Remainder = 0;
    RtlDivideSigned64(Dividend, Divisor, &Remainder);
    return Remainder;
}