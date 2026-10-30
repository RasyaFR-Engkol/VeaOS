/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : main.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : AHCI Header
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "ahciheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 25-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file main.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
DriverEntry(IN PDRIVER_OBJECT DriverObject)
{
    DPRINT("AHCI Driver loaded\n");

    DriverObject->AddDeviceRoutine = AhciAddDevice;
    return ObioAttachInterruptToThisObject(DriverObject, AhciObIntHandler);
}