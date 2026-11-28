/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : init.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PURPOSE
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <csr.h>

/* Revision History ------------------------------------------------------
 * DATE       : 28-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file init.c
 * --------------------------------------------------------------------- */

/* VARIABLE ***************************************************************/

HANDLE hSmApiPort = NULL;

/* PUBLIC *****************************************************************/

VEASTATUS
VEAPI
CsrServerInitialization(VOID)
{
    OBJECT_ATTRIBUTES ObjAttr;
    ANSI_STRING VeaPortName;
    ANSI_STRING SbPortName;
    ANSI_STRING SmPortName;
    HANDLE hApiThread = NULL;
    CLIENT_ID ClientId;
    VEASTATUS Status;

    CsrpInitializeProcessList();

    //
    // Create /VeaApiPort
    //

    RtlInitAnsiString(&VeaPortName, "/VeaApiPort");
    InitializeObjectAttributes(&ObjAttr, &VeaPortName, OBJ_CASE_INSENSITIVE, NULL, NULL);
    Status = VzCreatePort(&hVeaApiPort, &ObjAttr, CSR_MAX_CONN_INFO, CSR_MAX_MSG_LEN, KPC_MODE_MESSAGE);
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[CSRSRV] Failed to create port /VeaApiPort: 0x%08X\n\r", Status);
        return Status;
    }

    //
    // Create /SbApiPort
    //

    RtlInitAnsiString(&SbPortName, "/SbApiPort");
    InitializeObjectAttributes(&ObjAttr, &SbPortName, OBJ_CASE_INSENSITIVE, NULL, NULL);
    Status = VzCreatePort(&hSbApiPort, &ObjAttr, CSR_MAX_CONN_INFO, CSR_MAX_MSG_LEN, KPC_MODE_MESSAGE);
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[CSRSRV] Failed to create port /SbApiPort: 0x%08X\n\r", Status);
        return Status;
    }

    //
    // Load 2 of main Server DLL
    //

    Status = CsrLoadServerDll("basesrv.so", "BaseServerDllInitialization");
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    Status = CsrLoadServerDll("winsrv.so", "WinServerDllInitialization");
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    //
    // Create and run the thread worker
    //

    Status = RtlCreateUserThread(
        (HANDLE)-1,
        NULL,
        FALSE,
        0,
        0,
        0,
        (PVOID)CsrApiRequestThread,
        NULL,
        &hApiThread,
        &ClientId
    );
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[CSRSRV] Failed to create Workr Thread API: 0x%08X\n\r", Status);
        return Status;
    }
    if (hApiThread)
    {
        VzClose(hApiThread);
    }

    /* 5. Lapor ke SMSS setelah seluruh subsistem GUI & port siap */
    RtlInitAnsiString(&SmPortName, "/SmApiPort");
    Status = VzConnectPort(
        &hSmApiPort,
        &SmPortName,
        NULL,
        NULL,
        NULL,
        KPC_MODE_MESSAGE,
        NULL,
        NULL
    );
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[CSRSRV] VzConnectPort(/SmApiPort) failed: 0x%08X\n\r", Status);
        return Status;
    }

    KdPrintf("[CSRSRV] Successfully connected to /SmApiPort! (Handle=%p)\n\r", hSmApiPort);
    return STATUS_SUCCESS;
}

/* EOF */