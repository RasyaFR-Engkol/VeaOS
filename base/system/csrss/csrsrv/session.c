/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : session.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : CSR Session
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <csr.h>

/* Revision History ------------------------------------------------------
 * DATE       : 28-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file session.c
 * --------------------------------------------------------------------- */

HANDLE hSbApiPort  = NULL;

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