/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : session.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : SMSS Entry Point
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "session.h"
#include "vz_wrappers.h"
#include <psdk/msg.h>
#include <psdk/virtkey.h>
#include <psdk/winuser.h>

/* Revision History ------------------------------------------------------
 * DATE       : 15-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file session.c
 * --------------------------------------------------------------------- */

HANDLE gGuiSubsystemReadyEvent = NULL;

VEASTATUS
VEAPI
SmpStartSubsystem(
    IN BOOLEAN CliMode,
    IN PCHAR SubSysPath,    /* "/D1P1:/VeaOSReserved/csrss.elf" */
    IN PCHAR UiPath,        /* "/D1P1:/VeaOS/SystemReserved/VeaLogonScr.elf" atau winterm.elf */
    IN PVOID Environment,
    OUT PHANDLE SubSysProcessHandle,
    OUT PHANDLE UiProcessHandle
)
{
    RTL_USER_PROCESS_INFORMATION CsrssInfo;
    RTL_USER_PROCESS_INFORMATION UiInfo;
    ANSI_STRING VeapiImageStr;
    VEASTATUS Status;

    if (SubSysProcessHandle) *SubSysProcessHandle = NULL;
    if (UiProcessHandle)     *UiProcessHandle     = NULL;

    // Panggil Veapi
    RtlInitAnsiString(&VeapiImageStr, SmpVeapiPath);
    Status = VzSetSystemInformation(
        SystemExtendServiceTableInformation,
        &VeapiImageStr,
        sizeof(VeapiImageStr)
    );
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] Gagal memuat %s: 0x%08X\n\r", SmpVeapiPath, Status);
        return Status;
    }

    /* JALUR A: Mode CLI (/CLIMODE /WinTerminal) -> Langsung luncurkan terminal */
    if (CliMode)
    {
        KdPrintf("[SMSS] Step 4 (CLI Mode): Meluncurkan %s...\n\r", UiPath);

        Status = SmpExecuteImage(UiPath, UiPath, Environment, FALSE, &UiInfo);
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }

        VzClose(UiInfo.ThreadHandle);
        if (UiProcessHandle) *UiProcessHandle = UiInfo.ProcessHandle;
        return STATUS_SUCCESS;
    }

    /* JALUR B: Mode Grafis -> Luncurkan CSRSS dulu, baru VeaLogonScr */
    KdPrintf("[SMSS] Step 4: Meluncurkan Subsystem %s...\n\r", SubSysPath);

    Status = SmpExecuteImage(SubSysPath, SubSysPath, Environment, FALSE, &CsrssInfo);
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* Tutup handle thread CSRSS, simpan handle prosesnya untuk watchdog */
    VzClose(CsrssInfo.ThreadHandle);
    if (SubSysProcessHandle) *SubSysProcessHandle = CsrssInfo.ProcessHandle;

    /* Beri kesempatan CSRSS membuka /VeaApiPort & /SbApiPort sebelum UI jalan */
    VzYieldExecution();

    KdPrintf("[SMSS] Menunggu CSRSS & VEAPI siap...\n");
    VzWaitForSingleObject(gGuiSubsystemReadyEvent, FALSE, NULL);

    KdPrintf("[SMSS] Step 4: Meluncurkan Logon UI %s...\n\r", UiPath);

    Status = SmpExecuteImage(UiPath, UiPath, Environment, FALSE, &UiInfo);
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] Peringatan: Gagal meluncurkan UI (%s): 0x%08X\n\r", UiPath, Status);
        return Status;
    }

    VzClose(UiInfo.ThreadHandle);
    if (UiProcessHandle) *UiProcessHandle = UiInfo.ProcessHandle;

    return STATUS_SUCCESS;
}

VEASTATUS
VEACDECL
_main(
    int argc,
    char *argv[],
    char *envp,
    ULONG debugflags
)
{
    VEASTATUS Status;
    HANDLE hCsrssProc = NULL;
    HANDLE hUiProc    = NULL;
    RTL_USER_PROCESS_INFORMATION TestAppInfo;
    
    KdPrintf("SMSS LMAOOO\n");

    Status = SmpInitializeNamespace();
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] Failed to create Namespace: 0x%08X\n", Status);
        while (1);
    }

    Status = SmpLoadConfiguration(argc, argv, envp);
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] Step 2 GAGAL: 0x%08X\n", Status);
        while (1);
    }

    Status = SmpInitializeKnownDlls();
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] Peringatan: /KnownDLLs gagal diinisialisasi: 0x%08X\n", Status);
    }

    Status = SmpCreateApiPort();
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] Step 3 failed: 0x%x\n", Status);
        while (1);
    }

    Status = VzCreateEvent(
        &gGuiSubsystemReadyEvent, 
        EVENT_ALL_ACCESS, 
        NULL, 
        NotificationEvent, 
        FALSE);

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] Step 3.5 failed: 0x%08X\n\r", Status);
        goto ExecuteLoop;
    }

    Status = SmpStartSubsystem(
        SmpUseWinTerminal,
        SmpSubSystemPath,
        SmpSessionUiPath,
        envp,
        &hCsrssProc,
        &hUiProc
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] Step 4 failed: 0x%08X\n\r", Status);
    }

    PCHAR TestAppPath = "/D1P1:/VeaOS/SystemReserved/TestVeapiApp.elf";
    Status = SmpExecuteImage(TestAppPath, TestAppPath, envp, FALSE, &TestAppInfo);
    if (VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] Meluncurkan %s (PID=0x%p)\n\r", TestAppPath, TestAppInfo.ClientId.UniqueProcess);
        VzClose(TestAppInfo.ThreadHandle);
        VzClose(TestAppInfo.ProcessHandle);
    }
    else
    {
        KdPrintf("[SMSS] Gagal meluncurkan %s: 0x%08X\n\r", TestAppPath, Status);
    }

    KdPrintf("[SMSS] Boot sesi awal selesai! Memasuki mode standby...\n\r");

ExecuteLoop:
    while (1)
    {
        VzYieldExecution();
    }
}
