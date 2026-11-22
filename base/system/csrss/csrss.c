/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : csrss.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : CSRSS BASIC
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "csr.h"

/* Revision History ------------------------------------------------------
 * DATE       : 17-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file csrss.c
 * --------------------------------------------------------------------- */

VEASTATUS
__cdecl
_main(
    int argc,
    char *argv[],
    char *envp,
    ULONG debugflags
)
{
    VEASTATUS Status;

    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    UNREFERENCED_PARAMETER(envp);
    UNREFERENCED_PARAMETER(debugflags);

    KdPrintf("\n\r[CSRSS] Client/Server Runtime Subsystem dimulai...\n\r");

    Status = CsrServerInitialization();
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[CSRSS] Inisialisasi gagal total: 0x%08X\n\r", Status);
        VzTerminateProcess((HANDLE)-1, Status);
        return Status;
    }

    CsrRunSbSessionLoop();
    return STATUS_SUCCESS;
}

/* EOF */