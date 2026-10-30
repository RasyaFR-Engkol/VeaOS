/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : poevents.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Initializing Po Events
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 25-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file poevents.c
 * --------------------------------------------------------------------- */

/* PRIVATE */
VEASTATUS
VEAPI
PopAddRemoveSysCapsCallback(
    IN PVOID NotificationStructure,
    IN PVOID Context
)
{
    PDEVICE_INTERFACE_CHANGE_NOTIFICATION Notification;
    VEASTATUS Status;

    return Status;
}

/* PUBLIC */