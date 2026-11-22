/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : process.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VEAPI Process Handling
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file process.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
UserProcessCallout(
    IN PEPROCESS Process,
    IN BOOLEAN Create
)
{
    UNREFERENCED_PARAMETER(Process);
    UNREFERENCED_PARAMETER(Create);
    /* TODO: Inisialisasi / cleanup W32PROCESS saat proses masuk/keluar GUI */
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
UserThreadCallout(
    IN PETHREAD Thread,
    IN VEAPI_THREAD_CALLOUT_TYPE Type
)
{
    UNREFERENCED_PARAMETER(Thread);
    UNREFERENCED_PARAMETER(Type);
    /* TODO: Inisialisasi / cleanup W32THREAD & message queue saat thread exit */
    return STATUS_SUCCESS;
}

