/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : processthread.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Process Thread for Kernelapi
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <userapi.h>
#include <kernelapi_priv.h>

/* Revision History ------------------------------------------------------
 * DATE       : 28-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file processthread.c
 * --------------------------------------------------------------------- */

BOOL
VEAPI
CreateProcessA(
    IN PSTR lpApplicationName,
    IN PSTR lpCommandLine,
    IN LPSECURITY_ATTRIBUTES lpProcessAttributes,
    IN LPSECURITY_ATTRIBUTES lpThreadAttributes,
    IN BOOL bInheritHandles,
    IN DWORD dwCreationFlags,
    IN PVOID lpEnvironment,
    IN PSTR lpCurrentDirectory,
    IN LPSTARTUPINFOA lpStartupInfo,
    OUT LPPROCESS_INFORMATION lpProcessInformation
)
{
    ANSI_STRING ImagePath, CmdLine, CurDir;
    PRTL_USER_PROCESS_PARAMETERS ProcessParams = NULL;
    RTL_USER_PROCESS_INFORMATION ProcessInfo;
    CSR_KPC_MESSAGE CsrMsg;
    PBASE_CREATE_PROCESS_MSG CreateMsg;
    VEASTATUS Status;

    UNREFERENCED_PARAMETER(lpProcessAttributes);
    UNREFERENCED_PARAMETER(lpThreadAttributes);
    UNREFERENCED_PARAMETER(lpStartupInfo);

    if (!lpProcessInformation || (!lpApplicationName && !lpCommandLine))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if ((dwCreationFlags & (DETACHED_PROCESS | CREATE_NEW_CONSOLE)) ==
        (DETACHED_PROCESS | CREATE_NEW_CONSOLE))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    RtlZeroMemory(lpProcessInformation, sizeof(PROCESS_INFORMATION));
    RtlZeroMemory(&ProcessInfo, sizeof(ProcessInfo));

    RtlInitAnsiString(&ImagePath, lpApplicationName ? lpApplicationName : lpCommandLine);
    RtlInitAnsiString(&CmdLine, lpCommandLine ? lpCommandLine : lpApplicationName);
    RtlInitAnsiString(&CurDir, lpCurrentDirectory);

    Status = RtlCreateProcessParameters(
        &ProcessParams,
        &ImagePath,
        NULL,
        lpCurrentDirectory ? &CurDir : NULL,
        &CmdLine,
        lpEnvironment,
        NULL,
        NULL,
        NULL,
        NULL
    );
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[KERNELAPI] RtlCreateProcessParameters gagal: 0x%08X\n\r", Status);
        BaseSetLastVeaError(Status);
        return FALSE;
    }

    Status = RtlCreateUserProcess(
        &ImagePath,
        OBJ_CASE_INSENSITIVE,
        ProcessParams,
        NULL,
        NULL,
        NULL,
        (BOOLEAN)bInheritHandles,
        NULL,
        NULL,
        &ProcessInfo
    );
    RtlDestroyProcessParameters(ProcessParams);

    KdPrintf("[KERNELAPI] RtlCreateUserProcess status=0x%08X PID=0x%X\n\r",
             Status, ProcessInfo.ClientId.UniqueProcess);

    if (!VEA_SUCCESS(Status))
    {
        BaseSetLastVeaError(Status);
        return FALSE;
    }

    RtlZeroMemory(&CsrMsg, sizeof(CsrMsg));
    CreateMsg = (PBASE_CREATE_PROCESS_MSG)CsrMsg.Data;
    CreateMsg->ProcessHandle = ProcessInfo.ProcessHandle;
    CreateMsg->ThreadHandle  = ProcessInfo.ThreadHandle;
    CreateMsg->ClientId      = ProcessInfo.ClientId;
    CreateMsg->CreationFlags = dwCreationFlags;

    KdPrintf("[KERNELAPI] Calling CsrClientCallServer(BasepCreateProcess)...\n\r");
    Status = CsrClientCallServer(
        &CsrMsg.Header,
        CSR_MAKE_API_NUMBER(CSR_SERVER_BASE, BasepCreateProcess),
        sizeof(BASE_CREATE_PROCESS_MSG)
    );
    KdPrintf("[KERNELAPI] CsrClientCallServer returned 0x%08X\n\r", Status);
    if (!VEA_SUCCESS(Status))
    {
        VzTerminateProcess(ProcessInfo.ProcessHandle, Status);
        VzClose(ProcessInfo.ThreadHandle);
        VzClose(ProcessInfo.ProcessHandle);
        BaseSetLastVeaError(Status);
        return FALSE;
    }

    if (!(dwCreationFlags & CREATE_SUSPENDED))
    {
        VzResumeThread(ProcessInfo.ThreadHandle, NULL);
        KdPrintf("[KERNELAPI] CsrClientCallServer returned 0x%08X\n\r", Status);
    }

    lpProcessInformation->hProcess    = ProcessInfo.ProcessHandle;
    lpProcessInformation->hThread     = ProcessInfo.ThreadHandle;
    lpProcessInformation->dwProcessId = (DWORD)(ULONG_PTR)ProcessInfo.ClientId.UniqueProcess;
    lpProcessInformation->dwThreadId  = (DWORD)(ULONG_PTR)ProcessInfo.ClientId.UniqueThread;
    return TRUE;
}

VOID
VEAPI
ExitProcess(
    IN UINT uExitCode
)
{
    CSR_KPC_MESSAGE CsrMsg;
    PBASE_EXIT_PROCESS_MSG ExitMsg;

    // LdrShutdownProcess();

    RtlZeroMemory(&CsrMsg, sizeof(CsrMsg));
    ExitMsg = (PBASE_EXIT_PROCESS_MSG)CsrMsg.Data;
    ExitMsg->ExitCode = uExitCode;

    CsrClientCallServer(
        &CsrMsg.Header,
        CSR_MAKE_API_NUMBER(CSR_SERVER_BASE, BasepExitProcess),
        sizeof(BASE_EXIT_PROCESS_MSG)
    );

    VzTerminateProcess(NtCurrentProcess(), (VEASTATUS)uExitCode);

    while (1)
    {
        __asm__ __volatile__("pause");
    }
}

/* EOF */