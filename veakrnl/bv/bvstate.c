/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : bvstate.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : BV StateMachine
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>
#include "bv.h"

/* Revision History ------------------------------------------------------
 * DATE       : 01-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file bvstate.c
 * --------------------------------------------------------------------- */

// Display state. Assume lost
BV_DISPLAY_STATE BvDisplayState = BvDisplayLost;

BV_DISPLAY_STATE
VEAPI
BvAcquireDisplayState(BOOLEAN WeAcquired)
{
    if(WeAcquired)
    {
        BvDisplayState = BvDisplayAcquired;
    }
    else
    {
        BvDisplayState = BvDisplayLost;
    }

    return BvDisplayState;
}

BV_DISPLAY_STATE
VEAPI
BvGetDisplayState(VOID)
{
    return BvDisplayState;
}

VOID
VEAPI
BvNotifyDisplayOwnershipLost(VOID)
{
    // Simply set it to LOST
    BvDisplayState = BvDisplayLost;
}

BOOLEAN
VEAPI
BvDoWeHaveOwnership(VOID)
{
    return (BvDisplayState != BvDisplayLost);
}