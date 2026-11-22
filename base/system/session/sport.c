/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : sport.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : SMSS Port
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "session.h"

/* Revision History ------------------------------------------------------
 * DATE       : 16-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file sport.c
 * --------------------------------------------------------------------- */

HANDLE SmpApiPort = NULL;
HANDLE SmpApiThreadHandle = NULL;
volatile BOOLEAN SmpSubsystemRegistered = FALSE;
extern HANDLE gGuiSubsystemReadyEvent;

static
VOID
VEAPI
SmpClientCommLoop(
    IN PVOID Context
)
{
    HANDLE ServerCommHandle = (HANDLE)Context;
    VEASTATUS Status;
    UCHAR RequestBuffer[512];
    UCHAR ReplyBuffer[512];
    PKPC_MESSAGE_HEADER RequestMsg = (PKPC_MESSAGE_HEADER)RequestBuffer;
    PKPC_MESSAGE_HEADER ReplyMsg = NULL;

    KdPrintf("[SMSS/KPC] Comm thread aktif untuk handle=%p\n", ServerCommHandle);

    while (TRUE)
    {
        RtlZeroMemory(RequestBuffer, sizeof(RequestBuffer));

        Status = VzReplyWaitReceivePort(
            ServerCommHandle,
            NULL,
            ReplyMsg,
            RequestMsg
        );

        if (!VEA_SUCCESS(Status))
        {
            KdPrintf("[SMSS/KPC] Comm port %p error/putus: 0x%08X\n", ServerCommHandle, Status);
            ReplyMsg = NULL;
            break;
        }

        ReplyMsg = NULL;

        switch (RequestMsg->MessageType)
        {
            case KPC_REQUEST:
            {
                KdPrintf("[SMSS/KPC] KPC_REQUEST dari PID=%p (MsgId=%u, Len=%u)\n",
                         RequestMsg->ClientId.UniqueProcess,
                         RequestMsg->MessageId,
                         RequestMsg->DataLength);

                /* Siapkan reply header untuk dikirim di awal iterasi VzReplyWaitReceivePort berikutnya */
                RtlZeroMemory(ReplyBuffer, sizeof(ReplyBuffer));
                ReplyMsg = (PKPC_MESSAGE_HEADER)ReplyBuffer;
                ReplyMsg->ClientId    = RequestMsg->ClientId;
                ReplyMsg->MessageId   = RequestMsg->MessageId;
                ReplyMsg->MessageType = KPC_REPLY;
                ReplyMsg->DataLength  = 0;
                ReplyMsg->TotalLength = sizeof(KPC_MESSAGE_HEADER);
                break;
            }

            case KPC_DATAGRAM:
            {
                KdPrintf("[SMSS/KPC] KPC_DATAGRAM dari PID=%p\n",
                         RequestMsg->ClientId.UniqueProcess);
                break;
            }

            case KPC_PORT_CLOSED:
            case KPC_CLIENT_DIED:
            {
                KdPrintf("[SMSS/KPC] Client terputus (PID=%p)\n",
                         RequestMsg->ClientId.UniqueProcess);
                VzClose(ServerCommHandle);
                return;
            }

            default:
            {
                KdPrintf("[SMSS/KPC] MessageType tidak dikenal: 0x%X\n",
                         RequestMsg->MessageType);
                break;
            }
        }
    }

    VzClose(ServerCommHandle);
}

static
VOID
VEAPI
SmpApiLoop(
    IN PVOID Context
)
{
    VEASTATUS Status;
    UCHAR RequestBuffer[512];
    PKPC_MESSAGE_HEADER RequestMsg = (PKPC_MESSAGE_HEADER)RequestBuffer;

    (VOID)Context;
    KdPrintf("[SMSS/KPC] Listen thread /SmApiPort aktif...\n");

    while (TRUE)
    {
        RtlZeroMemory(RequestBuffer, sizeof(RequestBuffer));

        /* Listen port tidak pernah mengirim ReplyMessage (ConnectedPort == NULL) */
        Status = VzReplyWaitReceivePort(
            SmpApiPort,
            NULL,
            NULL,
            RequestMsg
        );

        if (!VEA_SUCCESS(Status))
        {
            KdPrintf("[SMSS/KPC] VzReplyWaitReceivePort(/SmApiPort) error: 0x%08X\n", Status);
            VzYieldExecution();
            continue;
        }

        if (RequestMsg->MessageType == KPC_CONNECTION_REQUEST)
        {
            HANDLE ServerCommHandle = NULL;
            HANDLE CommThreadHandle = NULL;
            CLIENT_ID CommClientId;

            KdPrintf("[SMSS/KPC] Connection request masuk dari PID=%p\n",
                     RequestMsg->ClientId.UniqueProcess);

            Status = VzAcceptConnectPort(
                &ServerCommHandle,
                NULL,
                RequestMsg,
                TRUE,
                NULL,
                NULL
            );

            if (!VEA_SUCCESS(Status))
            {
                KdPrintf("[SMSS/KPC] VzAcceptConnectPort gagal: 0x%08X\n", Status);
                continue;
            }

            SmpSubsystemRegistered = TRUE;
            VzSetEvent(gGuiSubsystemReadyEvent, NULL);
            KdPrintf("[SMSS/KPC] Koneksi diterima! ServerCommHandle=%p\n", ServerCommHandle);

            /* Jalankan thread dedicated untuk melayani komunikasi di ServerCommHandle */
            Status = RtlCreateUserThread(
                NtCurrentProcess(),
                NULL,
                FALSE,
                0,
                0,
                0,
                (PTHREAD_START_ROUTINE)SmpClientCommLoop,
                (PVOID)ServerCommHandle,
                &CommThreadHandle,
                &CommClientId
            );

            if (VEA_SUCCESS(Status))
            {
                VzClose(CommThreadHandle);
            }
            else
            {
                KdPrintf("[SMSS/KPC] Gagal bikin comm thread: 0x%08X\n", Status);
                VzClose(ServerCommHandle);
            }
        }
    }
}

VEASTATUS
VEAPI
SmpCreateApiPort(
    VOID
)
{
    OBJECT_ATTRIBUTES ObjAttr;
    ANSI_STRING PortName;
    CLIENT_ID ClientId;
    VEASTATUS Status;

    RtlInitAnsiString(&PortName, "/SmApiPort");
    InitializeObjectAttributes(&ObjAttr, &PortName, 0, NULL, NULL);

    /* MaxConnectionInfoLength = 256, MaxMessageLength = 512 */
    Status = VzCreatePort(&SmpApiPort, &ObjAttr, 256, 512, 0);
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] VzCreatePort(/SmApiPort) gagal: 0x%08X\n", Status);
        return Status;
    }

    KdPrintf("[SMSS] Port /SmApiPort berhasil dibuat (Handle=%p)\n", SmpApiPort);

    /* Nyalakan listener thread LPC */
    Status = RtlCreateUserThread(
        NtCurrentProcess(),
        NULL,
        FALSE,              /* Langsung jalan tanpa suspended */
        0,
        0,
        0,
        (PTHREAD_START_ROUTINE)SmpApiLoop,
        NULL,
        &SmpApiThreadHandle,
        &ClientId
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] Gagal membuat worker thread LPC: 0x%08X\n", Status);
        return Status;
    }

    KdPrintf("[SMSS] Worker thread LPC berjalan (TID=%p)\n", ClientId.UniqueThread);
    return STATUS_SUCCESS;
}