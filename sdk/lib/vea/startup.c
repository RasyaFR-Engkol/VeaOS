/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : startup.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VEA Startup
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "pttypes.h"
#include <procbind.h>
#include <veastatus.h>
#include <rtl.h>
#include <vz_wrappers.h>
#include <psdk/winuser.h>

/* Revision History ------------------------------------------------------
 * DATE       : 15-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file startup.c
 * --------------------------------------------------------------------- */

typedef VOID (__cdecl *PVEA_INIT_FN)(VOID);

extern PVEA_INIT_FN __init_array_start[] __attribute__((weak));
extern PVEA_INIT_FN __init_array_end[]   __attribute__((weak));
extern PVEA_INIT_FN __fini_array_start[] __attribute__((weak));
extern PVEA_INIT_FN __fini_array_end[]   __attribute__((weak));

static VOID
CrtpRunInitArray(VOID)
{
    if (!__init_array_start || !__init_array_end) return;

    for (PVEA_INIT_FN *Fn = __init_array_start; Fn < __init_array_end; Fn++)
    {
        if (*Fn) (*Fn)();
    }
}

static VOID
CrtpRunFiniArray(VOID)
{
    if (!__fini_array_start || !__fini_array_end) return;

    for (PVEA_INIT_FN *Fn = __fini_array_end; Fn > __fini_array_start; )
    {
        Fn--;
        if (*Fn) (*Fn)();
    }
}

VEASTATUS
VEACDECL
_main(
    int argc,
    char *argv[],
    char *envp,
    ULONG debugflags
);

extern int __cdecl main(int argc, char *argv[], char *envp[]);
int 
WINAPI 
WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PSTR     lpCmdLine,
    int       nCmdShow
);

PRTL_USER_PROCESS_PARAMETERS
VEAPI
RtlNormalizeProcessParams(
    IN OUT PRTL_USER_PROCESS_PARAMETERS Params
);

#if 0 // see crt_cui.c

static PSTR
CrtpExtractWinMainCmdLine(
    IN PRTL_USER_PROCESS_PARAMETERS Params
)
{
    if (!Params || !Params->CommandLine.Buffer || Params->CommandLine.Length == 0)
    {
        return "";
    }

    PSTR Cmd = Params->CommandLine.Buffer;
    BOOLEAN InQuotes = FALSE;

    /* Lewati token pertama (path executable), hormati tanda kutip "..." */
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

    /* Lewati spasi pemisah sebelum argumen pertama */
    while (*Cmd == ' ' || *Cmd == '\t')
    {
        Cmd++;
    }

    return Cmd;
}

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

    /* Alokasi array pointer gabungan untuk argv + NULL + envp + NULL (gaya ReactOS) */
    ArgList = (PCHAR *)RtlAllocateHeap(Heap, HEAP_ZERO_MEMORY, 512 * sizeof(PCHAR));
    argv = ArgList;

    if (Params && Params->CommandLine.Buffer && Params->CommandLine.Length > 0)
    {
        ULONG Len = Params->CommandLine.Length;
        Dest = (PCHAR)RtlAllocateHeap(Heap, HEAP_ZERO_MEMORY, Len + 1);
        PCHAR Src = Params->CommandLine.Buffer;

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
    *ArgList++ = NULL; /* Terminator argv */

    /* Susun array envp tepat setelah terminator argv */
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
    *ArgList++ = NULL; /* Terminator envp */

    if (Params && Params->DebugFlags != 0)
    {
        __asm__ __volatile__("int $3");
    }

    CrtpRunInitArray();
    ExitCode = main(argc, argv, envp);
    CrtpRunFiniArray();

    VzTerminateProcess(NtCurrentProcess(), (VEASTATUS)ExitCode);
}

#endif

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

        /* Pakai CommandLine; kalau kosong fallback ke ImagePathName */
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

            /* Lewati spasi / tab pemisah argumen */
            while (Src < End && (*Src == ' ' || *Src == '\t'))
            {
                Src++;
            }

            if (Src >= End || *Src == '\0')
            {
                break;
            }

            argv[argc++] = Dst;

            /* Salin satu token (mendukung argumen ber-tanda kutip "...") */
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

    Status = _main(argc, argv, envp, DebugFlags);

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

#if 0 // See crt_gui.c

VOID
VEAPI
WinMainCRTStartup(VOID)
{
    PPEB Peb = NtCurrentPeb();
    PRTL_USER_PROCESS_PARAMETERS Params = NULL;
    HINSTANCE hInstance = NULL;
    PSTR lpCmdLine = "";
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

    VzTerminateProcess(NtCurrentProcess(), (VEASTATUS)ExitCode);
}

#endif

/* EOF */