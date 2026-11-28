/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : stub.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : KernelAPI Stub
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <kernelapi_priv.h>

/* Revision History ------------------------------------------------------
 * DATE       : 28-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file stub.c
 * --------------------------------------------------------------------- */

static BOOLEAN BaseDllInitialized = FALSE;

BOOL
VEAPI
DllMain(
    IN PVOID hDll,
    IN DWORD dwReason,
    IN PVOID lpReserved
)
{
    VEASTATUS Status;
    BOOLEAN InServer = FALSE;

    UNREFERENCED_PARAMETER(hDll);
    UNREFERENCED_PARAMETER(lpReserved);

    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            if (BaseDllInitialized)
            {
                return TRUE;
            }

            /* Hubungkan proses ini ke CSRSS (basesrv.so) */
            Status = CsrClientConnectToServer(
                "/VeaOS",          /* Sesuaikan dengan nama ObjectDirectory CSRSS kamu */
                CSR_SERVER_BASE,
                NULL,
                NULL,
                &InServer
            );
            if (!VEA_SUCCESS(Status))
            {
                KdPrintf("[KERNELAPI] CsrClientConnectToServer failed: 0x%08X\n\r", Status);
                return FALSE;
            }

            BaseDllInitialized = TRUE;
            return TRUE;

        case DLL_PROCESS_DETACH:
            BaseDllInitialized = FALSE;
            return TRUE;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        default:
            return TRUE;
    }
}

/* EOF */