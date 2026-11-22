/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : ppb.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Process Parameter Block
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <rtl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 15-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file ppb.c
 * --------------------------------------------------------------------- */

#if defined(BUILD_USER)

VEASTATUS
VEAPI
RtlCreateProcessParameters(
    OUT PRTL_USER_PROCESS_PARAMETERS *ProcessParameters,
    IN PANSI_STRING ImagePathName,
    IN PANSI_STRING DllPath OPTIONAL,
    IN PANSI_STRING CurrentDirectory OPTIONAL,
    IN PANSI_STRING CommandLine OPTIONAL,
    IN PVOID Environment OPTIONAL,
    IN PANSI_STRING WindowTitle OPTIONAL,
    IN PANSI_STRING DesktopInfo OPTIONAL,
    IN PANSI_STRING ShellInfo OPTIONAL,
    IN PANSI_STRING RuntimeData OPTIONAL
)
{
    PRTL_USER_PROCESS_PARAMETERS Params;
    PANSI_STRING EffectiveCmdLine = (CommandLine && CommandLine->Buffer) ? CommandLine : ImagePathName;
    ULONG PathLen = (ImagePathName && ImagePathName->Buffer) ? ImagePathName->Length : 0;
    ULONG CmdLen  = (EffectiveCmdLine && EffectiveCmdLine->Buffer) ? EffectiveCmdLine->Length : 0;
    ULONG TotalSize = sizeof(RTL_USER_PROCESS_PARAMETERS)
                    + (PathLen ? PathLen + sizeof(CHAR) : 0)
                    + (CmdLen  ? CmdLen  + sizeof(CHAR) : 0);
    PCHAR StringData;

    UNREFERENCED_PARAMETER(DllPath);
    UNREFERENCED_PARAMETER(CurrentDirectory);
    UNREFERENCED_PARAMETER(Environment);
    UNREFERENCED_PARAMETER(WindowTitle);
    UNREFERENCED_PARAMETER(DesktopInfo);
    UNREFERENCED_PARAMETER(ShellInfo);
    UNREFERENCED_PARAMETER(RuntimeData);

    if (!ProcessParameters)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Alokasi 1 blok datar di Process Heap: [Header PPB] + [ImagePathName] + [CommandLine]
    Params = (PRTL_USER_PROCESS_PARAMETERS)RtlAllocateHeap(
        RtlGetProcessHeap(),
        HEAP_ZERO_MEMORY,
        TotalSize
    );
    if (!Params)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Params->MaximumLength = TotalSize;
    Params->Length = TotalSize;
    Params->Flags = RTL_USER_PROCESS_PARAMETERS_NORMALIZED;

    StringData = (PCHAR)(Params + 1);

    if (PathLen > 0)
    {
        RtlCopyMemory(StringData, ImagePathName->Buffer, PathLen);
        StringData[PathLen] = '\0';

        Params->ImagePathName.Length = (USHORT)PathLen;
        Params->ImagePathName.MaximumLength = (USHORT)(PathLen + sizeof(CHAR));
        Params->ImagePathName.Buffer = StringData;

        StringData += PathLen + sizeof(CHAR);
    }

    if (CmdLen > 0)
    {
        RtlCopyMemory(StringData, EffectiveCmdLine->Buffer, CmdLen);
        StringData[CmdLen] = '\0';

        Params->CommandLine.Length = (USHORT)CmdLen;
        Params->CommandLine.MaximumLength = (USHORT)(CmdLen + sizeof(CHAR));
        Params->CommandLine.Buffer = StringData;
    }

    *ProcessParameters = Params;
    return STATUS_SUCCESS;
}

VOID
VEAPI
RtlDestroyProcessParameters(
    IN PRTL_USER_PROCESS_PARAMETERS ProcessParameters
)
{
    if (ProcessParameters)
    {
        RtlFreeHeap(RtlGetProcessHeap(), 0, ProcessParameters);
    }
}

PRTL_USER_PROCESS_PARAMETERS
VEAPI
RtlNormalizeProcessParams(
    IN OUT PRTL_USER_PROCESS_PARAMETERS Params
)
{
    if (!Params)
    {
        return NULL;
    }

    if (!(Params->Flags & RTL_USER_PROCESS_PARAMETERS_NORMALIZED))
    {
        ULONG_PTR Base = (ULONG_PTR)Params;

        if (Params->CurrentDirectory.DosPath.Buffer)
            Params->CurrentDirectory.DosPath.Buffer = (PCHAR)(Base + (ULONG_PTR)Params->CurrentDirectory.DosPath.Buffer);

        if (Params->DllPath.Buffer)
            Params->DllPath.Buffer = (PCHAR)(Base + (ULONG_PTR)Params->DllPath.Buffer);

        if (Params->ImagePathName.Buffer)
            Params->ImagePathName.Buffer = (PCHAR)(Base + (ULONG_PTR)Params->ImagePathName.Buffer);

        if (Params->CommandLine.Buffer)
            Params->CommandLine.Buffer = (PCHAR)(Base + (ULONG_PTR)Params->CommandLine.Buffer);

        if (Params->WindowTitle.Buffer)
            Params->WindowTitle.Buffer = (PCHAR)(Base + (ULONG_PTR)Params->WindowTitle.Buffer);

        if (Params->DesktopInfo.Buffer)
            Params->DesktopInfo.Buffer = (PCHAR)(Base + (ULONG_PTR)Params->DesktopInfo.Buffer);

        if (Params->ShellInfo.Buffer)
            Params->ShellInfo.Buffer = (PCHAR)(Base + (ULONG_PTR)Params->ShellInfo.Buffer);

        if (Params->RuntimeData.Buffer)
            Params->RuntimeData.Buffer = (PCHAR)(Base + (ULONG_PTR)Params->RuntimeData.Buffer);

        Params->Flags |= RTL_USER_PROCESS_PARAMETERS_NORMALIZED;
    }

    return Params;
}

#endif

PRTL_USER_PROCESS_PARAMETERS
VEAPI
RtlDeNormalizeProcessParams(
    IN OUT PRTL_USER_PROCESS_PARAMETERS Params
)
{
    if (!Params)
    {
        return NULL;
    }

    if (Params->Flags & RTL_USER_PROCESS_PARAMETERS_NORMALIZED)
    {
        ULONG_PTR Base = (ULONG_PTR)Params;

        if (Params->CurrentDirectory.DosPath.Buffer)
            Params->CurrentDirectory.DosPath.Buffer = (PCHAR)((ULONG_PTR)Params->CurrentDirectory.DosPath.Buffer - Base);

        if (Params->DllPath.Buffer)
            Params->DllPath.Buffer = (PCHAR)((ULONG_PTR)Params->DllPath.Buffer - Base);

        if (Params->ImagePathName.Buffer)
            Params->ImagePathName.Buffer = (PCHAR)((ULONG_PTR)Params->ImagePathName.Buffer - Base);

        if (Params->CommandLine.Buffer)
            Params->CommandLine.Buffer = (PCHAR)((ULONG_PTR)Params->CommandLine.Buffer - Base);

        if (Params->WindowTitle.Buffer)
            Params->WindowTitle.Buffer = (PCHAR)((ULONG_PTR)Params->WindowTitle.Buffer - Base);

        if (Params->DesktopInfo.Buffer)
            Params->DesktopInfo.Buffer = (PCHAR)((ULONG_PTR)Params->DesktopInfo.Buffer - Base);

        if (Params->ShellInfo.Buffer)
            Params->ShellInfo.Buffer = (PCHAR)((ULONG_PTR)Params->ShellInfo.Buffer - Base);

        if (Params->RuntimeData.Buffer)
            Params->RuntimeData.Buffer = (PCHAR)((ULONG_PTR)Params->RuntimeData.Buffer - Base);

        Params->Flags &= ~RTL_USER_PROCESS_PARAMETERS_NORMALIZED;
    }

    return Params;
}
