/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : crt_cui.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VEA Console Subsystem Startup
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "crt_internal.h"

extern int __cdecl main(int argc, char *argv[], char *envp[]);

VOID
VEAPI
mainCRTStartup(VOID)
{
    PPEB Peb = NtCurrentPeb();
    PRTL_USER_PROCESS_PARAMETERS Params = NULL;
    PVOID Heap = RtlGetProcessHeap();
    PCHAR *ArgList = NULL;
    PCHAR *argv = NULL;
    PCHAR *envp = NULL;
    PCHAR Dest = NULL;
    INT argc = 0;
    INT ExitCode = 0;

    if (Peb && Peb->ProcessParameters)
    {
        Params = RtlNormalizeProcessParams(Peb->ProcessParameters);
    }

    ArgList = (PCHAR *)RtlAllocateHeap(Heap, HEAP_ZERO_MEMORY, 512 * sizeof(PCHAR));
    if (!ArgList)
    {
        VzTerminateProcess(NtCurrentProcess(), STATUS_NO_MEMORY);
        return;
    }
    argv = ArgList;

    if (Params && Params->CommandLine.Buffer && Params->CommandLine.Length > 0)
    {
        ULONG Len = Params->CommandLine.Length;
        Dest = (PCHAR)RtlAllocateHeap(Heap, HEAP_ZERO_MEMORY, Len + 1);
        PCHAR Src = Params->CommandLine.Buffer;

        if (Dest)
        {
            while (*Src != '\0')
            {
                BOOLEAN InQuotes = FALSE;
                while (*Src == ' ' || *Src == '\t') Src++;
                if (*Src == '\0') break;

                *ArgList++ = Dest;
                argc++;

                while (*Src != '\0')
                {
                    if (*Src == '"') { InQuotes = !InQuotes; Src++; continue; }
                    if (!InQuotes && (*Src == ' ' || *Src == '\t')) break;
                    *Dest++ = *Src++;
                }
                *Dest++ = '\0';
            }
        }
    }
    *ArgList++ = NULL;

    envp = ArgList;
    if (Params && Params->Environment)
    {
        PCHAR EnvSrc = (PCHAR)Params->Environment;
        while (*EnvSrc != '\0')
        {
            *ArgList++ = EnvSrc;
            while (*EnvSrc++ != '\0');
        }
    }
    *ArgList++ = NULL;

    if (Params && Params->DebugFlags != 0)
    {
        __asm__ __volatile__("int $3");
    }

    CrtpRunInitArray();
    ExitCode = main(argc, argv, envp);
    CrtpRunFiniArray();

    VzTerminateProcess(NtCurrentProcess(), (VEASTATUS)ExitCode);

    while (1)
    {
        __asm__ __volatile__("pause");
    }
}