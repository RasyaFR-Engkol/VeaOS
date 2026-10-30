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
FatLocateFcb(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN PANSI_STRING FileName,
    OUT PVEAFAT_FCB *OutFcb
)
{
    // 1. Cek Lookup Cache di FcbListHead
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

    /* On disk searching */

    VEASTATUS Status;
    ULONG CurrentDirCluster = Vcb->RootDirStartCluster;
    FAT_DIR_ENTRY FoundEntry;
    ULONG ParentCluster = 0;
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

        /* Cari delimiter berikutnya ('/' atau '\') */
        while (Pos < FileName->Length && FileName->Buffer[Pos] != '/' && FileName->Buffer[Pos] != '\\')
        {
            Pos++;
        }

        Component.Buffer = &FileName->Buffer[StartPos];
        Component.Length = Pos - StartPos;
        Component.MaximumLength = Component.Length;

        /* Cari komponen ini di dalam cluster direktori saat ini */
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
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        /* Skip trailing slashes */
        while (Pos < FileName->Length && (FileName->Buffer[Pos] == '/' || FileName->Buffer[Pos] == '\\'))
        {
            Pos++;
        }

        /* Jika belum di akhir path, pastikan entri ini adalah Sub-Directory */
        if (Pos < FileName->Length)
        {
            if (!BooleanFlagOn(FoundEntry.Attributes, FAT_ATTR_DIRECTORY))
            {
                return STATUS_OBJECT_PATH_NOT_FOUND;
            }

            /* Pindah konteks pencarian ke cluster folder baru ini */
            CurrentDirCluster = MERGE_CLUSTER_HI_N_LO(FoundEntry.FirstClusterHigh, FoundEntry.FirstClusterLow);
        }
    }

    PVEAFAT_FCB NewFcb = UlAllocatePoolWithTag(NonPagedPool, sizeof(VEAFAT_FCB), VEAFAT_POOL_TAG);
    if (!NewFcb)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    RtlZeroMemory(NewFcb, sizeof(VEAFAT_FCB));
    NewFcb->Vcb = Vcb;
    NewFcb->FirstCluster = MERGE_CLUSTER_HI_N_LO(FoundEntry.FirstClusterHigh, FoundEntry.FirstClusterLow);
    NewFcb->FileSize = FoundEntry.FileSize;
    NewFcb->Attributes = FoundEntry.Attributes;
    NewFcb->ParentDirCluster = ParentCluster;
    NewFcb->DirEntryByteOffset = EntryOffset;
    NewFcb->RefCount = 1;
    
    /* Duplikasi string nama */
    Status = RtlDuplicateAnsiString(RTL_DUPLICATE_ANSI_STRING_NULL_TERMINATE, FileName, &NewFcb->FileName);
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* 4. Masukkan ke FcbListHead Cache */
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
    ULONG CreateOptions = OipStack->Parameters.Create.Options;
    ULONG FileAttributes = OipStack->Parameters.Create.FileAttributes;
    ULONG ShareAccess = OipStack->Parameters.Create.ShareAccess;
    PFILE_OBJECT OriginalFile = Oip->OriginalFileObject;
    PANSI_STRING FileName = &OriginalFile->FileName;
    PVEAFAT_FCB Fcb = NULL;
    PVEAFAT_CCB Ccb = NULL;
    BOOLEAN IsRootPath;
    VEASTATUS Status;

    if (Vcb->IsCdo)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_DEVICE_REQUEST;
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    /* Access root path either Length is 0 or has '/*/
    IsRootPath = (FileName->Length == 0 || 
                 (FileName->Length == 1 && FileName->Buffer[0] == '/'));

    if (IsRootPath)
    {
        /* if open raw volume DASD */
        if (CreateOptions & FILE_NON_DIRECTORY_FILE)
        {
            Fcb = FatGetOrCreateVolumeFcb(Vcb);
        }
        else
        {
            Fcb = FatGetOrCreateRootFcb(Vcb);
        }

        if (!Fcb)
        {
            Oip->StatusInformation.Status = STATUS_INSUFFICIENT_MEMORY;
            return STATUS_INSUFFICIENT_MEMORY;
        }
    }
    else
    {
        Status = FatLocateFcb(Vcb, FileName, &Fcb);
        if (!VEA_SUCCESS(Status))
        {
            Oip->StatusInformation.Status = Status;
            return Status;
        }

        BOOLEAN IsDirectory = (Fcb->Attributes & FAT_ATTR_DIRECTORY) != 0;

        if (IsDirectory && (CreateOptions & FILE_NON_DIRECTORY_FILE))
        {
            FatDereferenceFcb(Fcb);
            Oip->StatusInformation.Status = STATUS_FILE_IS_A_DIRECTORY;
            return STATUS_FILE_IS_A_DIRECTORY;
        }

        if (!IsDirectory && (CreateOptions & FILE_DIRECTORY_FILE))
        {
            FatDereferenceFcb(Fcb);
            Oip->StatusInformation.Status = STATUS_NOT_A_DIRECTORY;
            return STATUS_NOT_A_DIRECTORY;
        }
    }

    Ccb = UlAllocatePoolZero(NonPagedPool, sizeof(VEAFAT_CCB), VEAFAT_POOL_TAG);
    if (!Ccb)
    {
        FatDereferenceFcb(Fcb);
        Oip->StatusInformation.Status = STATUS_INSUFFICIENT_MEMORY;
        return STATUS_INSUFFICIENT_MEMORY;
    }

    Ccb->Fcb = Fcb;
    Ccb->CurrentByteOffset = 0;

    OriginalFile->FsContext  = Fcb;
    OriginalFile->FsContext2 = Ccb;
    OriginalFile->Vpb        = Vcb->Vpb;

    Oip->StatusInformation.Status = STATUS_SUCCESS;
    Oip->StatusInformation.Information = FILE_OPEN;
    return STATUS_SUCCESS;
}