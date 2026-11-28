/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : api.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : CSR Api
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <csr.h>

/* Revision History ------------------------------------------------------
 * DATE       : 28-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file api.c
 * --------------------------------------------------------------------- */

/* VARIABLE ***************************************************************/

HANDLE hVeaApiPort = NULL;

/* PUBLIC *****************************************************************/

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
                PCSR_PROCESS ClientProc = CsrLocateProcessById(ClientPid);
                if (ClientProc)
                {
                    /* Proses sudah didaftarkan lebih dulu oleh BaseSrvCreateProcess */
                    ClientProc->ClientCommPort = hClientComm;
                    KdPrintf("[CSRSRV/API] Pre-registered client connected. PID=0x%X, CommHandle=%p\n\r",
                             ClientPid, hClientComm);
                }
                else if (!CsrpAllocateProcess(ClientPid, hClientComm))
                {
                    KdPrintf("[CSRSRV/API] ERROR: Allocate process failed\n\r");
                    VzClose(hClientComm);
                }
                else
                {
                    /* Proses yang diluncurkan langsung oleh SMSS (misal VeaLogonScr.elf) */
                    KdPrintf("[CSRSRV/API] Client connected. PID=0x%X, CommHandle=%p\n\r",
                             ClientPid, hClientComm);
                }
            }
        }
        else if (ReceiveMsg.Header.MessageType == KPC_PORT_CLOSED ||
                 ReceiveMsg.Header.MessageType == KPC_CLIENT_DIED ||
                 ReceiveMsg.Header.DataLength < (sizeof(ULONG) + sizeof(VEASTATUS)))
        {
            /* Notifikasi port ditutup / klien mati */
            CsrpRemoveProcess(ClientPid);
        }
        else
        {
            USHORT ServerId = CSR_GET_SERVER_ID(ReceiveMsg.ApiNumber);
            USHORT FuncId   = CSR_GET_FUNCTION_ID(ReceiveMsg.ApiNumber);
            BOOLEAN ShouldReply = TRUE;
            VEASTATUS ApiStatus = STATUS_INVALID_PARAMETER;

            PCSR_PROCESS ClientProc = CsrLocateProcessById(ClientPid);

            if (ServerId < gServerDllCapacity &&
                gServerDlls[ServerId] != NULL &&
                gServerDlls[ServerId]->ApiTable != NULL &&
                FuncId < gServerDlls[ServerId]->ApiCount &&
                gServerDlls[ServerId]->ApiTable[FuncId] != NULL)
            {
                ApiStatus = gServerDlls[ServerId]->ApiTable[FuncId](
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

/* EOF */