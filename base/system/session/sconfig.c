/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : sconfig.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Session Config
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "session.h"

/* Revision History ------------------------------------------------------
 * DATE       : 16-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file sconfig.c
 * --------------------------------------------------------------------- */

BOOLEAN SmpUseWinTerminal = FALSE;
BOOLEAN SmpMiniVeaMode = FALSE;
CHAR SmpSubSystemPath[260] = "/D1P1:/VeaOS/SystemReserved/csrss.elf";
CHAR SmpSessionUiPath[260] = "/D1P1:/VeaOS/SystemReserved/VeaLogonScr.elf";
CHAR SmpVeapiPath[260]     = "/D1P1:/VeaOS/SystemReserved/veapi.ko";

BOOLEAN
SmpStrEqualI(
    IN PCHAR Str1,
    IN PCHAR Str2
)
{
    if (!Str1 || !Str2) return FALSE;

    while (*Str1 && *Str2)
    {
        CHAR c1 = *Str1;
        CHAR c2 = *Str2;
        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
        if (c1 != c2) return FALSE;
        Str1++;
        Str2++;
    }

    return (*Str1 == '\0' && *Str2 == '\0');
}

static
VOID
SmpCopyStringSafe(
    OUT PCHAR Dest,
    IN ULONG DestSize,
    IN PCHAR Src,
    IN ULONG SrcLen
)
{
    ULONG CopyLen = SrcLen;
    if (CopyLen >= DestSize)
    {
        CopyLen = DestSize - 1;
    }
    RtlCopyMemory(Dest, Src, CopyLen);
    Dest[CopyLen] = '\0';
}

VOID
VEAPI
SmpInspectEnvironment(
    IN PCHAR EnvBlock
)
{
    PCHAR Cursor = EnvBlock;

    if (!Cursor)
    {
        KdPrintf("[SMSS] Warning: envp NULL!\n");
        return;
    }

    KdPrintf("[SMSS] Environment Block:\n");
    while (*Cursor != '\0')
    {
        ULONG Len = 0;
        KdPrintf("  -> %s\n", Cursor);
        while (Cursor[Len] != '\0') Len++;
        Cursor += (Len + 1);
    }
}

VOID
VEAPI
SmpParseCommandLine(
    IN int argc,
    IN char *argv[]
)
{
    for (int i = 1; i < argc; i++)
    {
        if (!argv[i]) continue;

        if (SmpStrEqualI(argv[i], "/WinTerminal"))
        {
            SmpUseWinTerminal = TRUE;
        }
        else if (SmpStrEqualI(argv[i], "/MiniVea"))
        {
            SmpMiniVeaMode = TRUE;
        }
    }
}

static
BOOLEAN
SmpQueryRegistryString(
    IN HANDLE KeyHandle,
    IN PCHAR ValueNameStr,
    OUT PCHAR OutBuffer,
    IN ULONG OutBufferSize
)
{
    ANSI_STRING ValueName;
    CHAR DataBuffer[260];
    ULONG ValueType = 0;
    ULONG ResultLength = 0;
    VEASTATUS Status;

    RtlInitAnsiString(&ValueName, ValueNameStr);
    RtlZeroMemory(DataBuffer, sizeof(DataBuffer));

    Status = VzQueryValueKey(
        KeyHandle,
        &ValueName,
        &ValueType,
        DataBuffer,
        sizeof(DataBuffer),
        &ResultLength
    );

    /* Sesuaikan VKEY_SZ / REG_SZ (nilai 1) sesuai enum tipe di header vk.h */
    if (!VEA_SUCCESS(Status) || ValueType != FILE_KEY_STRING || ResultLength == 0)
    {
        return FALSE;
    }

    SmpCopyStringSafe(OutBuffer, OutBufferSize, DataBuffer, ResultLength);
    return TRUE;
}

VEASTATUS
VEAPI
SmpLoadConfiguration(
    IN int argc,
    IN char *argv[],
    IN PCHAR envp
)
{
    OBJECT_ATTRIBUTES ObjAttr;
    ANSI_STRING KeyPath;
    HANDLE SmssKeyHandle = NULL;
    VEASTATUS Status;

    SmpInspectEnvironment(envp);
    SmpParseCommandLine(argc, argv);

    /* Coba buka key Session Manager di Registry */
    RtlInitAnsiString(&KeyPath, "/Registry/Machine/System/CurrentControlSet/Control/SessionManager");
    InitializeObjectAttributes(&ObjAttr, &KeyPath, 0, NULL, NULL);

    Status = VzOpenVeaKey(&SmssKeyHandle, KEY_READ, &ObjAttr);
    if (VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] Registry SessionManager terbuka, membaca konfigurasi...\n");
        SmpQueryRegistryString(SmssKeyHandle, "SubSystem", SmpSubSystemPath, sizeof(SmpSubSystemPath));
        SmpQueryRegistryString(SmssKeyHandle, "InitialCommand", SmpSessionUiPath, sizeof(SmpSessionUiPath));
        VzClose(SmssKeyHandle);
    }
    else
    {
        KdPrintf("[SMSS] Key Registry belum ada (0x%08X), pakai konfigurasi default.\n", Status);
    }

    /* Boot flag /CLIMODE (/WinTerminal) menimpa target UI default */
    if (SmpUseWinTerminal)
    {
        SmpCopyStringSafe(
            SmpSessionUiPath,
            sizeof(SmpSessionUiPath),
            "/D1P1:/VeaOS/SystemReserved/winterm.elf",
            39
        );
    }

    KdPrintf("[SMSS] Mode CLI     : %s\n", SmpUseWinTerminal ? "YA (/WinTerminal)" : "TIDAK (Grafis)");
    KdPrintf("[SMSS] Target SubSys: %s\n", SmpSubSystemPath);
    KdPrintf("[SMSS] Target UI    : %s\n", SmpSessionUiPath);

    return STATUS_SUCCESS;
}

/* EOF */
