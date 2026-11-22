/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : create.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : FAT32 Create Handle to Kernel
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "veafat.h"

/* Revision History ------------------------------------------------------
 * DATE       : 17-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file create.c
 * --------------------------------------------------------------------- */

static
BOOLEAN
FatpIsSlash(CHAR c)
{
    return (c == '/' || c == '\\');
}

static
BOOLEAN
FatpEqualNormalizedPath(
    IN PANSI_STRING PathA,
    IN PANSI_STRING PathB
)
{
    if (!PathA || !PathB) return FALSE;

    USHORT StartA = 0, EndA = PathA->Length;
    USHORT StartB = 0, EndB = PathB->Length;

    /* Abaikan slash di paling depan dan paling belakang */
    while (StartA < EndA && FatpIsSlash(PathA->Buffer[StartA])) StartA++;
    while (EndA > StartA && FatpIsSlash(PathA->Buffer[EndA - 1])) EndA--;

    while (StartB < EndB && FatpIsSlash(PathB->Buffer[StartB])) StartB++;
    while (EndB > StartB && FatpIsSlash(PathB->Buffer[EndB - 1])) EndB--;

    USHORT iA = StartA;
    USHORT iB = StartB;

    while (iA < EndA && iB < EndB)
    {
        BOOLEAN SlashA = FatpIsSlash(PathA->Buffer[iA]);
        BOOLEAN SlashB = FatpIsSlash(PathB->Buffer[iB]);

        if (SlashA || SlashB)
        {
            if (!SlashA || !SlashB) return FALSE;
            /* Lipat slash berurutan (misal "//" == "\") */
            while (iA < EndA && FatpIsSlash(PathA->Buffer[iA])) iA++;
            while (iB < EndB && FatpIsSlash(PathB->Buffer[iB])) iB++;
        }
        else
        {
            if (RtlUpperChar(PathA->Buffer[iA]) != RtlUpperChar(PathB->Buffer[iB]))
            {
                return FALSE;
            }
            iA++;
            iB++;
        }
    }

    return (BOOLEAN)(iA == EndA && iB == EndB);
}

PVEAFAT_FCB
VEAPI
FatGetOrCreateVolumeFcb(IN PFAT32_DEVICE_EXTENSION Vcb)
{
    KIRQL OldIrql;
    PLIST_ENTRY Entry;
    PVEAFAT_FCB Fcb = NULL;

    KsAcquireSpinLock(&Vcb->FcbListLock, &OldIrql);

    for (Entry = Vcb->FcbListHead.Flink; Entry != &Vcb->FcbListHead; Entry = Entry->Flink)
    {
        PVEAFAT_FCB CurrFcb = CONTAINING_RECORD(Entry, VEAFAT_FCB, FcbListEntry);
        if (CurrFcb->IsVolumeFcb)
        {
            FatReferenceFcb(CurrFcb);
            KsReleaseSpinLock(&Vcb->FcbListLock, OldIrql);
            return CurrFcb;
        }
    }

    KsReleaseSpinLock(&Vcb->FcbListLock, OldIrql);

    Fcb = UlAllocatePoolZero(NonPagedPool, sizeof(VEAFAT_FCB), VEAFAT_POOL_TAG);
    if (!Fcb) return NULL;

    Fcb->Vcb = Vcb;
    Fcb->IsVolumeFcb = TRUE;
    Fcb->FirstCluster = 0;
    Fcb->FileSize = (ULONGLONG)Vcb->TotalSectors * Vcb->BytesPerSector;
    Fcb->RefCount = 1;
    Fcb->Attributes = 0;

    ANSI_STRING RootStr;
    RtlInitAnsiString(&RootStr, "/");
    if (!VEA_SUCCESS(RtlDuplicateAnsiString(RTL_DUPLICATE_ANSI_STRING_NULL_TERMINATE, &RootStr, &Fcb->FileName)))
    {
        UlFreePoolWithTag(Fcb, VEAFAT_POOL_TAG);
        return NULL;
    }
    
    KsInitializeSpinlock(&Fcb->FcbLock);

    KsAcquireSpinLock(&Vcb->FcbListLock, &OldIrql);
    InsertTailList(&Vcb->FcbListHead, &Fcb->FcbListEntry);
    KsReleaseSpinLock(&Vcb->FcbListLock, OldIrql);

    return Fcb;
}

PVEAFAT_FCB
VEAPI
FatGetOrCreateRootFcb(
    IN PFAT32_DEVICE_EXTENSION Vcb
)
{
    KIRQL OldIrql;
    PLIST_ENTRY Entry;
    PVEAFAT_FCB Fcb = NULL;

    KsAcquireSpinLock(&Vcb->FcbListLock, &OldIrql);

    for (Entry = Vcb->FcbListHead.Flink; Entry != &Vcb->FcbListHead; Entry = Entry->Flink)
    {
        PVEAFAT_FCB CurrFcb = CONTAINING_RECORD(Entry, VEAFAT_FCB, FcbListEntry);
        if (!CurrFcb->IsVolumeFcb && CurrFcb->FirstCluster == Vcb->RootDirStartCluster)
        {
            FatReferenceFcb(CurrFcb);
            KsReleaseSpinLock(&Vcb->FcbListLock, OldIrql);
            return CurrFcb;
        }
    }

    KsReleaseSpinLock(&Vcb->FcbListLock, OldIrql);

    Fcb = UlAllocatePoolZero(NonPagedPool, sizeof(VEAFAT_FCB), VEAFAT_POOL_TAG);
    if (!Fcb) return NULL;

    Fcb->Vcb = Vcb;
    Fcb->IsVolumeFcb = FALSE;
    Fcb->FirstCluster = Vcb->RootDirStartCluster;
    Fcb->FileSize = 0;
    Fcb->RefCount = 1;
    Fcb->Attributes = FAT_ATTR_DIRECTORY;

    ANSI_STRING RootStr;
    RtlInitAnsiString(&RootStr, "/");
    if (!VEA_SUCCESS(RtlDuplicateAnsiString(RTL_DUPLICATE_ANSI_STRING_NULL_TERMINATE, &RootStr, &Fcb->FileName)))
    {
        UlFreePoolWithTag(Fcb, VEAFAT_POOL_TAG);
        return NULL;
    }

    KsInitializeSpinlock(&Fcb->FcbLock);

    KsAcquireSpinLock(&Vcb->FcbListLock, &OldIrql);
    InsertTailList(&Vcb->FcbListHead, &Fcb->FcbListEntry);
    KsReleaseSpinLock(&Vcb->FcbListLock, OldIrql);

    return Fcb;
}

VEASTATUS
VEAPI
FatLocateFcbEx(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN PANSI_STRING FileName,
    OUT PVEAFAT_FCB *OutFcb,
    OUT PULONG OutParentCluster,
    OUT PANSI_STRING OutLeafName
)
{
    /* 1. Cek Lookup Cache di FcbListHead */
    KIRQL OldIrql;
    PLIST_ENTRY Entry;

    KsAcquireSpinLock(&Vcb->FcbListLock, &OldIrql);
    for (Entry = Vcb->FcbListHead.Flink; Entry != &Vcb->FcbListHead; Entry = Entry->Flink)
    {
        PVEAFAT_FCB Fcb = CONTAINING_RECORD(Entry, VEAFAT_FCB, FcbListEntry);
        if (!Fcb->IsVolumeFcb && FatpEqualNormalizedPath(&Fcb->FileName, FileName))
        {
            Fcb->RefCount++;
            KsReleaseSpinLock(&Vcb->FcbListLock, OldIrql);
            *OutFcb = Fcb;
            return STATUS_SUCCESS;
        }
    }
    KsReleaseSpinLock(&Vcb->FcbListLock, OldIrql);

    /* 2. On-disk searching */
    VEASTATUS Status;
    ULONG CurrentDirCluster = Vcb->RootDirStartCluster;
    FAT_DIR_ENTRY FoundEntry;
    ULONG ParentCluster = Vcb->RootDirStartCluster;
    ULONG EntryOffset = 0;

    USHORT Pos = 0;
    ANSI_STRING Component;

    while (Pos < FileName->Length && (FileName->Buffer[Pos] == '/' || FileName->Buffer[Pos] == '\\'))
    {
        Pos++;
    }

    /* Loop parsing path token per token */
    while (Pos < FileName->Length)
    {
        USHORT StartPos = Pos;

        while (Pos < FileName->Length && FileName->Buffer[Pos] != '/' && FileName->Buffer[Pos] != '\\')
        {
            Pos++;
        }

        Component.Buffer = &FileName->Buffer[StartPos];
        Component.Length = Pos - StartPos;
        Component.MaximumLength = Component.Length;

        while (Pos < FileName->Length && (FileName->Buffer[Pos] == '/' || FileName->Buffer[Pos] == '\\'))
        {
            Pos++;
        }

        ParentCluster = CurrentDirCluster;

        Status = FatpSearchDirectory(
            Vcb,
            CurrentDirCluster,
            &Component,
            &FoundEntry,
            &ParentCluster,
            &EntryOffset
        );

        if (!VEA_SUCCESS(Status))
        {
            // Jika token ini adalah token TERAKHIR (nama file) dan tidak ketemu
            if (Pos >= FileName->Length)
            {
                if (OutParentCluster) *OutParentCluster = CurrentDirCluster;
                if (OutLeafName) *OutLeafName = Component; // Mengembalikan "minidumpreserved.sys"
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            return STATUS_OBJECT_PATH_NOT_FOUND;
        }

        if (Pos < FileName->Length)
        {
            if (!BooleanFlagOn(FoundEntry.Attributes, FAT_ATTR_DIRECTORY))
            {
                return STATUS_OBJECT_PATH_NOT_FOUND;
            }

            CurrentDirCluster = MERGE_CLUSTER_HI_N_LO(FoundEntry.FirstClusterHigh, FoundEntry.FirstClusterLow);
        }
    }

    ULONG TargetCluster = MERGE_CLUSTER_HI_N_LO(FoundEntry.FirstClusterHigh, FoundEntry.FirstClusterLow);

    DPRINT("[VEAFAT] Locate '%Z': FirstCluster=%u, FileSize=%u, Attr=0x%02X\n",
            FileName, TargetCluster, FoundEntry.FileSize, FoundEntry.Attributes);

    if (FoundEntry.FileSize > 0 && TargetCluster < 2)
    {
        DPRINT("[FAT ERROR] Corrupt DirEntry for %Z: FileSize=%u but FirstCluster=%u!\n",
                FileName, FoundEntry.FileSize, TargetCluster);
        return STATUS_CORRUPT_FILE;
    }

    KsAcquireSpinLock(&Vcb->FcbListLock, &OldIrql);
    for (Entry = Vcb->FcbListHead.Flink; Entry != &Vcb->FcbListHead; Entry = Entry->Flink)
    {
        PVEAFAT_FCB ExistingFcb = CONTAINING_RECORD(Entry, VEAFAT_FCB, FcbListEntry);
        if (!ExistingFcb->IsVolumeFcb &&
            ExistingFcb->ParentDirCluster == ParentCluster &&
            ExistingFcb->DirEntryByteOffset == EntryOffset)
        {
            ExistingFcb->RefCount++;
            KsReleaseSpinLock(&Vcb->FcbListLock, OldIrql);
            *OutFcb = ExistingFcb;
            return STATUS_SUCCESS;
        }
    }
    KsReleaseSpinLock(&Vcb->FcbListLock, OldIrql);

    if (BooleanFlagOn(FoundEntry.Attributes, FAT_ATTR_DIRECTORY) && TargetCluster == 0)
    {
        TargetCluster = Vcb->RootDirStartCluster;
    }

    PVEAFAT_FCB NewFcb = UlAllocatePoolWithTag(NonPagedPool, sizeof(VEAFAT_FCB), VEAFAT_POOL_TAG);
    if (!NewFcb)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NewFcb, sizeof(VEAFAT_FCB));
    NewFcb->Vcb = Vcb;
    NewFcb->FirstCluster = TargetCluster;
    NewFcb->FileSize = FoundEntry.FileSize;
    NewFcb->Attributes = FoundEntry.Attributes;
    NewFcb->ParentDirCluster = ParentCluster;
    NewFcb->DirEntryByteOffset = EntryOffset;
    NewFcb->RefCount = 1;
    KsInitializeSpinlock(&NewFcb->FcbLock);
    
    Status = RtlDuplicateAnsiString(RTL_DUPLICATE_ANSI_STRING_NULL_TERMINATE, FileName, &NewFcb->FileName);
    if (!VEA_SUCCESS(Status))
    {
        /* Fix Memory Leak: Free NewFcb jika alokasi string gagal */
        UlFreePoolWithTag(NewFcb, VEAFAT_POOL_TAG);
        return Status;
    }

    /* Masukkan ke FcbListHead Cache */
    KsAcquireSpinLock(&Vcb->FcbListLock, &OldIrql);
    InsertTailList(&Vcb->FcbListHead, &NewFcb->FcbListEntry);
    KsReleaseSpinLock(&Vcb->FcbListLock, OldIrql);

    *OutFcb = NewFcb;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
FatpFreeClusterChain(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN ULONG StartCluster
)
{
    ULONG Current = StartCluster;

    while (Current >= 2 && !FAT32_IS_EOF(Current))
    {
        ULONG Next = 0;
        VEASTATUS Status = FatpGetNextCluster(Vcb, Current, &Next);
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }

        Status = FatpSetNextCluster(Vcb, Current, 0x00000000);
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }

        if (Vcb->FreeClusterCount != 0xFFFFFFFF)
        {
            Vcb->FreeClusterCount++;
        }
        Vcb->FsInfoDirty = TRUE;
        Current = Next;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
FatpInitializeDirectoryCluster(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN ULONG NewDirCluster,
    IN ULONG ParentDirCluster
)
{
    PUCHAR SectorBuffer = UlAllocatePoolZero(NonPagedPool, Vcb->BytesPerSector, VEAFAT_POOL_TAG);
    if (!SectorBuffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    PFAT_DIR_ENTRY Entries = (PFAT_DIR_ENTRY)SectorBuffer;
    ULONG DotDotCluster = (ParentDirCluster == Vcb->RootDirStartCluster) ? 0 : ParentDirCluster;

    /* Entri '.' */
    RtlFillMemory(Entries[0].FileName, 11, ' ');
    Entries[0].FileName[0]      = '.';
    Entries[0].Attributes       = FAT_ATTR_DIRECTORY;
    Entries[0].FirstClusterLow  = (USHORT)(NewDirCluster & 0xFFFF);
    Entries[0].FirstClusterHigh = (USHORT)((NewDirCluster >> 16) & 0xFFFF);

    /* Entri '..' */
    RtlFillMemory(Entries[1].FileName, 11, ' ');
    Entries[1].FileName[0]      = '.';
    Entries[1].FileName[1]      = '.';
    Entries[1].Attributes       = FAT_ATTR_DIRECTORY;
    Entries[1].FirstClusterLow  = (USHORT)(DotDotCluster & 0xFFFF);
    Entries[1].FirstClusterHigh = (USHORT)((DotDotCluster >> 16) & 0xFFFF);

    ULONGLONG BaseLba = FAT32_CLUSTER_TO_LBA(Vcb, NewDirCluster);
    VEASTATUS Status = FatWriteDiskSectors(Vcb->TargetDeviceObject, BaseLba, 1, Vcb->BytesPerSector, SectorBuffer);

    if (VEA_SUCCESS(Status) && Vcb->SectorsPerCluster > 1)
    {
        RtlZeroMemory(SectorBuffer, Vcb->BytesPerSector);
        for (ULONG s = 1; s < Vcb->SectorsPerCluster; s++)
        {
            Status = FatWriteDiskSectors(Vcb->TargetDeviceObject, BaseLba + s, 1, Vcb->BytesPerSector, SectorBuffer);
            if (!VEA_SUCCESS(Status)) break;
        }
    }

    UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
    return Status;
}

VEASTATUS
VEAPI
FatMjCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PFAT32_DEVICE_EXTENSION Vcb = EXTENSION_FROM_DEVOBJ(DeviceObject);
    ULONG RawOptions            = OipStack->Parameters.Create.Options;
    ULONG CreateDisposition     = (RawOptions >> 24) & 0xFF;
    ULONG CreateOptions         = RawOptions & 0x00FFFFFF;
    ULONG FileAttributes        = OipStack->Parameters.Create.FileAttributes;
    PFILE_OBJECT TargetFile     = OipStack->FileObject ? OipStack->FileObject : Oip->OriginalFileObject;
    PANSI_STRING FileName;
    PVEAFAT_FCB Fcb             = NULL;
    PVEAFAT_CCB Ccb             = NULL;
    BOOLEAN IsRootPath;
    ULONG Information           = FILE_OPENED;
    VEASTATUS Status;

    if (Vcb->IsCdo || !TargetFile)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_DEVICE_REQUEST;
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if ((CreateOptions & FILE_DIRECTORY_FILE) && (CreateOptions & FILE_NON_DIRECTORY_FILE))
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        return STATUS_INVALID_PARAMETER;
    }

    FileName = &TargetFile->FileName;
    IsRootPath = (FileName->Length == 0 ||
                 (FileName->Length == 1 && (FileName->Buffer[0] == '/' || FileName->Buffer[0] == '\\')));

    KsWaitForSingleObject(&Vcb->VolumeMutex);

    if (IsRootPath)
    {
        if (CreateDisposition == FILE_CREATE)
        {
            KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
            Oip->StatusInformation.Status = STATUS_OBJECT_NAME_COLLISION;
            return STATUS_OBJECT_NAME_COLLISION;
        }

        if (CreateOptions & FILE_NON_DIRECTORY_FILE)
            Fcb = FatGetOrCreateVolumeFcb(Vcb);
        else
            Fcb = FatGetOrCreateRootFcb(Vcb);

        if (!Fcb)
        {
            KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
            Oip->StatusInformation.Status = STATUS_INSUFFICIENT_RESOURCES;
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }
    else
    {
        ULONG ParentCluster = Vcb->RootDirStartCluster;
        ANSI_STRING LeafName;

        Status = FatLocateFcbEx(Vcb, FileName, &Fcb, &ParentCluster, &LeafName);
        if (!VEA_SUCCESS(Status))
        {
            BOOLEAN CanCreate = (CreateDisposition == FILE_CREATE ||
                                 CreateDisposition == FILE_OPEN_IF ||
                                 CreateDisposition == FILE_OVERWRITE_IF ||
                                 CreateDisposition == FILE_SUPERSEDE);

            if (Status != STATUS_OBJECT_NAME_NOT_FOUND || !CanCreate)
            {
                KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
                Oip->StatusInformation.Status = Status;
                return Status;
            }

            ULONG FirstCluster = 0;
            ULONG FileSize = 0;
            ULONG EntryParentCluster = ParentCluster;
            ULONG EntryByteOffset = 0;
            UCHAR FatAttr = (UCHAR)(FileAttributes & 0x37);

            if (CreateOptions & FILE_DIRECTORY_FILE)
            {
                FatAttr |= FAT_ATTR_DIRECTORY;

                /* Direktori baru di FAT32 wajib punya 1 cluster untuk entri '.' dan '..' */
                Status = FatpAllocateCluster(Vcb, &FirstCluster);
                if (!VEA_SUCCESS(Status))
                {
                    KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
                    Oip->StatusInformation.Status = Status;
                    return Status;
                }

                Status = FatpInitializeDirectoryCluster(Vcb, FirstCluster, ParentCluster);
                if (!VEA_SUCCESS(Status))
                {
                    FatpFreeClusterChain(Vcb, FirstCluster);
                    KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
                    Oip->StatusInformation.Status = Status;
                    return Status;
                }
            }
            else
            {
                FatAttr |= FAT_ATTR_ARCHIVE;
            }

            Status = FatpAddDirectoryEntry(
                Vcb,
                ParentCluster,
                &LeafName,
                FatAttr,
                FirstCluster,
                FileSize,
                &EntryParentCluster,
                &EntryByteOffset
            );
            if (!VEA_SUCCESS(Status))
            {
                if (FirstCluster >= 2)
                {
                    FatpFreeClusterChain(Vcb, FirstCluster);
                }
                DPRINT("[VEAFAT] Failed to write Directory Entry for %Z: 0x%X\n", &LeafName, Status);
                KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
                Oip->StatusInformation.Status = Status;
                return Status;
            }

            Fcb = UlAllocatePoolZero(NonPagedPool, sizeof(VEAFAT_FCB), VEAFAT_POOL_TAG);
            if (!Fcb)
            {
                KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
                Oip->StatusInformation.Status = STATUS_INSUFFICIENT_RESOURCES;
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            Fcb->Vcb = Vcb;
            Fcb->FirstCluster = FirstCluster;
            Fcb->FileSize = FileSize;
            Fcb->Attributes = FatAttr;
            Fcb->ParentDirCluster = EntryParentCluster;
            Fcb->DirEntryByteOffset = EntryByteOffset;
            Fcb->RefCount = 1;
            KsInitializeSpinlock(&Fcb->FcbLock);

            Status = RtlDuplicateAnsiString(
                RTL_DUPLICATE_ANSI_STRING_NULL_TERMINATE,
                FileName,
                &Fcb->FileName
            );
            if (!VEA_SUCCESS(Status))
            {
                UlFreePoolWithTag(Fcb, VEAFAT_POOL_TAG);
                KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
                Oip->StatusInformation.Status = Status;
                return Status;
            }

            KIRQL OldIrql;
            KsAcquireSpinLock(&Vcb->FcbListLock, &OldIrql);
            InsertTailList(&Vcb->FcbListHead, &Fcb->FcbListEntry);
            KsReleaseSpinLock(&Vcb->FcbListLock, OldIrql);

            Information = FILE_CREATED;
            DPRINT("[VEAFAT] Created %Z: FirstCluster=%u, Parent=%u, Offset=%u\n",
                   &LeafName, FirstCluster, EntryParentCluster, EntryByteOffset);
        }
        else
        {
            BOOLEAN IsDirectory = (Fcb->Attributes & FAT_ATTR_DIRECTORY) != 0;

            if (CreateDisposition == FILE_CREATE)
            {
                FatDereferenceFcb(Fcb);
                KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
                Oip->StatusInformation.Status = STATUS_OBJECT_NAME_COLLISION;
                return STATUS_OBJECT_NAME_COLLISION;
            }

            if (IsDirectory && (CreateOptions & FILE_NON_DIRECTORY_FILE))
            {
                FatDereferenceFcb(Fcb);
                KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
                Oip->StatusInformation.Status = STATUS_FILE_IS_A_DIRECTORY;
                return STATUS_FILE_IS_A_DIRECTORY;
            }

            if (!IsDirectory && (CreateOptions & FILE_DIRECTORY_FILE))
            {
                FatDereferenceFcb(Fcb);
                KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
                Oip->StatusInformation.Status = STATUS_NOT_A_DIRECTORY;
                return STATUS_NOT_A_DIRECTORY;
            }

            if (CreateDisposition == FILE_OVERWRITE ||
                CreateDisposition == FILE_OVERWRITE_IF ||
                CreateDisposition == FILE_SUPERSEDE)
            {
                if (IsDirectory || (Fcb->Attributes & FAT_ATTR_READ_ONLY))
                {
                    FatDereferenceFcb(Fcb);
                    KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
                    Oip->StatusInformation.Status = STATUS_ACCESS_DENIED;
                    return STATUS_ACCESS_DENIED;
                }

                /* Bebaskan seluruh rantai cluster lama dan reset ukuran file ke 0 */
                if (Fcb->FirstCluster >= 2 && !FAT32_IS_EOF(Fcb->FirstCluster))
                {
                    FatpFreeClusterChain(Vcb, Fcb->FirstCluster);
                }

                Fcb->FirstCluster = 0;
                Fcb->FileSize = 0;

                if (CreateDisposition == FILE_SUPERSEDE)
                {
                    Fcb->Attributes = (UCHAR)((FileAttributes & 0x37) | FAT_ATTR_ARCHIVE);
                    Information = FILE_SUPERSEDED;
                }
                else
                {
                    Fcb->Attributes |= (UCHAR)((FileAttributes & 0x37) | FAT_ATTR_ARCHIVE);
                    Information = FILE_OVERWRITTEN;
                }

                FatpUpdateDirectoryEntry(Vcb, Fcb);
            }
            else
            {
                Information = FILE_OPENED;
            }
        }
    }

    Ccb = UlAllocatePoolZero(NonPagedPool, sizeof(VEAFAT_CCB), VEAFAT_POOL_TAG);
    if (!Ccb)
    {
        FatDereferenceFcb(Fcb);
        KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
        Oip->StatusInformation.Status = STATUS_INSUFFICIENT_RESOURCES;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Ccb->Fcb = Fcb;
    Ccb->CurrentByteOffset = 0;

    TargetFile->DeviceObject = Vcb->VolumeDeviceObject;
    TargetFile->FsContext  = Fcb;
    TargetFile->FsContext2 = Ccb;
    TargetFile->Vpb        = Vcb->Vpb;
    TargetFile->SectionObjectPointer = &Fcb->SectionObjectPointers;

    KsReleaseMutex(&Vcb->VolumeMutex, FALSE);

    Oip->StatusInformation.Status = STATUS_SUCCESS;
    Oip->StatusInformation.Information = Information;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
FatMjCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PFAT32_DEVICE_EXTENSION Vcb = EXTENSION_FROM_DEVOBJ(DeviceObject);
    PFILE_OBJECT FileObject = OipStack->FileObject ? OipStack->FileObject : Oip->OriginalFileObject;

    if (!Vcb->IsCdo && FileObject && FileObject->FsContext)
    {
        PVEAFAT_FCB Fcb = (PVEAFAT_FCB)FileObject->FsContext;
        if (Fcb->DeletePending && Fcb->RefCount <= 1)
        {
            FatpDeleteDirectoryEntry(Vcb, Fcb);
        }
        FatpFlushFsInfo(Vcb);
    }
    
    Oip->StatusInformation.Status = STATUS_SUCCESS;
    Oip->StatusInformation.Information = 0;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
FatMjClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PFAT32_DEVICE_EXTENSION Vcb = EXTENSION_FROM_DEVOBJ(DeviceObject);
    PFILE_OBJECT FileObject = OipStack->FileObject ? OipStack->FileObject : Oip->OriginalFileObject;

    if (!Vcb->IsCdo && FileObject)
    {
        PVEAFAT_CCB Ccb = (PVEAFAT_CCB)FileObject->FsContext2;
        PVEAFAT_FCB Fcb = (PVEAFAT_FCB)FileObject->FsContext;

        if (Ccb)
        {
            if (Ccb->DirectorySearchFilter.Buffer)
            {
                UlFreePoolWithTag(Ccb->DirectorySearchFilter.Buffer, VEAFAT_POOL_TAG);
            }
            UlFreePoolWithTag(Ccb, VEAFAT_POOL_TAG);
            FileObject->FsContext2 = NULL;
        }

        if (Fcb)
        {
            FatDereferenceFcb(Fcb);
            FileObject->FsContext = NULL;
        }
    }

    Oip->StatusInformation.Status = STATUS_SUCCESS;
    Oip->StatusInformation.Information = 0;
    return STATUS_SUCCESS;
}

/* EOF */