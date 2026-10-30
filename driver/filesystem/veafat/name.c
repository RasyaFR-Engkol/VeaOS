/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : name.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : FAT32 Name
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "veafat.h"

/* Revision History ------------------------------------------------------
 * DATE       : 17-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file name.c
 * --------------------------------------------------------------------- */

VOID
FatpFormatSfn(
    IN PANSI_STRING InputName,
    OUT UCHAR Sfn[11]
)
{
    ULONG i = 0, j = 0;
    
    /* Isi buffer dengan spasi (0x20) */
    RtlFillMemory(Sfn, 11, ' ');

    /* Format Nama (Maksimal 8 karakter) */
    while (i < InputName->Length && InputName->Buffer[i] != '.' && j < 8)
    {
        Sfn[j++] = (UCHAR)RtlUpperChar(InputName->Buffer[i++]);
    }

    /* Skip karakter sampai ketemu titik '.' jika nama > 8 karakter */
    while (i < InputName->Length && InputName->Buffer[i] != '.')
    {
        i++;
    }

    /* Format Ekstensi (Maksimal 3 karakter) */
    if (i < InputName->Length && InputName->Buffer[i] == '.')
    {
        i++; // Skip titik
        j = 8;
        while (i < InputName->Length && j < 11)
        {
            Sfn[j++] = (UCHAR)RtlUpperChar(InputName->Buffer[i++]);
        }
    }
}

UCHAR
FatpCalculateSfnChecksum(
    IN const UCHAR Sfn[11]
)
{
    UCHAR Checksum = 0;
    for (ULONG i = 0; i < 11; i++)
    {
        Checksum = ((Checksum & 1) ? 0x80 : 0) + (Checksum >> 1) + Sfn[i];
    }
    return Checksum;
}

VOID
FatpExtractLfnChars(
    IN PFAT_LFN_ENTRY LfnEntry,
    OUT PWCHAR TargetSlot
)
{
    RtlCopyMemory(&TargetSlot[0], LfnEntry->Name1, 5 * sizeof(WCHAR));
    RtlCopyMemory(&TargetSlot[5], LfnEntry->Name2, 6 * sizeof(WCHAR));
    RtlCopyMemory(&TargetSlot[11], LfnEntry->Name3, 2 * sizeof(WCHAR));
}

BOOLEAN
FatpCompareLfnWithAnsi(
    IN PWCHAR LfnBuffer,
    IN PANSI_STRING ComponentName
)
{
    ULONG LfnLen = 0;

    /* Hitung panjang string WCHAR (berhenti jika 0x0000 atau 0xFFFF) */
    while (LfnLen < 255 && LfnBuffer[LfnLen] != 0x0000 && LfnBuffer[LfnLen] != 0xFFFF)
    {
        LfnLen++;
    }

    if (LfnLen != ComponentName->Length)
    {
        return FALSE;
    }

    for (ULONG i = 0; i < LfnLen; i++)
    {
        CHAR C1 = (CHAR)RtlUpperChar((UCHAR)LfnBuffer[i]);
        CHAR C2 = (CHAR)RtlUpperChar((UCHAR)ComponentName->Buffer[i]);

        if (C1 != C2)
        {
            return FALSE;
        }
    }

    return TRUE;
}