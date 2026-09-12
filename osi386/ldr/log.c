/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : log.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Abstraction to LogUI Engine
 ----- Effective since 07-2026 til FOREVER ------------------------------- */

#include <osi386.h>

/* Revision History ------------------------------------------------------
 * DATE       : 26-07-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file log.c
 * --------------------------------------------------------------------- */
BOOLEAN InTextMode = TRUE;
CHAR LdrLastCheckedFile[256];

VOID
VEAPI
LdrInfo(const PCHAR String)
{
    if(InTextMode)
    {
        bios_print_string("INFO: ");
        bios_print_string(String);
        bios_print_string("\r\r");
        return;
    }
    BvPrintLog("INFO: ");
    BvPrintLog(String);
}

VOID
VEAPI
LdrWarning(const PCHAR String)
{
    if(InTextMode)
    {
        bios_print_string("WARNING: ");
        bios_print_string(String);
        bios_print_string("\r\r");
        return;
    }
    BvPrintLog("WARNING: ");
    BvPrintLog(String);
}

VOID
VEAPI
LdrError(IN ULONG ErrorCode)
{
    CHAR String[16];
    cmemset(String, 0, sizeof(String));
    chextoascii(ErrorCode, String);

    if(InTextMode)
    {
        bios_print_string("ERROR: ");
        bios_print_string(String);
        bios_print_string("\r\r");
        restart_n_message();
        return;
    }

    // Call Bv internal API
    BvErrorLog(String, LdrLastCheckedFile);

    wait_for_keypress();
    force_reboot();
}