/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : lock.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : OSI386 Section Team
   PURPOSE     : To intialize and do locking while in loader
 ----- Effective since 07-2026 til FOREVER ------------------------------- */

#include <osi386.h>

/* Revision History ------------------------------------------------------
 * DATE       : 26-07-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file lock.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
LdrInitializeSpinLock(PKSPIN_LOCK SpinLock)
{
    *SpinLock = 0;
}