/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : api.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : CSR API for CsrLib
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <csr.h>

/* Revision History ------------------------------------------------------
 * DATE       : 28-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file api.c
 * --------------------------------------------------------------------- */

/* PRIVATE ****************************************************************/

extern HANDLE CsrApiPort;
extern BOOLEAN CsrServerProcess;

/* PUBLIC *****************************************************************/

VEASTATUS
VEAPI
CsrClientCallServer(
    IN OUT PKPC_MESSAGE_HEADER ApiMessage,
    IN ULONG ApiNumber,
    IN ULONG DataLength
)
{
    PCSR_KPC_MESSAGE Msg = (PCSR_KPC_MESSAGE)ApiMessage;
    ULONG PayloadLength;
    VEASTATUS Status;

    if (!Msg || DataLength > sizeof(Msg->Data))
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (CsrServerProcess)
    {
        return STATUS_SUCCESS;
    }

    if (!CsrApiPort)
    {
        return STATUS_PORT_DISCONNECTED;
    }

    /* Payload KPC = ApiNumber (4B) + Status (4B) + DataLength */
    PayloadLength = sizeof(ULONG) + sizeof(VEASTATUS) + DataLength;

    ApiMessage->DataLength     = (USHORT)PayloadLength;
    ApiMessage->TotalLength    = (USHORT)(sizeof(KPC_MESSAGE_HEADER) + PayloadLength);
    ApiMessage->MessageType    = KPC_REQUEST;
    ApiMessage->DataInfoOffset = 0;

    Msg->ApiNumber = ApiNumber;
    Msg->Status    = STATUS_PENDING;

    Status = VzRequestWaitReplyPort(
        CsrApiPort,
        ApiMessage,
        ApiMessage
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[CSRLIB] VzRequestWaitReplyPort(0x%08X) gagal: 0x%08X\n\r",
                 ApiNumber, Status);
        return Status;
    }

    return Msg->Status;
}

/* EOF */