/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : crt_native.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VEA Native Process Startup
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "crt_internal.h"

VEASTATUS
VEACDECL
_main(
    int argc,
    char *argv[],
    char *envp,
    ULONG debugflags
);

VOID
VEAPI
VeaProcessStartup(IN PPEB Peb)
{
    VEASTATUS Status;
    PRTL_USER_PROCESS_PARAMETERS Params = NULL;
    PANSI_STRING CmdLine = NULL;
    PVOID Heap = RtlGetProcessHeap();
    PUCHAR RawBuffer = NULL;
    char *FallbackArgv[1] = { NULL };
    char **argv = FallbackArgv;
    char *envp = NULL;
    int argc = 0;
    ULONG DebugFlags = 0;

    if (Peb && Peb->ProcessParameters)
    {
        Params = RtlNormalizeProcessParams(Peb->ProcessParameters);
        DebugFlags = Params->DebugFlags;
        envp = (char *)Params->Environment;

        CmdLine = &Params->CommandLine;
        if (!CmdLine->Buffer || CmdLine->Length == 0)
        {
            CmdLine = &Params->ImagePathName;
        }
    }

    if (CmdLine && CmdLine->Buffer && CmdLine->Length > 0 && Heap)
    {
        SIZE_T CmdLen = CmdLine->Length;
        SIZE_T MaxTokens = (CmdLen / 2) + 2;
        SIZE_T TotalAlloc = (MaxTokens * sizeof(char *)) + CmdLen + 1;

        RawBuffer = (PUCHAR)RtlAllocateHeap(Heap, HEAP_ZERO_MEMORY, TotalAlloc);
        if (!RawBuffer)
        {
            VzTerminateProcess(NtCurrentProcess(), STATUS_NO_MEMORY);
            return;
        }

        argv = (char **)RawBuffer;
        char *Dst = (char *)(RawBuffer + (MaxTokens * sizeof(char *)));
        const char *Src = CmdLine->Buffer;
        const char *End = CmdLine->Buffer + CmdLen;

        while (Src < End && *Src != '\0')
        {
            BOOLEAN InQuotes = FALSE;

            while (Src < End && (*Src == ' ' || *Src == '\t'))
            {
                Src++;
            }

            if (Src >= End || *Src == '\0')
            {
                break;
            }

            argv[argc++] = Dst;

            while (Src < End && *Src != '\0')
            {
                if (*Src == '"')
                {
                    InQuotes = !InQuotes;
                    Src++;
                    continue;
                }

                if (!InQuotes && (*Src == ' ' || *Src == '\t'))
                {
                    break;
                }

                *Dst++ = *Src++;
            }

            *Dst++ = '\0';
        }

        argv[argc] = NULL;
    }

    if (DebugFlags != 0)
    {
        __asm__ __volatile__("int $3");
    }

    CrtpRunInitArray();
    Status = _main(argc, argv, envp, DebugFlags);
    CrtpRunFiniArray();

    if (RawBuffer && Heap)
    {
        RtlFreeHeap(Heap, 0, RawBuffer);
    }

    VzTerminateProcess(NtCurrentProcess(), Status);

    while (1)
    {
        __asm__ __volatile__("pause");
    }
}