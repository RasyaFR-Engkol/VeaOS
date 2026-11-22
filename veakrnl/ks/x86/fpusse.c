/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : fpusse.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : FPU SSE Helper
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file fpusse.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
KiEnableSseAndFxsave(VOID)
{
    ULONG Cr0, Cr4;

    Cr0 = __ReadCR0();
    Cr0 &= ~(1UL << 2);  /* Clear CR0.EM (bit 2) */
    Cr0 &= ~(1UL << 3);  /* Clear CR0.TS (bit 3) */
    Cr0 |=  (1UL << 1);  /* Set CR0.MP (bit 1) */
    Cr0 |=  (1UL << 5);  /* Set CR0.NE (bit 5) */
    __WriteCR0(Cr0);

    Cr4 = __ReadCR4();
    Cr4 |= (1UL << 9);   /* Set CR4.OSFXSR (bit 9) */
    Cr4 |= (1UL << 10);  /* Set CR4.OSXMMEXCPT (bit 10) */
    __WriteCR4(Cr4);

    __asm__ __volatile__("fninit");
}