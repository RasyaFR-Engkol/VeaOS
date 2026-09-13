/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : thread.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Part of threading system
 ----- Effective since 07-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 25-07-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file thread.c
 * --------------------------------------------------------------------- */

VOID
VEAFAST
KsiDispatchInterrupt(VOID)
{
    PKPRCB Prcb = KeGetCurrentPrcb();

    Prcb->DpcInterruptRequested = FALSE;

    if (!IsListEmpty(&Prcb->DpcListHead)) {
        KsiRetireDpcList(Prcb);
    }

    PtSchedulerTick(KsQuerySystemTime());
}