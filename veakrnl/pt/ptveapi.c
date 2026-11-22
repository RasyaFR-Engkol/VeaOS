/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : ptveapi.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VEAPI 
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file ptveapi.c
 * --------------------------------------------------------------------- */

VEAPI_CALLOUTS_FPNS PtpVeapiCallouts = { 0 };

VOID
VEAPI
PtEstablishVeapiCallouts(
    IN PVEAPI_CALLOUTS_FPNS CalloutData
)
{
    if (!CalloutData)
    {
        return;
    }

    /* Salin seluruh pointer fungsi dari veapi.ko ke kernel */
    RtlCopyMemory(&PtpVeapiCallouts, CalloutData, sizeof(VEAPI_CALLOUTS_FPNS));

    /* Update ukuran body objek supaya ObCreateObject alokasi ukuran yang pas */
    if (UlWindowStationObjectType && CalloutData->WindowStationObjectSize > 0)
    {
        UlWindowStationObjectType->ObjectSize = CalloutData->WindowStationObjectSize;
    }

    if (UlDesktopObjectType && CalloutData->DesktopObjectSize > 0)
    {
        UlDesktopObjectType->ObjectSize = CalloutData->DesktopObjectSize;
    }

    DPRINT("[PT] VEAPI callouts established (WinStaSize=%u, DeskSize=%u)\n",
           CalloutData->WindowStationObjectSize,
           CalloutData->DesktopObjectSize);
}