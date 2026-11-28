/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : connect.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : CSR Connect
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <csr.h>

/* Revision History ------------------------------------------------------
 * DATE       : 28-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file connect.c
 * --------------------------------------------------------------------- */

/* VARIABLE ***************************************************************/

HANDLE CsrApiPort = NULL;
BOOLEAN CsrServerProcess = FALSE;

/* PUBLIC *****************************************************************/

VEASTATUS
VEAPI
CsrClientConnectToServer(
    IN PCSTR ObjectDirectory OPTIONAL,
    IN ULONG ServerId,
    IN PVOID ConnectionInformation OPTIONAL,
    IN OUT PULONG ConnectionInformationLength OPTIONAL,
    OUT PBOOLEAN ServerToServer OPTIONAL
)
{
    ANSI_STRING PortName;
    VEASTATUS Status;

    UNREFERENCED_PARAMETER(ObjectDirectory);
    UNREFERENCED_PARAMETER(ServerId);

    //
    // Initialize ServerToServer and make sure we never connect
    // to ourselves and make sure we have connected before and
    // just return
    //

    if (ServerToServer)
    {
        *ServerToServer = FALSE;
    }

    if (CsrServerProcess)
    {
        if (ServerToServer)
        {
            *ServerToServer = TRUE;
        }
        return STATUS_SUCCESS;
    }

    if (CsrApiPort != NULL)
    {
        return STATUS_SUCCESS;
    }

    //
    // Open connection to /VeaApiPort
    //

    RtlInitAnsiString(&PortName, "/VeaApiPort");

    Status = VzConnectPort(
        &CsrApiPort,
        &PortName,
        NULL,
        NULL,
        NULL,
        KPC_MODE_MESSAGE,
        ConnectionInformation,
        ConnectionInformationLength
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[CSRLIB] VzConnectPort(/VeaApiPort) failed: 0x%08X\n\r", Status);
        CsrApiPort = NULL;
        return Status;
    }

    return STATUS_SUCCESS;
}

/* EOF */