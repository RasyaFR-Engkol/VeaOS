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

    RtlInitAnsiString(&Fcb->FileName, "\\");
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

    RtlInitAnsiString(&Fcb->FileName, "\\");
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
        if (RtlEqualString(&Fcb->FileName, FileName, TRUE))
        {
            FatReferenceFcb(Fcb);
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
FatMjCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PFAT32_DEVICE_EXTENSION Vcb = EXTENSION_FROM_DEVOBJ(DeviceObject);
    ULONG RawOptions        = OipStack->Parameters.Create.Options;
    ULONG CreateDisposition = (RawOptions >> 24) & 0xFF;
    ULONG CreateOptions     = RawOptions & 0x00FFFFFF;
    ULONG FileAttributes    = OipStack->Parameters.Create.FileAttributes;
    ULONG ShareAccess      = OipStack->Parameters.Create.ShareAccess;
    PFILE_OBJECT OriginalFile = Oip->OriginalFileObject;
    PANSI_STRING FileName   = &OriginalFile->FileName;
    PVEAFAT_FCB Fcb         = NULL;
    PVEAFAT_CCB Ccb         = NULL;
    BOOLEAN IsRootPath;
    ULONG Information       = FILE_OPENED;
    VEASTATUS Status;

    if (Vcb->IsCdo)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_DEVICE_REQUEST;
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    IsRootPath = (FileName->Length == 0 || 
                 (FileName->Length == 1 && (FileName->Buffer[0] == '/' || FileName->Buffer[0] == '\\')));

    if (IsRootPath)
    {
        if (CreateOptions & FILE_NON_DIRECTORY_FILE)
            Fcb = FatGetOrCreateVolumeFcb(Vcb);
        else
            Fcb = FatGetOrCreateRootFcb(Vcb);

        if (!Fcb)
        {
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
            if (Status == STATUS_OBJECT_NAME_NOT_FOUND &&
               (CreateDisposition == FILE_CREATE || 
                CreateDisposition == FILE_OPEN_IF || 
                CreateDisposition == FILE_OVERWRITE_IF || 
                CreateDisposition == FILE_SUPERSEDE))
            {
                ULONG FirstCluster = 0; 
                ULONG FileSize = 0;

                Status = FatpAddDirectoryEntry(
                    Vcb,
                    ParentCluster,  // Cluster folder parent (misal 13109)
                    &LeafName,      // Nama asli tanpa path
                    (UCHAR)(FileAttributes & 0x37),
                    FirstCluster,   // Cluster 0
                    FileSize        // 0 Byte
                );

                if (!VEA_SUCCESS(Status))
                {
                    DPRINT("[VEAFAT] Failed to write Directory Entry for %Z: 0x%X\n", &LeafName, Status);
                    Oip->StatusInformation.Status = Status;
                    return Status;
                }

                Fcb = UlAllocatePoolWithTag(NonPagedPool, sizeof(VEAFAT_FCB), VEAFAT_POOL_TAG);
                if (!Fcb)
                {
                    Oip->StatusInformation.Status = STATUS_INSUFFICIENT_RESOURCES;
                    return STATUS_INSUFFICIENT_RESOURCES;
                }

                RtlZeroMemory(Fcb, sizeof(VEAFAT_FCB));
                Fcb->Vcb = Vcb;
                Fcb->FirstCluster = FirstCluster;
                Fcb->FileSize = FileSize;
                Fcb->Attributes = (UCHAR)(FileAttributes & 0x37);
                Fcb->RefCount = 1;

                Status = RtlDuplicateAnsiString(
                    RTL_DUPLICATE_ANSI_STRING_NULL_TERMINATE, 
                    FileName, 
                    &Fcb->FileName
                );
                if (!VEA_SUCCESS(Status))
                {
                    UlFreePoolWithTag(Fcb, VEAFAT_POOL_TAG);
                    Oip->StatusInformation.Status = Status;
                    return Status;
                }

                KIRQL OldIrql;
                KsAcquireSpinLock(&Vcb->FcbListLock, &OldIrql);
                InsertTailList(&Vcb->FcbListHead, &Fcb->FcbListEntry);
                KsReleaseSpinLock(&Vcb->FcbListLock, OldIrql);

                Information = FILE_CREATED;
                DPRINT("[VEAFAT] File %Z created! FirstCluster=%u, Size=%u in DirCluster=%u\n", 
                       &LeafName, FirstCluster, FileSize, ParentCluster);
            }
            else
            {
                Oip->StatusInformation.Status = Status;
                return Status;
            }
        }
        else
        {
            if (CreateDisposition == FILE_CREATE)
            {
                FatDereferenceFcb(Fcb);
                Oip->StatusInformation.Status = STATUS_OBJECT_NAME_COLLISION;
                return STATUS_OBJECT_NAME_COLLISION;
            }

            Information = (CreateDisposition == FILE_SUPERSEDE) ? FILE_SUPERSEDED : FILE_OPENED;
        }

        BOOLEAN IsDirectory = (Fcb->Attributes & FAT_ATTR_DIRECTORY) != 0;

        if (IsDirectory && (CreateOptions & FILE_NON_DIRECTORY_FILE))
        {
            FatDereferenceFcb(Fcb);
            Oip->StatusInformation.Status = STATUS_FILE_IS_A_DIRECTORY;
            return STATUS_FILE_IS_A_DIRECTORY;
        }
    }

    Ccb = UlAllocatePoolZero(NonPagedPool, sizeof(VEAFAT_CCB), VEAFAT_POOL_TAG);
    if (!Ccb)
    {
        FatDereferenceFcb(Fcb);
        Oip->StatusInformation.Status = STATUS_INSUFFICIENT_RESOURCES;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Ccb->Fcb = Fcb;
    Ccb->CurrentByteOffset = 0;

    OriginalFile->DeviceObject = Vcb->VolumeDeviceObject;
    OriginalFile->FsContext  = Fcb;
    OriginalFile->FsContext2 = Ccb;
    OriginalFile->Vpb        = Vcb->Vpb;
    OriginalFile->SectionObjectPointer = &Fcb->SectionObjectPointers;

    Oip->StatusInformation.Status = STATUS_SUCCESS;
    Oip->StatusInformation.Information = Information; // FIX: Pake variable Information, bukan hardcode FILE_OPEN
    return STATUS_SUCCESS;
}