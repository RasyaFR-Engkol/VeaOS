/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : crt_gui.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VEA GUI Subsystem Startup
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "crt_internal.h"
#include <userapi.h>
#include <kernelapi.h>

int
WINAPI
WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PSTR      lpCmdLine,
    int       nCmdShow
);

static PSTR
CrtpExtractWinMainCmdLine(
    IN PRTL_USER_PROCESS_PARAMETERS Params
)
{
    static CHAR EmptyCmd[] = "";

    if (!Params || !Params->CommandLine.Buffer || Params->CommandLine.Length == 0)
    {
        return EmptyCmd;
    }

    PSTR Cmd = Params->CommandLine.Buffer;
    BOOLEAN InQuotes = FALSE;

    while (*Cmd != '\0')
    {
        if (*Cmd == '"')
        {
            InQuotes = !InQuotes;
        }
        else if (!InQuotes && (*Cmd == ' ' || *Cmd == '\t'))
        {
            break;
        }
        Cmd++;
    }

    while (*Cmd == ' ' || *Cmd == '\t')
    {
        Cmd++;
    }

    return Cmd;
}

VOID
VEAPI
WinMainCRTStartup(VOID)
{
    PPEB Peb = NtCurrentPeb();
    PRTL_USER_PROCESS_PARAMETERS Params = NULL;
    HINSTANCE hInstance = NULL;
    PSTR lpCmdLine = CrtpExtractWinMainCmdLine(NULL);
    INT nCmdShow = 10; /* SW_SHOWDEFAULT */
    INT ExitCode = 0;

    if (Peb)
    {
        hInstance = (HINSTANCE)Peb->ImageBaseAddress;

        if (Peb->ProcessParameters)
        {
            Params = RtlNormalizeProcessParams(Peb->ProcessParameters);
            lpCmdLine = CrtpExtractWinMainCmdLine(Params);

            if (Params->WindowFlags & 0x00000001 /* STARTF_USESHOWWINDOW */)
            {
                nCmdShow = (INT)Params->ShowWindowFlags;
            }

            if (Params->DebugFlags != 0)
            {
                __asm__ __volatile__("int $3");
            }
        }
    }

    CrtpRunInitArray();
    ExitCode = WinMain(hInstance, NULL, lpCmdLine, nCmdShow);
    CrtpRunFiniArray();

    // Exit via kernelapi
    ExitProcess(ExitCode);

    while (1)
    {
        __asm__ __volatile__("pause");
    }
}