/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : exception.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Handle Exception for Veakrnl
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <hal.h>

/* Revision History ------------------------------------------------------
 * DATE       : 23-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file exception.c
 * --------------------------------------------------------------------- */

BOOLEAN HctNMIInProgress = FALSE;

VOID
VEAPI
HctHandleNMI(VOID)
{
    SYSTEM_CONTROL_PORT_B_REGISTER SystemControl;

    if (HctNMIInProgress++)
        KdpDebugMailboxPrint("***STOP: RECURSIVE NMI!!!\n");

    SystemControl.Bits = __In8(SYSTEM_CONTROL_PORT_B);

    BvAcquireDisplayState(TRUE);
    BvSetColorText(0xFFFFFFFF);
    BvSetColorBackground(0xFF000080);
    BvClearScreen();

    BvPrintLog("\n***FATAL: Hardware Malfunction\n\n");
    BvPrintLog("Call your hardware vendor for support \n\n");

    if (SystemControl.ParityCheck)
    {
       BvPrintLog("NMI: Parity Check / Memory Parity Error\n");
    }

    if (SystemControl.ChannelCheck)
    {
        BvPrintLog("NMI: Channel Check / IOCHK\n");
    }

    BvPrintLog("\n\n*** The system has been halted ***\n\n");

    while (TRUE)
        __YieldProcessor();
}