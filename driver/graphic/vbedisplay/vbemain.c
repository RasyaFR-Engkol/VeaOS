/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : vbemain.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VBE Main Entry
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "vbe.h"

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file vbemain.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
DriverEntry(
    IN PDRIVER_OBJECT DriverObject
)
{
    VEASTATUS Status;

    Status = ObioAttachInterruptToThisObject(DriverObject, VbeObIntHandler);
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    return VbeCreateDevice(DriverObject);
}
