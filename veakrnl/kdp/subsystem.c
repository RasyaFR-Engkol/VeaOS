/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : subsystem.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Subsystem dumping utility
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 15-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file subsystem.c
 * --------------------------------------------------------------------- */

typedef enum _KDP_SUBSYSTEM_NO
{
    Ks = 0,
    Hct,
    Rm,
    Fs,
    Kpc,
    Mm,
    Ob,
    Pt,
    Sf,
    Ul,
    Vk
} KDP_SUBSYSTEM_NO;

/*
 * Compare a subsystem name with an ASCII literal without considering case.
 */
static BOOLEAN
KdpSubsystemNameEquals(
    IN PCHAR String,
    IN PCHAR Name
)
{
    ULONG NameLength = (ULONG)RtlRawStringLength(Name);
    ULONG StringLength = 0;

    /* Count only the first token from the complete debugger command. */
    while (String[StringLength] != '\0' && String[StringLength] != ' ')
    {
        StringLength++;
    }

    /* A subsystem name matches only when the first token has equal length. */
    if (StringLength != NameLength)
    {
        return FALSE;
    }

    /* Normalize ASCII letters while comparing each character. */
    for (ULONG CharacterIndex = 0; CharacterIndex < NameLength; CharacterIndex++)
    {
        CHAR StringCharacter = String[CharacterIndex];
        CHAR NameCharacter = Name[CharacterIndex];

        if (StringCharacter >= 'a' && StringCharacter <= 'z')
        {
            StringCharacter = (CHAR)(StringCharacter - ('a' - 'A'));
        }

        if (NameCharacter >= 'a' && NameCharacter <= 'z')
        {
            NameCharacter = (CHAR)(NameCharacter - ('a' - 'A'));
        }

        if (StringCharacter != NameCharacter)
        {
            return FALSE;
        }
    }

    return TRUE;
}

/*
 * Identify separators accepted between debugger command arguments.
 */
static BOOLEAN
KdpIsArgumentSeparator(IN CHAR Character)
{
    /* Accept spaces and tabs as argument separators. */
    return Character == ' ' || Character == '\t';
}

/*
 * Translate a textual debugger subsystem name into its enum value.
 */
KDP_SUBSYSTEM_NO
VEAPI
KdpTranslateSubsystem(IN PCHAR String)
{
    /* Map each accepted subsystem command to its numeric identifier. */
    if (KdpSubsystemNameEquals(String, "Ks")) return Ks;
    if (KdpSubsystemNameEquals(String, "Hct")) return Hct;
    if (KdpSubsystemNameEquals(String, "Rm")) return Rm;
    if (KdpSubsystemNameEquals(String, "Fs")) return Fs;
    if (KdpSubsystemNameEquals(String, "Kpc")) return Kpc;
    if (KdpSubsystemNameEquals(String, "Mm")) return Mm;
    if (KdpSubsystemNameEquals(String, "Ob")) return Ob;
    if (KdpSubsystemNameEquals(String, "Pt")) return Pt;
    if (KdpSubsystemNameEquals(String, "Sf")) return Sf;
    if (KdpSubsystemNameEquals(String, "Ul")) return Ul;
    if (KdpSubsystemNameEquals(String, "Vk")) return Vk;

    /* The caller contract guarantees a valid subsystem name. */
    return Ks;
}

BOOLEAN
VEAPI
KdpSeparateArgument(
    IN PCHAR String,
    OUT PCHAR SubsystemCommand,
    IN ULONG SubsystemCommandSize,
    OUT PULONG ArgumentCount,
    OUT PCHAR *Argument,
    IN ULONG ArgumentCapacity,
    OUT PCHAR ArgumentStorage,
    IN ULONG ArgumentStorageSize
)
{
    ULONG SubsystemLength = 0;
    ULONG ArgumentStart;
    ULONG ArgumentIndex = 0;
    ULONG StorageOffset = 0;

    /* Validate the input and output buffers before splitting the command. */
    if (!String || !SubsystemCommand || !ArgumentCount || !Argument ||
        !ArgumentStorage || SubsystemCommandSize == 0 ||
        ArgumentCapacity == 0 || ArgumentStorageSize == 0)
    {
        return FALSE;
    }

    /* Find the separator between the subsystem and its command. */
    while (String[SubsystemLength] != '\0' &&
           !KdpIsArgumentSeparator(String[SubsystemLength]))
    {
        SubsystemLength++;
    }

    /* Reject input that does not contain a subsystem token. */
    if (SubsystemLength == 0 || SubsystemLength >= SubsystemCommandSize)
    {
        return FALSE;
    }

    /* Skip separators so the argument list starts with the actual command. */
    ArgumentStart = SubsystemLength;
    while (KdpIsArgumentSeparator(String[ArgumentStart]))
    {
        ArgumentStart++;
    }

    /* Copy the subsystem token into its null-terminated output buffer. */
    RtlZeroMemory(SubsystemCommand, SubsystemCommandSize);
    RtlCopyRawStringN(SubsystemCommand, String, SubsystemLength);
    SubsystemCommand[SubsystemLength] = '\0';

    /* Initialize the argument outputs before tokenizing the remaining text. */
    *ArgumentCount = 0;
    RtlZeroMemory(ArgumentStorage, ArgumentStorageSize);

    /* Split each remaining word and point Argument at its copied token. */
    while (String[ArgumentStart] != '\0')
    {
        ULONG TokenStart = ArgumentStart;
        ULONG TokenLength;

        if (ArgumentIndex >= ArgumentCapacity)
        {
            return FALSE;
        }

        while (String[ArgumentStart] != '\0' &&
               !KdpIsArgumentSeparator(String[ArgumentStart]))
        {
            ArgumentStart++;
        }

        TokenLength = ArgumentStart - TokenStart;
        if (StorageOffset + TokenLength + 1 > ArgumentStorageSize)
        {
            return FALSE;
        }

        Argument[ArgumentIndex] = &ArgumentStorage[StorageOffset];
        RtlCopyRawStringN(
            Argument[ArgumentIndex],
            &String[TokenStart],
            TokenLength
        );
        Argument[ArgumentIndex][TokenLength] = '\0';
        StorageOffset += TokenLength + 1;
        ArgumentIndex++;

        while (KdpIsArgumentSeparator(String[ArgumentStart]))
        {
            ArgumentStart++;
        }
    }

    *ArgumentCount = ArgumentIndex;

    return TRUE;
}

BOOLEAN
VEAPI
KdpSubsystemDump(IN PCHAR String)
{
    KDP_SUBSYSTEM_NO SubsystemNumber;
    CHAR SubsystemCommand[32];
    CHAR ArgumentStorage[256];
    PCHAR Argument[16];
    ULONG ArgumentCount;

    /* Validate the complete subsystem command before splitting it. */
    if (!String || String[0] == '\0')
    {
        return FALSE;
    }

    /* Translate the first token, for example Pt from "Pt dump process". */
    SubsystemNumber = KdpTranslateSubsystem(String);

    /* Split the subsystem token from the remaining debugger command. */
    if (!KdpSeparateArgument(
        String,
        SubsystemCommand,
        sizeof(SubsystemCommand),
        &ArgumentCount,
        Argument,
        sizeof(Argument) / sizeof(Argument[0]),
        ArgumentStorage,
        sizeof(ArgumentStorage)))
    {
        return FALSE;
    }

    /* Switch between subsystem */
    switch (SubsystemNumber)
    {
        /* Pass the tokenized arguments into the subsystem handlers. */
        case Ks:
        {
            break;
        }

        case Hct:
        {
            break;
        }

        case Rm:
        {
            break;
        }

        case Fs:
        {
            break;
        }

        case Kpc:
        {
            break;
        }

        case Mm:
        {
            break;
        }

        case Ob:
        {
            return ObProcessDebug(ArgumentCount, Argument);
            break;
        }

        case Pt:
        {
            break;
        }

        case Sf:
        {
            break;
        }

        case Ul:
        {
            break;
        }

        case Vk:
        {
            break;
        }

        default:
            break;
    }

    return FALSE;
}