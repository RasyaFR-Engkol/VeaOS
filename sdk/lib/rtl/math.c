/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : math.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Rtl Math
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <rtl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 17-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file math.c
 * --------------------------------------------------------------------- */

ULONGLONG
VEAPI
RtlDivideUnsigned64(
    IN ULONGLONG Dividend,
    IN ULONGLONG Divisor,
    OUT PULONGLONG Remainder OPTIONAL
)
{
    if (Divisor == 0)
    {
        if (Remainder) *Remainder = 0;
        return 0;
    }

    // Fast-path: Kalau nilai masih muat di 32-bit, pakai pembagian hardware 32-bit biasa.
    // Pembagian 32-bit TIDAK akan memanggil __udivdi3.
    if (Dividend <= 0xFFFFFFFFULL && Divisor <= 0xFFFFFFFFULL)
    {
        if (Remainder) *Remainder = (ULONGLONG)((ULONG)Dividend % (ULONG)Divisor);
        return (ULONGLONG)((ULONG)Dividend / (ULONG)Divisor);
    }

    // Slow-path: Bitwise long division untuk nilai 64-bit murni
    ULONGLONG Quotient = 0;
    ULONGLONG Rem = 0;

    for (INT i = 63; i >= 0; i--)
    {
        Rem = (Rem << 1) | ((Dividend >> i) & 1ULL);
        if (Rem >= Divisor)
        {
            Rem -= Divisor;
            Quotient |= (1ULL << i);
        }
    }

    if (Remainder)
    {
        *Remainder = Rem;
    }

    return Quotient;
}

LONGLONG
VEAPI
RtlDivideSigned64(
    IN LONGLONG Dividend,
    IN LONGLONG Divisor,
    OUT PLONGLONG Remainder OPTIONAL
)
{
    if (Divisor == 0)
    {
        if (Remainder) *Remainder = 0;
        return 0;
    }

    /* Tentukan tanda (+/-) */
    BOOLEAN IsQuotientNegative = ((Dividend < 0) ^ (Divisor < 0));
    BOOLEAN IsRemainderNegative = (Dividend < 0);

    /* Ambil nilai mutlak/absolut */
    ULONGLONG AbsDividend = (Dividend < 0) ? (ULONGLONG)(-Dividend) : (ULONGLONG)Dividend;
    ULONGLONG AbsDivisor  = (Divisor < 0)  ? (ULONGLONG)(-Divisor)  : (ULONGLONG)Divisor;

    ULONGLONG UnsignedRem = 0;
    ULONGLONG UnsignedQuot = RtlDivideUnsigned64(AbsDividend, AbsDivisor, &UnsignedRem);

    LONGLONG Quotient = IsQuotientNegative ? -(LONGLONG)UnsignedQuot : (LONGLONG)UnsignedQuot;
    LONGLONG Rem = IsRemainderNegative ? -(LONGLONG)UnsignedRem : (LONGLONG)UnsignedRem;

    if (Remainder)
    {
        *Remainder = Rem;
    }

    return Quotient;
}