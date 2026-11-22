/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : basesrv.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : CSR Base Server
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "csr.h"

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file basesrv.c
 * --------------------------------------------------------------------- */

enum {
    BasepCreateProcess = 0,
    BasepCreateThread  = 1,
    BasepExitProcess   = 2,
    BasepMaxApiNumber
};

typedef struct _BASE_CREATE_PROCESS_MSG {
    HANDLE ProcessHandle;
    HANDLE ThreadHandle;
    CLIENT_ID ClientId;
    ULONG CreationFlags;
} BASE_CREATE_PROCESS_MSG, *PBASE_CREATE_PROCESS_MSG;

static VEASTATUS
VEAPI
BaseSrvCreateProcess(
    IN PCSR_PROCESS Process,
    IN OUT PCSR_KPC_MESSAGE Request,
    OUT PBOOLEAN Reply
)
{
    PBASE_CREATE_PROCESS_MSG Msg = (PBASE_CREATE_PROCESS_MSG)Request->Data;
    UNREFERENCED_PARAMETER(Process);
    UNREFERENCED_PARAMETER(Reply);

    KdPrintf("[BASESRV] CreateProcess notification: ChildPID=0x%X, Flags=0x%X\n\r",
             Msg->ClientId.UniqueProcess, Msg->CreationFlags);
    return STATUS_SUCCESS;
}

static VEASTATUS
VEAPI
BaseSrvCreateThread(
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
BaseSrvExitProcess(
    IN PCSR_PROCESS Process,
    IN OUT PCSR_KPC_MESSAGE Request,
    OUT PBOOLEAN Reply
)
{
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Reply);

    if (Process)
    {
        KdPrintf("[BASESRV] Proses PID=0x%X meminta ExitProcess\n\r", Process->ProcessId);
    }
    return STATUS_SUCCESS;
}

static CONST PCSR_API_ROUTINE BaseSrvApiTable[BasepMaxApiNumber] = {
    BaseSrvCreateProcess,
    BaseSrvCreateThread,
    BaseSrvExitProcess
};

VEASTATUS
VEAPI
BaseServerDllInitialization(VOID)
{
    CSR_SERVER_DLL ServerDll = {
        .ServerId          = CSR_SERVER_BASE,
        .Name              = "basesrv",
        .ApiTable          = BaseSrvApiTable,
        .ApiCount          = BasepMaxApiNumber,
        .DisconnectRoutine = NULL
    };

    return CsrRegisterServerDll(&ServerDll);
}

/* EOF */