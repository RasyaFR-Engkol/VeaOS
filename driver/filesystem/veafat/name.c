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

static
BOOLEAN
FatpMatchWildcard(
    IN PANSI_STRING Pattern,
    IN const CHAR *Name,
    IN ULONG NameLen
)
{
    if (!Pattern || !Pattern->Buffer || Pattern->Length == 0)
    {
        return TRUE;
    }

    if ((Pattern->Length == 1 && Pattern->Buffer[0] == '*') ||
        (Pattern->Length == 3 && RtlCompareMemory(Pattern->Buffer, "*.*", 3) == 3))
    {
        return TRUE;
    }

    ULONG p = 0, n = 0;
    ULONG StarIdx = (ULONG)-1, MatchIdx = 0;

    while (n < NameLen)
    {
        if (p < Pattern->Length && (Pattern->Buffer[p] == '?' ||
            RtlUpperChar(Pattern->Buffer[p]) == RtlUpperChar(Name[n])))
        {
            p++;
            n++;
        }
        else if (p < Pattern->Length && Pattern->Buffer[p] == '*')
        {
            StarIdx = p++;
            MatchIdx = n;
        }
        else if (StarIdx != (ULONG)-1)
        {
            p = StarIdx + 1;
            n = ++MatchIdx;
        }
        else
        {
            return FALSE;
        }
    }

    while (p < Pattern->Length && Pattern->Buffer[p] == '*')
    {
        p++;
    }

    return (BOOLEAN)(p == Pattern->Length);
}

static
ULONG
FatpFormatSfnToReadable(
    IN const UCHAR Sfn[11],
    OUT CHAR OutName[13]
)
{
    ULONG OutLen = 0;
    ULONG NameEnd = 8;
    while (NameEnd > 0 && Sfn[NameEnd - 1] == ' ') NameEnd--;

    for (ULONG i = 0; i < NameEnd; i++)
    {
        OutName[OutLen++] = (CHAR)Sfn[i];
    }

    ULONG ExtEnd = 11;
    while (ExtEnd > 8 && Sfn[ExtEnd - 1] == ' ') ExtEnd--;

    if (ExtEnd > 8)
    {
        OutName[OutLen++] = '.';
        for (ULONG i = 8; i < ExtEnd; i++)
        {
            OutName[OutLen++] = (CHAR)Sfn[i];
        }
    }

    OutName[OutLen] = '\0';
    return OutLen;
}

VOID
FatpGenerateSfnWithTail(
    IN PANSI_STRING InputName,
    IN ULONG Tail,
    OUT UCHAR Sfn[11]
)
{
    RtlFillMemory(Sfn, 11, ' ');

    if (!FatpNeedsLfn(InputName))
    {
        FatpFormatSfn(InputName, Sfn);
        return;
    }

    /* Format string tail: "~1", "~2", ..., "~10", dst. */
    CHAR TailStr[12];
    CHAR Digits[10];
    ULONG TailLen = 0;
    ULONG DigitCount = 0;
    ULONG Temp = (Tail == 0) ? 1 : Tail;

    while (Temp > 0 && DigitCount < 10)
    {
        Digits[DigitCount++] = (CHAR)('0' + (Temp % 10));
        Temp /= 10;
    }

    TailStr[TailLen++] = '~';
    while (DigitCount > 0)
    {
        TailStr[TailLen++] = Digits[--DigitCount];
    }

    ULONG MaxBase = (TailLen < 8) ? (8 - TailLen) : 0;

    /* Lewati spasi & titik di awal nama (misal ".gitignore") */
    ULONG Start = 0;
    while (Start < InputName->Length &&
           (InputName->Buffer[Start] == '.' || InputName->Buffer[Start] == ' '))
    {
        Start++;
    }

    /* Cari titik terakhir untuk memisahkan ekstensi */
    LONG LastDot = -1;
    for (LONG k = (LONG)InputName->Length - 1; k > (LONG)Start; k--)
    {
        if (InputName->Buffer[k] == '.')
        {
            LastDot = k;
            break;
        }
    }

    ULONG NameEnd = (LastDot > 0) ? (ULONG)LastDot : InputName->Length;
    ULONG j = 0;

    for (ULONG i = Start; i < NameEnd && j < MaxBase; i++)
    {
        CHAR c = InputName->Buffer[i];
        if (c == ' ' || c == '.') continue;
        if (c == '+' || c == ',' || c == ';' || c == '=' || c == '[' || c == ']')
        {
            c = '_';
        }
        Sfn[j++] = (UCHAR)RtlUpperChar(c);
    }

    for (ULONG t = 0; t < TailLen && j < 8; t++)
    {
        Sfn[j++] = (UCHAR)TailStr[t];
    }

    if (LastDot > 0)
    {
        j = 8;
        for (ULONG i = (ULONG)LastDot + 1; i < InputName->Length && j < 11; i++)
        {
            CHAR c = InputName->Buffer[i];
            if (c == ' ' || c == '.') continue;
            if (c == '+' || c == ',' || c == ';' || c == '=' || c == '[' || c == ']')
            {
                c = '_';
            }
            Sfn[j++] = (UCHAR)RtlUpperChar(c);
        }
    }
}

BOOLEAN
FatpSfnExistsInDirectory(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN ULONG DirStartCluster,
    IN const UCHAR TargetSfn[11]
)
{
    ULONG CurrentCluster = (DirStartCluster == 0) ? Vcb->RootDirStartCluster : DirStartCluster;
    PUCHAR SectorBuffer = UlAllocatePoolZero(NonPagedPool, Vcb->BytesPerSector, VEAFAT_POOL_TAG);
    if (!SectorBuffer)
    {
        return FALSE;
    }

    ULONG EntriesPerSector = Vcb->BytesPerSector / sizeof(FAT_DIR_ENTRY);

    while (!FAT32_IS_EOF(CurrentCluster) && CurrentCluster >= 2)
    {
        ULONGLONG ClusterLba = FAT32_CLUSTER_TO_LBA(Vcb, CurrentCluster);

        for (ULONG s = 0; s < Vcb->SectorsPerCluster; s++)
        {
            if (!VEA_SUCCESS(FatReadDiskSectors(Vcb->TargetDeviceObject, ClusterLba + s, 1, Vcb->BytesPerSector, SectorBuffer)))
            {
                UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
                return FALSE;
            }

            PFAT_DIR_ENTRY Entries = (PFAT_DIR_ENTRY)SectorBuffer;
            for (ULONG i = 0; i < EntriesPerSector; i++)
            {
                if (Entries[i].FileName[0] == 0x00)
                {
                    UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
                    return FALSE;
                }
                if (Entries[i].FileName[0] == 0xE5 || Entries[i].Attributes == FAT_ATTR_LFN)
                {
                    continue;
                }
                if (RtlCompareMemory(Entries[i].FileName, (PVOID)TargetSfn, 11) == 11)
                {
                    UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
                    return TRUE;
                }
            }
        }

        if (!VEA_SUCCESS(FatpGetNextCluster(Vcb, CurrentCluster, &CurrentCluster)))
        {
            break;
        }
    }

    UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
    return FALSE;
}

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
    OUT PUSHORT TargetSlot
)
{
    RtlCopyMemory(&TargetSlot[0], LfnEntry->Name1, 5 * sizeof(USHORT));
    RtlCopyMemory(&TargetSlot[5], LfnEntry->Name2, 6 * sizeof(USHORT));
    RtlCopyMemory(&TargetSlot[11], LfnEntry->Name3, 2 * sizeof(USHORT));
}

BOOLEAN
FatpCompareLfnWithAnsi(
    IN PUSHORT LfnBuffer,
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
    ULONG LfnCount = (NameLen + 12) / 13;
    USHORT Utf16Name[256];

    RtlZeroMemory(Utf16Name, sizeof(Utf16Name));

    for (ULONG i = 0; i < NameLen; i++)
    {
        Utf16Name[i] = (USHORT)(UCHAR)InputName->Buffer[i];
    }

    Utf16Name[NameLen] = 0x0000;
    for (ULONG i = NameLen + 1; i < (LfnCount * 13); i++)
    {
        Utf16Name[i] = 0xFFFF;
    }

    RtlZeroMemory(LfnEntriesBuffer, LfnCount * sizeof(FAT_LFN_ENTRY));

    for (ULONG Seq = 1; Seq <= LfnCount; Seq++)
    {
        PFAT_LFN_ENTRY Lfn = &LfnEntriesBuffer[LfnCount - Seq];

        Lfn->SequenceNumber = (UCHAR)Seq;
        if (Seq == LfnCount)
        {
            Lfn->SequenceNumber |= FAT_LFN_LAST_ENTRY;
        }

        Lfn->Attributes = FAT_ATTR_LFN;
        Lfn->Type = 0x00;
        Lfn->Checksum = SfnChecksum;
        Lfn->FirstClusterLow = 0x0000;

        ULONG CharOffset = (Seq - 1) * 13;
        RtlCopyMemory(Lfn->Name1, &Utf16Name[CharOffset + 0], 5 * sizeof(USHORT));
        RtlCopyMemory(Lfn->Name2, &Utf16Name[CharOffset + 5], 6 * sizeof(USHORT));
        RtlCopyMemory(Lfn->Name3, &Utf16Name[CharOffset + 11], 2 * sizeof(USHORT));
    }

    return LfnCount;
}

VEASTATUS
VEAPI
FatMjDirectoryControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK Stack
)
{
    PFAT32_DEVICE_EXTENSION Vcb = EXTENSION_FROM_DEVOBJ(DeviceObject);
    PFILE_OBJECT FileObject     = Stack->FileObject ? (PFILE_OBJECT)Stack->FileObject : Oip->OriginalFileObject;
    ULONG BufferLength          = Stack->Parameters.QueryDirectory.Length;
    PANSI_STRING SearchPattern  = Stack->Parameters.QueryDirectory.FileName;
    FILE_INFORMATION_CLASS InfoClass = Stack->Parameters.QueryDirectory.FileInformationClass;
    PUCHAR UserBuffer           = Oip->UserBuffer ? (PUCHAR)Oip->UserBuffer : (PUCHAR)Oip->AssociatedOip.SystemBuffer;

    if (Vcb->IsCdo || Stack->MinorFunction != OIP_MN_QUERY_DIRECTORY ||
        !FileObject || !FileObject->FsContext || !FileObject->FsContext2 || !UserBuffer)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        return STATUS_INVALID_PARAMETER;
    }

    PVEAFAT_FCB Fcb = (PVEAFAT_FCB)FileObject->FsContext;
    PVEAFAT_CCB Ccb = (PVEAFAT_CCB)FileObject->FsContext2;

    if (!BooleanFlagOn(Fcb->Attributes, FAT_ATTR_DIRECTORY))
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        return STATUS_INVALID_PARAMETER;
    }

    if (Stack->Flags & SL_RESTART_SCAN)
    {
        Ccb->CurrentByteOffset = 0;
    }

    /* Simpan search pattern pada panggilan pertama */
    if (!Ccb->DirectorySearchFilter.Buffer)
    {
        ANSI_STRING DefaultWild;
        RtlInitAnsiString(&DefaultWild, "*");
        PANSI_STRING SrcPat = (SearchPattern && SearchPattern->Buffer && SearchPattern->Length > 0)
                              ? SearchPattern : &DefaultWild;

        if (!VEA_SUCCESS(RtlDuplicateAnsiString(RTL_DUPLICATE_ANSI_STRING_NULL_TERMINATE, SrcPat, &Ccb->DirectorySearchFilter)))
        {
            Oip->StatusInformation.Status = STATUS_INSUFFICIENT_RESOURCES;
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    PUCHAR SectorBuffer = UlAllocatePoolZero(NonPagedPool, Vcb->BytesPerSector, VEAFAT_POOL_TAG);
    if (!SectorBuffer)
    {
        Oip->StatusInformation.Status = STATUS_INSUFFICIENT_RESOURCES;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Lompat ke cluster sesuai Ccb->CurrentByteOffset */
    ULONG ClusterIndex   = Ccb->CurrentByteOffset / Vcb->BytesPerCluster;
    ULONG OffsetInClus   = Ccb->CurrentByteOffset % Vcb->BytesPerCluster;
    ULONG CurrentCluster = Fcb->FirstCluster;

    for (ULONG c = 0; c < ClusterIndex && CurrentCluster >= 2 && !FAT32_IS_EOF(CurrentCluster); c++)
    {
        if (!VEA_SUCCESS(FatpGetNextCluster(Vcb, CurrentCluster, &CurrentCluster)))
        {
            UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
            Oip->StatusInformation.Status = STATUS_UNEXPECTED_IO_ERROR;
            return STATUS_UNEXPECTED_IO_ERROR;
        }
    }

    USHORT LfnWchar[256];
    CHAR ResolvedName[260];
    BOOLEAN HasLfn = FALSE;
    UCHAR LfnChecksum = 0;
    ULONG BytesWritten = 0;
    ULONG LastNextEntryOffset = 0;
    BOOLEAN FirstScan = (Ccb->CurrentByteOffset == 0);
    BOOLEAN SingleEntry = (Stack->Flags & SL_RETURN_SINGLE_ENTRY) != 0;

    while (CurrentCluster >= 2 && !FAT32_IS_EOF(CurrentCluster))
    {
        ULONGLONG ClusterLba = FAT32_CLUSTER_TO_LBA(Vcb, CurrentCluster);
        ULONG StartSector = OffsetInClus / Vcb->BytesPerSector;
        ULONG StartEntry  = (OffsetInClus % Vcb->BytesPerSector) / sizeof(FAT_DIR_ENTRY);
        ULONG EntriesPerSector = Vcb->BytesPerSector / sizeof(FAT_DIR_ENTRY);

        for (ULONG s = StartSector; s < Vcb->SectorsPerCluster; s++)
        {
            VEASTATUS Status = FatReadDiskSectors(Vcb->TargetDeviceObject, ClusterLba + s, 1, Vcb->BytesPerSector, SectorBuffer);
            if (!VEA_SUCCESS(Status))
            {
                UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
                Oip->StatusInformation.Status = Status;
                return Status;
            }

            PFAT_DIR_ENTRY Entries = (PFAT_DIR_ENTRY)SectorBuffer;

            for (ULONG i = (s == StartSector ? StartEntry : 0); i < EntriesPerSector; i++)
            {
                ULONG EntryBytePos = (ClusterIndex * Vcb->BytesPerCluster) +
                                     (s * Vcb->BytesPerSector) +
                                     (i * sizeof(FAT_DIR_ENTRY));

                if (Entries[i].FileName[0] == 0x00)
                {
                    UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
                    if (BytesWritten > 0)
                    {
                        *(PULONG)(UserBuffer + LastNextEntryOffset) = 0;
                        Oip->StatusInformation.Status = STATUS_SUCCESS;
                        Oip->StatusInformation.Information = BytesWritten;
                        return STATUS_SUCCESS;
                    }
                    VEASTATUS EndStatus = FirstScan ? STATUS_NO_SUCH_FILE : STATUS_NO_MORE_FILES;
                    Oip->StatusInformation.Status = EndStatus;
                    Oip->StatusInformation.Information = 0;
                    return EndStatus;
                }

                if (Entries[i].FileName[0] == 0xE5)
                {
                    HasLfn = FALSE;
                    Ccb->CurrentByteOffset = EntryBytePos + sizeof(FAT_DIR_ENTRY);
                    continue;
                }

                if (Entries[i].Attributes == FAT_ATTR_LFN)
                {
                    PFAT_LFN_ENTRY LfnEntry = (PFAT_LFN_ENTRY)&Entries[i];
                    if (LfnEntry->SequenceNumber & FAT_LFN_LAST_ENTRY)
                    {
                        RtlZeroMemory(LfnWchar, sizeof(LfnWchar));
                        HasLfn = TRUE;
                        LfnChecksum = LfnEntry->Checksum;
                    }
                    ULONG SeqIdx = LfnEntry->SequenceNumber & FAT_LFN_SEQ_MASK;
                    if (SeqIdx >= 1 && SeqIdx <= 20)
                    {
                        ULONG CharOff = (SeqIdx - 1) * 13;
                        if (CharOff + 13 < 256)
                        {
                            FatpExtractLfnChars(LfnEntry, &LfnWchar[CharOff]);
                        }
                    }
                    Ccb->CurrentByteOffset = EntryBytePos + sizeof(FAT_DIR_ENTRY);
                    continue;
                }

                if (Entries[i].Attributes & FAT_ATTR_VOLUME_ID)
                {
                    HasLfn = FALSE;
                    Ccb->CurrentByteOffset = EntryBytePos + sizeof(FAT_DIR_ENTRY);
                    continue;
                }

                /* Ekstrak nama LFN jika checksum valid, jika tidak fallback ke SFN 8.3 */
                ULONG NameLen = 0;
                if (HasLfn && FatpCalculateSfnChecksum(Entries[i].FileName) == LfnChecksum)
                {
                    while (NameLen < 255 && LfnWchar[NameLen] != 0x0000 && LfnWchar[NameLen] != 0xFFFF)
                    {
                        ResolvedName[NameLen] = (CHAR)(LfnWchar[NameLen] & 0xFF);
                        NameLen++;
                    }
                    ResolvedName[NameLen] = '\0';
                }
                else
                {
                    NameLen = FatpFormatSfnToReadable(Entries[i].FileName, ResolvedName);
                }
                HasLfn = FALSE;

                if (!FatpMatchWildcard(&Ccb->DirectorySearchFilter, ResolvedName, NameLen))
                {
                    Ccb->CurrentByteOffset = EntryBytePos + sizeof(FAT_DIR_ENTRY);
                    continue;
                }

                /* Hitung ukuran struct output berdasarkan InfoClass */
                ULONG BaseStructSize = (InfoClass == FileBothDirectoryInformation)
                                       ? FIELD_OFFSET(FILE_BOTH_DIR_INFORMATION, FileName)
                                       : FIELD_OFFSET(FILE_DIRECTORY_INFORMATION, FileName);
                ULONG EntryTotalSize = ALIGN_UP_BY(BaseStructSize + NameLen, 8);

                if (BytesWritten + EntryTotalSize > BufferLength)
                {
                    UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
                    if (BytesWritten == 0)
                    {
                        Oip->StatusInformation.Status = STATUS_BUFFER_TOO_SMALL;
                        return STATUS_BUFFER_TOO_SMALL;
                    }
                    *(PULONG)(UserBuffer + LastNextEntryOffset) = 0;
                    Oip->StatusInformation.Status = STATUS_SUCCESS;
                    Oip->StatusInformation.Information = BytesWritten;
                    return STATUS_SUCCESS;
                }

                PFILE_DIRECTORY_INFORMATION DirInfo = (PFILE_DIRECTORY_INFORMATION)(UserBuffer + BytesWritten);
                RtlZeroMemory(DirInfo, EntryTotalSize);
                DirInfo->NextEntryOffset  = EntryTotalSize;
                DirInfo->FileIndex        = EntryBytePos / sizeof(FAT_DIR_ENTRY);
                DirInfo->EndOfFile.QuadPart = (Entries[i].Attributes & FAT_ATTR_DIRECTORY) ? 0 : Entries[i].FileSize;
                DirInfo->AllocationSize.QuadPart = (Entries[i].Attributes & FAT_ATTR_DIRECTORY)
                                                   ? 0 : ALIGN_UP_BY((ULONGLONG)Entries[i].FileSize, Vcb->BytesPerCluster);
                DirInfo->FileAttributes   = Entries[i].Attributes ? Entries[i].Attributes : FILE_ATTRIBUTE_NORMAL;
                DirInfo->FileNameLength   = NameLen;

                if (InfoClass == FileBothDirectoryInformation)
                {
                    PFILE_BOTH_DIR_INFORMATION BothInfo = (PFILE_BOTH_DIR_INFORMATION)DirInfo;
                    CHAR ShortReadable[13];
                    BothInfo->ShortNameLength = (CHAR)FatpFormatSfnToReadable(Entries[i].FileName, ShortReadable);
                    RtlCopyMemory(BothInfo->ShortName, ShortReadable, BothInfo->ShortNameLength);
                    RtlCopyMemory(BothInfo->FileName, ResolvedName, NameLen);
                }
                else
                {
                    RtlCopyMemory(DirInfo->FileName, ResolvedName, NameLen);
                }

                LastNextEntryOffset = BytesWritten;
                BytesWritten += EntryTotalSize;
                Ccb->CurrentByteOffset = EntryBytePos + sizeof(FAT_DIR_ENTRY);

                if (SingleEntry)
                {
                    *(PULONG)(UserBuffer + LastNextEntryOffset) = 0;
                    UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
                    Oip->StatusInformation.Status = STATUS_SUCCESS;
                    Oip->StatusInformation.Information = BytesWritten;
                    return STATUS_SUCCESS;
                }
            }
        }

        OffsetInClus = 0;
        ClusterIndex++;
        if (!VEA_SUCCESS(FatpGetNextCluster(Vcb, CurrentCluster, &CurrentCluster)))
        {
            break;
        }
    }

    UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);

    if (BytesWritten > 0)
    {
        *(PULONG)(UserBuffer + LastNextEntryOffset) = 0;
        Oip->StatusInformation.Status = STATUS_SUCCESS;
        Oip->StatusInformation.Information = BytesWritten;
        return STATUS_SUCCESS;
    }

    VEASTATUS FinalStatus = FirstScan ? STATUS_NO_SUCH_FILE : STATUS_NO_MORE_FILES;
    Oip->StatusInformation.Status = FinalStatus;
    Oip->StatusInformation.Information = 0;
    return FinalStatus;
}

/* EOF */