/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : trigger.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Triggering Debug Breakpoint
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 13-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file trigger.c
 * --------------------------------------------------------------------- */

DEBUGSTATUS KdpLastStatus;

VOID
VEAPI
KdpDebugBreak(DEBUGSTATUS Reason)
{
    /* Set LastStatus */
    KdpLastStatus = Reason;

    /* Trigger */
    asm volatile ("int $3");
}