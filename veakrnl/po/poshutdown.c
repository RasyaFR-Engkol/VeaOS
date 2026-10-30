/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : poshutdown.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Shutdown handling
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 25-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file poshutdown.c
 * --------------------------------------------------------------------- */

/* VARIABLE */

ULONG PopShutdownPowerOffPolicy;
KEVENT PopShutdownEvent;
PPOP_SHUTDOWN_WAIT_ENTRY PopShutdownThreadList;
LIST_ENTRY PopShutdownQueue;
BOOLEAN PopShutdownListAvailable;

/* PRIVATE */
VOID
VEAPI
PopInitShutdownList(VOID)
{
    KsInitializeEvent(&PopShutdownEvent, NotificationEvent, FALSE);

    PopShutdownThreadList = NULL;
    InitializeListHead(&PopShutdownQueue);

    PopShutdownListAvailable = TRUE;
}