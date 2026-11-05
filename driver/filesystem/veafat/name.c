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

BOOLEAN
FatpNeedsLfn(
    IN PANSI_STRING InputName
)
//
// Routine Description:
//     Check if the input filename requires LFN (Long File Name) entries.
//     Returns TRUE if length > 8.3 or contains lowercase / special characters.
//
{
    ULONG NameLen = 0;
    ULONG ExtLen = 0;
    BOOLEAN HasExt = FALSE;

    for (ULONG i = 0; i < InputName->Length; i++)
    {
        CHAR c = InputName->Buffer[i];

        if (c == '.')
        {
            if (HasExt) return TRUE; // Titik ganda -> Butuh LFN
            HasExt = TRUE;
            continue;
        }

        // Karakter kecil atau spesial di luar aturan SFN dasar -> Butuh LFN
        if (c >= 'a' && c <= 'z') return TRUE;
        if (c == ' ' || c == '+' || c == ',' || c == ';' || c == '=' || c == '[' || c == ']') return TRUE;

        if (!HasExt) NameLen++;
        else ExtLen++;
    }

    if (NameLen > 8 || ExtLen > 3) return TRUE;

    return FALSE;
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

VOID
FatpGenerateSfn(
    IN PANSI_STRING InputName,
    OUT UCHAR Sfn[11]
)
//
// Routine Description:
//     Generate short 8.3 name with ~1 tail for long file names.
//     e.g., "minidumpreserved.sys" -> "MINIDU~1SYS"
//
{
    RtlFillMemory(Sfn, 11, ' ');

    if (!FatpNeedsLfn(InputName))
    {
        FatpFormatSfn(InputName, Sfn);
        return;
    }

    // Ambil 6 karakter pertama untuk nama + tail "~1"
    ULONG i = 0, j = 0;
    while (i < InputName->Length && InputName->Buffer[i] != '.' && j < 6)
    {
        CHAR c = InputName->Buffer[i++];
        if (c == ' ') continue;
        Sfn[j++] = (UCHAR)RtlUpperChar(c);
    }

    Sfn[j++] = '~';
    Sfn[j++] = '1';

    // Cari ekstensi
    while (i < InputName->Length && InputName->Buffer[i] != '.') i++;

    if (i < InputName->Length && InputName->Buffer[i] == '.')
    {
        i++;
        j = 8;
        while (i < InputName->Length && j < 11)
        {
            CHAR c = InputName->Buffer[i++];
            if (c == ' ') continue;
            Sfn[j++] = (UCHAR)RtlUpperChar(c);
        }
    }
}

ULONG
FatpGenerateLfnEntries(
    IN PANSI_STRING InputName,
    IN UCHAR SfnChecksum,
    OUT PFAT_LFN_ENTRY LfnEntriesBuffer
)
//
// Routine Description:
//     Fills an array of FAT_LFN_ENTRY structs for the given file name.
//     Returns the total number of LFN entries generated.
//
{
    ULONG NameLen = InputName->Length;
    ULONG LfnCount = (NameLen + 12) / 13; // 13 karakter Unicode per entri LFN
    WCHAR WcharName[256];

    RtlZeroMemory(WcharName, sizeof(WcharName));

    // Convert ANSI byte ke WCHAR (ASCII padding)
    for (ULONG i = 0; i < NameLen; i++)
    {
        WcharName[i] = (WCHAR)(UCHAR)InputName->Buffer[i];
    }

    // Isi terminator 0x0000 dan padding 0xFFFF sisa string
    WcharName[NameLen] = 0x0000;
    for (ULONG i = NameLen + 1; i < (LfnCount * 13); i++)
    {
        WcharName[i] = 0xFFFF;
    }

    RtlZeroMemory(LfnEntriesBuffer, LfnCount * sizeof(FAT_LFN_ENTRY));

    for (ULONG Seq = 1; Seq <= LfnCount; Seq++)
    {
        // FAT LFN disimpan dari urutan terbesar (Sequence N) ke 1 di disk
        PFAT_LFN_ENTRY Lfn = &LfnEntriesBuffer[LfnCount - Seq];

        Lfn->SequenceNumber = (UCHAR)Seq;
        if (Seq == LfnCount)
        {
            Lfn->SequenceNumber |= FAT_LFN_LAST_ENTRY; // Bit 0x40 penanda entri LFN paling atas
        }

        Lfn->Attributes = FAT_ATTR_LFN; // 0x0F
        Lfn->Type = 0x00;
        Lfn->Checksum = SfnChecksum;
        Lfn->FirstClusterLow = 0x0000;

        ULONG CharOffset = (Seq - 1) * 13;
        RtlCopyMemory(Lfn->Name1, &WcharName[CharOffset + 0], 5 * sizeof(WCHAR));
        RtlCopyMemory(Lfn->Name2, &WcharName[CharOffset + 5], 6 * sizeof(WCHAR));
        RtlCopyMemory(Lfn->Name3, &WcharName[CharOffset + 11], 2 * sizeof(WCHAR));
    }

    return LfnCount;
}