/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : main.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : CSR Main Entry
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <csr.h>

/* Revision History ------------------------------------------------------
 * DATE       : 28-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file main.c
 * --------------------------------------------------------------------- */

INT
VEACDECL
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
        KdPrintf("[CSRSS] CsrServerInitialization gagal: 0x%08X\n\r", Status);
        VzTerminateProcess(NtCurrentProcess(), Status);
        return (INT)Status;
    }

    CsrRunSbSessionLoop();

    return 0;
}

/* EOF */