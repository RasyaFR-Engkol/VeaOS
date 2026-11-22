/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : csrsrv.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : CSR Server
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "csr.h"

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file csrsrv.c
 * --------------------------------------------------------------------- */

static HANDLE hVeaApiPort = NULL;
static HANDLE hSbApiPort  = NULL;
static HANDLE hSmApiPort  = NULL;

static CSR_PROCESS gProcessTable[CSR_MAX_PROCESSES];
static CSR_SERVER_DLL gServerDlls[CSR_MAX_SERVER_DLLS];
static volatile int gProcTableLock = 0;

static VOID CsrpLockProcesses(VOID)
{
    while (__sync_lock_test_and_set(&gProcTableLock, 1))
    {
        VzYieldExecution();
    }
}

static VOID CsrpUnlockProcesses(VOID)
{
    __sync_lock_release(&gProcTableLock);
}

static PCSR_PROCESS
CsrpAllocateProcess(
    IN HANDLE ProcessId,
    IN HANDLE ClientCommPort
)
{
    PCSR_PROCESS Found = NULL;

    CsrpLockProcesses();
    for (ULONG i = 0; i < CSR_MAX_PROCESSES; i++)
    {
        if (!gProcessTable[i].InUse)
        {
            RtlZeroMemory(&gProcessTable[i], sizeof(CSR_PROCESS));
            gProcessTable[i].InUse          = TRUE;
            gProcessTable[i].ProcessId      = ProcessId;
            gProcessTable[i].ClientCommPort = ClientCommPort;
            gProcessTable[i].ShutdownLevel  = 0x280;
            Found = &gProcessTable[i];
            break;
        }
    }
    CsrpUnlockProcesses();

    return Found;
}

static VOID
CsrpRemoveProcess(
    IN HANDLE ProcessId
)
{
    CSR_PROCESS Snapshot = {0};
    BOOLEAN Removed = FALSE;

    CsrpLockProcesses();

    for (ULONG i = 0; i < CSR_MAX_PROCESSES; i++)
    {
        if (gProcessTable[i].InUse && gProcessTable[i].ProcessId == ProcessId)
        {
            Snapshot = gProcessTable[i];
            RtlZeroMemory(&gProcessTable[i], sizeof(CSR_PROCESS));
            Removed = TRUE;
            break;
        }
    }

    CsrpUnlockProcesses();

    if (Removed)
    {
        for (ULONG s = 0; s < CSR_MAX_SERVER_DLLS; s++)
        {
            if (gServerDlls[s].DisconnectRoutine)
            {
                gServerDlls[s].DisconnectRoutine(&Snapshot);
            }
        }

        if (Snapshot.ClientCommPort)
        {
            VzClose(Snapshot.ClientCommPort);
        }

        KdPrintf("[CSRSRV] Client with PID=0x%X removed from table.\n\r", ProcessId);
    }
}

PCSR_PROCESS
VEAPI
CsrLocateProcessById(HANDLE ProcessId)
{
    PCSR_PROCESS Found = NULL;

    CsrpLockProcesses();
    for (ULONG i = 0; i < CSR_MAX_PROCESSES; i++)
    {
        if (gProcessTable[i].InUse && gProcessTable[i].ProcessId == ProcessId)
        {
            Found = &gProcessTable[i];
            break;
        }
    }
    CsrpUnlockProcesses();

    return Found;
}

VEASTATUS
VEAPI
CsrRegisterServerDll(PCSR_SERVER_DLL ServerDll)
{
    if (!ServerDll || ServerDll->ServerId >= CSR_MAX_SERVER_DLLS)
    {
        return STATUS_INVALID_PARAMETER;
    }

    gServerDlls[ServerDll->ServerId] = *ServerDll;
    KdPrintf("[CSRSRV] Registered on Server DLL [%u]: %s (%lu API)\n\r",
             ServerDll->ServerId, ServerDll->Name, ServerDll->ApiCount);
    return STATUS_SUCCESS;
}

VOID
VEAPI
CsrApiRequestThread(
    IN PVOID Parameter
)
{
    CSR_KPC_MESSAGE ReceiveMsg;
    CSR_KPC_MESSAGE ReplyMsg;
    PKPC_MESSAGE_HEADER ReplyPtr = NULL;
    VEASTATUS Status;

    UNREFERENCED_PARAMETER(Parameter);

    KdPrintf("[CSRSRV/API] Worker thread of /VeaApiPort active...\n\r");

    while (TRUE)
    {
        RtlZeroMemory(&ReceiveMsg, sizeof(ReceiveMsg));

        Status = VzReplyWaitReceivePort(
            hVeaApiPort,
            NULL,
            ReplyPtr,
            &ReceiveMsg.Header
        );

        ReplyPtr = NULL;

        if (!VEA_SUCCESS(Status))
        {
            KdPrintf("[CSRSRV/API] VzReplyWaitReceivePort error: 0x%08X\n\r", Status);
            VzYieldExecution();
            continue;
        }

        HANDLE ClientPid = ReceiveMsg.Header.ClientId.UniqueProcess;

        if (ReceiveMsg.Header.MessageType == KPC_CONNECTION_REQUEST)
        {
            HANDLE hClientComm = NULL;
            Status = VzAcceptConnectPort(
                &hClientComm,
                NULL,
                &ReceiveMsg.Header,
                TRUE,
                NULL,
                NULL
            );

            if (VEA_SUCCESS(Status))
            {
                if (!CsrpAllocateProcess(ClientPid, hClientComm))
                {
                    KdPrintf("[CSRSRV/API] FIXME: Process table is full on PID=0x%X!\n\r", ClientPid);
                    VzClose(hClientComm);
                }
                else
                {
                    KdPrintf("[CSRSRV/API] Client connected. PID=0x%X, CommHandle=%p\n\r",
                             ClientPid, hClientComm);
                }
            }
        }
        else if (ReceiveMsg.Header.DataLength < (sizeof(ULONG) + sizeof(VEASTATUS)))
        {
            /* Notifikasi port ditutup / klien mati (KPC_PORT_CLOSED / KPC_CLIENT_DIED) */
            CsrpRemoveProcess(ClientPid);
        }
        else
        {
            USHORT ServerId = CSR_GET_SERVER_ID(ReceiveMsg.ApiNumber);
            USHORT FuncId   = CSR_GET_FUNCTION_ID(ReceiveMsg.ApiNumber);
            BOOLEAN ShouldReply = TRUE;
            VEASTATUS ApiStatus = STATUS_INVALID_PARAMETER;

            PCSR_PROCESS ClientProc = CsrLocateProcessById(ClientPid);

            if (ServerId < CSR_MAX_SERVER_DLLS &&
                gServerDlls[ServerId].ApiTable != NULL &&
                FuncId < gServerDlls[ServerId].ApiCount &&
                gServerDlls[ServerId].ApiTable[FuncId] != NULL)
            {
                ApiStatus = gServerDlls[ServerId].ApiTable[FuncId](
                    ClientProc,
                    &ReceiveMsg,
                    &ShouldReply
                );
            }
            else
            {
                KdPrintf("[CSRSRV/API] Unknown API Number: 0x%08X (PID=0x%X)\n\r",
                         ReceiveMsg.ApiNumber, ClientPid);
            }

            if (ShouldReply)
            {
                RtlCopyMemory(&ReplyMsg, &ReceiveMsg, sizeof(CSR_KPC_MESSAGE));
                ReplyMsg.Header.TotalLength = sizeof(CSR_KPC_MESSAGE);
                ReplyMsg.Header.DataLength  = sizeof(CSR_KPC_MESSAGE) - sizeof(KPC_MESSAGE_HEADER);
                ReplyMsg.Header.MessageId   = ReceiveMsg.Header.MessageId;
                ReplyMsg.Status             = ApiStatus;

                ReplyPtr = &ReplyMsg.Header;
            }
        }
    }
}

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

    RtlZeroMemory(gProcessTable, sizeof(gProcessTable));
    RtlZeroMemory(gServerDlls, sizeof(gServerDlls));

    /* 1. Buat /VeaApiPort */
    RtlInitAnsiString(&VeaPortName, "/VeaApiPort");
    InitializeObjectAttributes(&ObjAttr, &VeaPortName, OBJ_CASE_INSENSITIVE, NULL, NULL);

    Status = VzCreatePort(&hVeaApiPort, &ObjAttr, CSR_MAX_CONN_INFO, CSR_MAX_MSG_LEN, KPC_MODE_MESSAGE);
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[CSRSRV] Failed to create port /VeaApiPort: 0x%08X\n\r", Status);
        return Status;
    }

    /* 2. Buat /SbApiPort */
    RtlInitAnsiString(&SbPortName, "/SbApiPort");
    InitializeObjectAttributes(&ObjAttr, &SbPortName, OBJ_CASE_INSENSITIVE, NULL, NULL);

    Status = VzCreatePort(&hSbApiPort, &ObjAttr, CSR_MAX_CONN_INFO, CSR_MAX_MSG_LEN, KPC_MODE_MESSAGE);
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[CSRSRV] Failed to create port /SbApiPort: 0x%08X\n\r", Status);
        return Status;
    }

    /* 3. Inisialisasi Server DLL (basesrv & winsrv -> memanggil VeaUserInitialize) */
    Status = BaseServerDllInitialization();
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    Status = WinServerDllInitialization();
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* 4. Jalankan worker thread /VeaApiPort & tutup handle thread-nya */
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

VOID
VEAPI
CsrRunSbSessionLoop(VOID)
{
    CSR_KPC_MESSAGE SbReceiveMsg;
    CSR_KPC_MESSAGE SbReplyMsg;
    PKPC_MESSAGE_HEADER SbReplyPtr = NULL;
    VEASTATUS Status;

    KdPrintf("[CSRSRV] Wait for session at /SbApiPort...\n\r");

    while (TRUE)
    {
        RtlZeroMemory(&SbReceiveMsg, sizeof(SbReceiveMsg));

        Status = VzReplyWaitReceivePort(
            hSbApiPort,
            NULL,
            SbReplyPtr,
            &SbReceiveMsg.Header
        );

        SbReplyPtr = NULL;

        if (!VEA_SUCCESS(Status))
        {
            VzYieldExecution();
            continue;
        }

        if (SbReceiveMsg.Header.MessageType == KPC_CONNECTION_REQUEST)
        {
            HANDLE hSmComm = NULL;
            VzAcceptConnectPort(&hSmComm, NULL, &SbReceiveMsg.Header, TRUE, NULL, NULL);
            KdPrintf("[CSRSRV/SB] SMSS connected to /SbApiPort (Handle=%p)\n\r", hSmComm);
        }
        else
        {
            KdPrintf("[CSRSRV/SB] Session command received: ApiNumber=0x%X\n\r", SbReceiveMsg.ApiNumber);

            RtlZeroMemory(&SbReplyMsg, sizeof(SbReplyMsg));
            SbReplyMsg.Header.TotalLength = sizeof(CSR_KPC_MESSAGE);
            SbReplyMsg.Header.DataLength  = sizeof(CSR_KPC_MESSAGE) - sizeof(KPC_MESSAGE_HEADER);
            SbReplyMsg.Header.MessageId   = SbReceiveMsg.Header.MessageId;
            SbReplyMsg.Status             = STATUS_SUCCESS;

            SbReplyPtr = &SbReplyMsg.Header;
        }
    }
}

/* EOF */