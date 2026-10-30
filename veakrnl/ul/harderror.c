/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : harderror.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Handling software Hard Error
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 29-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file harderror.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
UlRaiseStatus(
    IN VEASTATUS Status
)
{
    // Cukup panggil KsiDispatchSehException dengan TrapFrame = NULL.
    // Dispatcher akan mengambil SavedEsp, SavedEbp, & TargetEip yang valid
    // dari _VeaFrame di FS:[0], lalu melakukan direct jump unwind!
    KsiDispatchSehException(NULL, Status);

    // Jika tidak ada _VEA_TRY yang menangkap status ini, Kernel Panic!
    KsBugCheckEx(FATAL_UNHANDLED_HARD_ERROR, Status, 0, 0, 0);
}

/* EOF */