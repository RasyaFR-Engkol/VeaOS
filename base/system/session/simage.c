/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : simage.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Execute SMP Image
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "session.h"

/* Revision History ------------------------------------------------------
 * DATE       : 17-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file simage.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
SmpExecuteImage(
    IN PCHAR ImagePath,
    IN PCHAR CommandLine OPTIONAL,
    IN PVOID Environment OPTIONAL,
    IN BOOLEAN StartSuspended,
    OUT PRTL_USER_PROCESS_INFORMATION ProcessInfo
)
{
    ANSI_STRING ImageStr;
    ANSI_STRING CmdLineStr;
    ANSI_STRING CurDirStr;
    PRTL_USER_PROCESS_PARAMETERS ProcessParams = NULL;
    VEASTATUS Status;

    if (!ImagePath || !ProcessInfo)
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(ProcessInfo, sizeof(RTL_USER_PROCESS_INFORMATION));

    RtlInitAnsiString(&ImageStr, ImagePath);
    RtlInitAnsiString(&CmdLineStr, CommandLine ? CommandLine : ImagePath);
    RtlInitAnsiString(&CurDirStr, "/D1P1:/VeaOS/SystemReserved");

    /* 1. Rakit ProcessParameters (akan disalin RtlCreateUserProcess ke PEB anak) */
    KdPrintf("[SMSS/EXEC] 1. Sebelum RtlCreateProcessParameters (%s)\n\r", ImagePath);
    Status = RtlCreateProcessParameters(
        &ProcessParams,
        &ImageStr,
        NULL,           /* DllPath */
        &CurDirStr,     /* CurrentDirectory */
        &CmdLineStr,    /* CommandLine */
        Environment,    /* Environment block dari Step 2 SMSS */
        NULL,           /* WindowTitle */
        NULL,           /* DesktopInfo */
        NULL,           /* ShellInfo */
        NULL            /* RuntimeData */
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] RtlCreateProcessParameters gagal (%s): 0x%08X\n\r", ImagePath, Status);
        return Status;
    }

    /* 2. Buat proses + address space + initial thread (dalam keadaan Suspended) */
    KdPrintf("[SMSS/EXEC] 2. Sebelum RtlCreateUserProcess (%s)\n\r", ImagePath);
    Status = RtlCreateUserProcess(
        &ImageStr,
        OBJ_CASE_INSENSITIVE,
        ProcessParams,
        NULL,           /* ProcessSecurityDescriptor (pakai DefaultDacl SYSTEM) */
        NULL,           /* ThreadSecurityDescriptor */
        NULL,           /* ParentProcess -> otomatis PtSystemProcess (Token SYSTEM) */
        FALSE,          /* InheritHandles */
        NULL,           /* DebugPort */
        NULL,           /* ExceptionPort */
        ProcessInfo
    );

    KdPrintf("[SMSS/EXEC] 3. Sesudah RtlCreateUserProcess: 0x%08X\n\r", Status);
    RtlDestroyProcessParameters(ProcessParams);

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] RtlCreateUserProcess gagal (%s): 0x%08X\n\r", ImagePath, Status);
        return Status;
    }

    KdPrintf("[SMSS] Proses %s dibuat (PID=0x%X, TID=0x%X)\n\r",
             ImagePath,
             ProcessInfo->ClientId.UniqueProcess,
             ProcessInfo->ClientId.UniqueThread);

    /* 3. Jalankan thread utama proses anak jika tidak diminta tetap suspended */
    if (!StartSuspended)
    {
        Status = VzResumeThread(ProcessInfo->ThreadHandle, NULL);
        if (!VEA_SUCCESS(Status))
        {
            KdPrintf("[SMSS] VzResumeThread gagal (%s): 0x%08X\n\r", ImagePath, Status);
            VzTerminateProcess(ProcessInfo->ProcessHandle, Status);
            VzClose(ProcessInfo->ThreadHandle);
            VzClose(ProcessInfo->ProcessHandle);
            return Status;
        }
    }

    return STATUS_SUCCESS;
}