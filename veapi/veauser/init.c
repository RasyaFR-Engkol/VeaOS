/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : init.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Initialize User
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veapi.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file init.c
 * --------------------------------------------------------------------- */

VEASTATUS
WINAPI
UserInitialize(VOID)
{
    VEASTATUS Status;

    if (UserSubsystemInitialized) return STATUS_SUCCESS;

    UserSubsystemInitialized = TRUE;

    Status = InitVideo();
    if (!VEA_SUCCESS(Status))
    {
        KsBugCheckEx(VIDEO_DRIVER_INIT_FAILURE, Status, 0, 0, 0);
    }

    Status = UserInitializeWindowStations();
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    TimerInitialize();

    Status = GdiInitializeFontEngine();
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[VEAPI] Warning: GdiInitializeFontEngine gagal (0x%08X), lanjut tanpa font FT2\n", Status);
        /* Jangan BugCheck biar sistem tetap bisa jalan pakai fallback bitmap 8x16 */
    }


    return STATUS_SUCCESS;
}