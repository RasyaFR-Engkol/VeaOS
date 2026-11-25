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

static
ULONG
RtlpCalcParamStringSize(
    IN PANSI_STRING Str OPTIONAL
)
{
    if (Str && Str->Buffer && Str->Length > 0)
    {
        return (ULONG)Str->Length + sizeof(CHAR);
    }
    return 0;
}

static
VOID
RtlpCopyParamString(
    IN OUT PCHAR *Cursor,
    OUT PANSI_STRING Dest,
    IN PANSI_STRING Src OPTIONAL
)
{
    if (Src && Src->Buffer && Src->Length > 0)
    {
        RtlCopyMemory(*Cursor, Src->Buffer, Src->Length);
        (*Cursor)[Src->Length] = '\0';

        Dest->Length = Src->Length;
        Dest->MaximumLength = (USHORT)(Src->Length + sizeof(CHAR));
        Dest->Buffer = *Cursor;

        *Cursor += Src->Length + sizeof(CHAR);
    }
}

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
    ULONG TotalSize = sizeof(RTL_USER_PROCESS_PARAMETERS);
    PCHAR StringData;

    if (!ProcessParameters)
    {
        return STATUS_INVALID_PARAMETER;
    }

    TotalSize += RtlpCalcParamStringSize(CurrentDirectory);
    TotalSize += RtlpCalcParamStringSize(DllPath);
    TotalSize += RtlpCalcParamStringSize(ImagePathName);
    TotalSize += RtlpCalcParamStringSize(EffectiveCmdLine);
    TotalSize += RtlpCalcParamStringSize(WindowTitle);
    TotalSize += RtlpCalcParamStringSize(DesktopInfo);
    TotalSize += RtlpCalcParamStringSize(ShellInfo);
    TotalSize += RtlpCalcParamStringSize(RuntimeData);

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
    Params->Environment = Environment;

    StringData = (PCHAR)(Params + 1);

    RtlpCopyParamString(&StringData, &Params->CurrentDirectory.DosPath, CurrentDirectory);
    RtlpCopyParamString(&StringData, &Params->DllPath, DllPath);
    RtlpCopyParamString(&StringData, &Params->ImagePathName, ImagePathName);
    RtlpCopyParamString(&StringData, &Params->CommandLine, EffectiveCmdLine);
    RtlpCopyParamString(&StringData, &Params->WindowTitle, WindowTitle);
    RtlpCopyParamString(&StringData, &Params->DesktopInfo, DesktopInfo);
    RtlpCopyParamString(&StringData, &Params->ShellInfo, ShellInfo);
    RtlpCopyParamString(&StringData, &Params->RuntimeData, RuntimeData);

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
