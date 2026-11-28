/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : winsrv.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : CSR Win Server
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <csr.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file winsrv.c
 * --------------------------------------------------------------------- */
static VEASTATUS
VEAPI
WinSrvAllocConsole(
    IN PCSR_PROCESS Process,
    IN OUT PCSR_KPC_MESSAGE Request,
    OUT PBOOLEAN Reply
)
{
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Reply);

    KdPrintf("[WINSRV] AllocConsole requested by PID=0x%X\n\r",
             Process ? Process->ProcessId : NULL);
    return STATUS_SUCCESS;
}

static VEASTATUS
VEAPI
WinSrvWriteConsole(
    IN PCSR_PROCESS Process,
    IN OUT PCSR_KPC_MESSAGE Request,
    OUT PBOOLEAN Reply
)
{
    UNREFERENCED_PARAMETER(Process);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Reply);
    return STATUS_SUCCESS;
}

static VEASTATUS
VEAPI
WinSrvReadConsole(
    IN PCSR_PROCESS Process,
    IN OUT PCSR_KPC_MESSAGE Request,
    OUT PBOOLEAN Reply
)
{
    UNREFERENCED_PARAMETER(Process);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Reply);
    return STATUS_SUCCESS;
}

static VEASTATUS
VEAPI
WinSrvExitWindows(
    IN PCSR_PROCESS Process,
    IN OUT PCSR_KPC_MESSAGE Request,
    OUT PBOOLEAN Reply
)
{
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Reply);

    KdPrintf("[WINSRV] ExitWindows requested by PID=0x%X\n\r",
             Process ? Process->ProcessId : NULL);
    return STATUS_SUCCESS;
}

static VOID
VEAPI
WinSrvClientDisconnect(
    IN PCSR_PROCESS Process
)
{
    /* TODO: Bersihkan screen buffer konsol jika klien ini pemilik konsol */
    UNREFERENCED_PARAMETER(Process);
}

static CONST PCSR_API_ROUTINE WinSrvApiTable[WinSrvpMaxApiNumber] = {
    WinSrvAllocConsole,
    WinSrvWriteConsole,
    WinSrvReadConsole,
    WinSrvExitWindows
};

VEASTATUS
VEAPI
WinServerDllInitialization(VOID)
{
    VEASTATUS Status;

    /* Inisialisasi video & WinSta0\Default di system_veapi.ko */
    Status = VeaUserInitialize();
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[WINSRV] VeaUserInitialize failed: 0x%08X\n\r", Status);
        return Status;
    }

    CSR_SERVER_DLL ServerDll = {
        .ServerId          = CSR_SERVER_WIN,
        .Name              = "winsrv",
        .ApiTable          = WinSrvApiTable,
        .ApiCount          = WinSrvpMaxApiNumber,
        .DisconnectRoutine = WinSrvClientDisconnect
    };

    return CsrRegisterServerDll(&ServerDll);
}