/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : readwrite.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : FAT Mj read Write
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "veafat.h"

/* Revision History ------------------------------------------------------
 * DATE       : 17-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file readwrite.c
 * --------------------------------------------------------------------- */

/* PRIVATE */
static
VOID
VEAPI
FatpInvalidateFatSectorCache(
    IN PFAT32_DEVICE_EXTENSION Vcb
)
{
    KIRQL OldIrql;

    KsAcquireSpinLock(&Vcb->FatSectorCacheLock, &OldIrql);
    Vcb->FatSectorCacheGeneration++;
    Vcb->FatSectorCacheValid = FALSE;
    KsReleaseSpinLock(&Vcb->FatSectorCacheLock, OldIrql);
}

static
VOID
VEAPI
FatpUpdateFatSectorCache(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN ULONGLONG FatSectorLba,
    IN PUCHAR FatBuffer
)
{
    KIRQL OldIrql;

    KsAcquireSpinLock(&Vcb->FatSectorCacheLock, &OldIrql);
    Vcb->FatSectorCacheGeneration++;
    RtlCopyMemory(Vcb->FatSectorCache, FatBuffer, Vcb->BytesPerSector);
    Vcb->FatSectorCacheLba = FatSectorLba;
    Vcb->FatSectorCacheValid = TRUE;
    KsReleaseSpinLock(&Vcb->FatSectorCacheLock, OldIrql);
}

VEASTATUS
VEAPI
FatpGetNextCluster(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN ULONG CurrentCluster,
    OUT PULONG NextCluster
)
{
    VEASTATUS Status;
    ULONGLONG FatSectorLba;
    ULONG FatOffsetInSector;
    PUCHAR FatBuffer;
    ULONG FatEntry;
    ULONG CacheGeneration;
    KIRQL OldIrql;

    if (!Vcb || !NextCluster || Vcb->BytesPerSector == 0 ||
        Vcb->BytesPerSector > VEAFAT_MAX_SECTOR_SIZE)
    {
        DPRINT("VEAFAT: ERROR: No VCB or No Next Cluster or invalid sector size\n");
        return STATUS_INVALID_PARAMETER;
    }

    FatSectorLba = Vcb->FatStartLba + ((ULONGLONG)CurrentCluster * 4 / Vcb->BytesPerSector);
    FatOffsetInSector = (CurrentCluster * 4) % Vcb->BytesPerSector;

    KsAcquireSpinLock(&Vcb->FatSectorCacheLock, &OldIrql);
    if (Vcb->FatSectorCacheValid &&
        Vcb->FatSectorCacheLba == FatSectorLba)
    {
        RtlCopyMemory(
            &FatEntry,
            Vcb->FatSectorCache + FatOffsetInSector,
            sizeof(FatEntry)
        );
        KsReleaseSpinLock(&Vcb->FatSectorCacheLock, OldIrql);
        *NextCluster = FatEntry & 0x0FFFFFFF;
        return STATUS_SUCCESS;
    }

    CacheGeneration = Vcb->FatSectorCacheGeneration;
    KsReleaseSpinLock(&Vcb->FatSectorCacheLock, OldIrql);

    FatBuffer = (PUCHAR)UlAllocatePoolWithTag(
        NonPagedPool,
        Vcb->BytesPerSector,
        'FAT '
    );
    if (!FatBuffer)
        return STATUS_INSUFFICIENT_RESOURCES;

    Status = FatReadDiskSectors(
        Vcb->TargetDeviceObject,
        FatSectorLba,
        1,
        Vcb->BytesPerSector,
        FatBuffer
    );
    if (!VEA_SUCCESS(Status))
    {
        UlFreePoolWithTag(FatBuffer, 'FAT ');
        return Status;
    }

    RtlCopyMemory(&FatEntry, FatBuffer + FatOffsetInSector, sizeof(FatEntry));

    KsAcquireSpinLock(&Vcb->FatSectorCacheLock, &OldIrql);
    if (Vcb->FatSectorCacheValid &&
        Vcb->FatSectorCacheLba == FatSectorLba)
    {
        RtlCopyMemory(
            &FatEntry,
            Vcb->FatSectorCache + FatOffsetInSector,
            sizeof(FatEntry)
        );
    }
    else if (Vcb->FatSectorCacheGeneration == CacheGeneration)
    {
        RtlCopyMemory(
            Vcb->FatSectorCache,
            FatBuffer,
            Vcb->BytesPerSector
        );
        Vcb->FatSectorCacheLba = FatSectorLba;
        Vcb->FatSectorCacheValid = TRUE;
    }
    KsReleaseSpinLock(&Vcb->FatSectorCacheLock, OldIrql);

    UlFreePoolWithTag(FatBuffer, 'FAT ');

    *NextCluster = FatEntry & 0x0FFFFFFF;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
FatpSetNextCluster(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN ULONG CurrentCluster,
    IN ULONG NextCluster
)
{
    VEASTATUS Status;
    ULONGLONG FatSectorLba;
    ULONG FatOffsetInSector;
    PUCHAR FatBuffer;
    PULONG FatTable;

    FatBuffer = (PUCHAR)UlAllocatePoolZero(NonPagedPool, Vcb->BytesPerSector, 'FAT ');
    if (!FatBuffer) return STATUS_INSUFFICIENT_RESOURCES;

    FatSectorLba = Vcb->FatStartLba + ((ULONGLONG)CurrentCluster * 4 / Vcb->BytesPerSector);
    FatOffsetInSector = (CurrentCluster * 4) % Vcb->BytesPerSector;

    Status = FatReadDiskSectors(Vcb->TargetDeviceObject, FatSectorLba, 1, Vcb->BytesPerSector, FatBuffer);
    if (!VEA_SUCCESS(Status))
    {
        UlFreePoolWithTag(FatBuffer, 'FAT ');
        return Status;
    }

    FatTable = (PULONG)FatBuffer;
    ULONG OldVal = FatTable[FatOffsetInSector / 4];
    FatTable[FatOffsetInSector / 4] = (OldVal & 0xF0000000) | (NextCluster & 0x0FFFFFFF);

    for (ULONG f = 0; f < Vcb->NumberOfFats; f++)
    {
        ULONGLONG TargetFatLba = FatSectorLba + (f * Vcb->FatSizeSectors);
        Status = FatWriteDiskSectors(Vcb->TargetDeviceObject, TargetFatLba, 1, Vcb->BytesPerSector, FatBuffer);
        if (!VEA_SUCCESS(Status)) break;
    }

    if (VEA_SUCCESS(Status))
        FatpUpdateFatSectorCache(Vcb, FatSectorLba, FatBuffer);
    else
        FatpInvalidateFatSectorCache(Vcb);

    UlFreePoolWithTag(FatBuffer, 'FAT ');
    return Status;
}

VEASTATUS
VEAPI
FatpAllocateCluster(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    OUT PULONG OutCluster
)
{
    VEASTATUS Status;
    ULONG MaxCluster = Vcb->TotalClusters + 1;
    ULONG StartSearch = (Vcb->NextFreeCluster >= 2 && Vcb->NextFreeCluster <= MaxCluster)
                        ? Vcb->NextFreeCluster : 2;

    if (MaxCluster < 2)
    {
        return STATUS_DISK_FULL;
    }

    /* Pass 1: dari NextFreeCluster sampai ujung volume; Pass 2: wrap-around dari cluster 2 */
    for (ULONG Pass = 0; Pass < 2; Pass++)
    {
        ULONG Begin = (Pass == 0) ? StartSearch : 2;
        ULONG End   = (Pass == 0) ? MaxCluster  : (StartSearch - 1);

        for (ULONG Cluster = Begin; Cluster <= End; Cluster++)
        {
            ULONG Value = 0;
            Status = FatpGetNextCluster(Vcb, Cluster, &Value);
            if (VEA_SUCCESS(Status) && Value == 0x00000000)
            {
                Status = FatpSetNextCluster(Vcb, Cluster, 0x0FFFFFFF);
                if (VEA_SUCCESS(Status))
                {
                    *OutCluster = Cluster;
                    Vcb->NextFreeCluster = (Cluster + 1 <= MaxCluster) ? (Cluster + 1) : 2;

                    if (Vcb->FreeClusterCount != 0xFFFFFFFF && Vcb->FreeClusterCount > 0)
                    {
                        Vcb->FreeClusterCount--;
                    }
                    Vcb->FsInfoDirty = TRUE;
                    return STATUS_SUCCESS;
                }
            }
        }
    }

    return STATUS_DISK_FULL;
}

VEASTATUS
VEAPI
FatpReadFile(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN POIP Oip,
    IN POIP_STACK OipStack,
    IN PVEAFAT_FCB Fcb,
    IN PFILE_OBJECT FileObject,
    IN LARGE_INTEGER ByteOffset,
    IN ULONG Length
)
{
    VEASTATUS Status = STATUS_SUCCESS;
    ULONG CurrentCluster;
    ULONG BytesRead = 0;
    ULONG BytesToRead = Length;
    PUCHAR UserBuffer = NULL;

    UNREFERENCED_PARAMETER(OipStack);

    if (Oip->MdlAddress != NULL && Oip->MdlAddress->MappedSystemVa != NULL)
    {
        UserBuffer = (PUCHAR)Oip->MdlAddress->MappedSystemVa;
    }
    else if ((Oip->Flags & OIP_BUFFERED_IO) && Oip->AssociatedOip.SystemBuffer != NULL)
    {
        UserBuffer = (PUCHAR)Oip->AssociatedOip.SystemBuffer;
    }
    else
    {
        UserBuffer = (PUCHAR)Oip->UserBuffer;
    }

    if (!UserBuffer)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        Oip->StatusInformation.Information = 0;
        ObioCompleteRequest(Oip);
        return STATUS_INVALID_PARAMETER;
    }

    if (!Fcb || Fcb->FirstCluster < 2 || FAT32_IS_EOF(Fcb->FirstCluster))
    {
        Oip->StatusInformation.Status = STATUS_END_OF_FILE;
        Oip->StatusInformation.Information = 0;
        ObioCompleteRequest(Oip);
        return STATUS_END_OF_FILE;
    }

    if (ByteOffset.QuadPart >= Fcb->FileSize)
    {
        Oip->StatusInformation.Status = STATUS_END_OF_FILE;
        Oip->StatusInformation.Information = 0;
        ObioCompleteRequest(Oip);
        return STATUS_END_OF_FILE;
    }

    if ((ByteOffset.QuadPart + Length) > Fcb->FileSize)
    {
        BytesToRead = (ULONG)(Fcb->FileSize - ByteOffset.QuadPart);
    }

    ULONG TargetClusterIdx = (ULONG)(ByteOffset.QuadPart / Vcb->BytesPerCluster);
    ULONG OffsetInCluster  = (ULONG)(ByteOffset.QuadPart % Vcb->BytesPerCluster);
    PVEAFAT_CCB Ccb        = (FileObject && FileObject->FsContext2) ? (PVEAFAT_CCB)FileObject->FsContext2 : NULL;

    /* 1. Mulai traversal dari cursor CCB jika pembacaan maju/sekuensial */
    ULONG StartIdx = 0;
    CurrentCluster = Fcb->FirstCluster;

    if (Ccb &&
        Ccb->CachedFirstCluster == Fcb->FirstCluster &&
        Ccb->LastClusterNumber >= 2 &&
        !FAT32_IS_EOF(Ccb->LastClusterNumber) &&
        TargetClusterIdx >= Ccb->LastClusterIndex)
    {
        StartIdx       = Ccb->LastClusterIndex;
        CurrentCluster = Ccb->LastClusterNumber;
    }

    ULONG CurrentClusterIdx = StartIdx;
    while (CurrentClusterIdx < TargetClusterIdx && !FAT32_IS_EOF(CurrentCluster) && CurrentCluster >= 2)
    {
        Status = FatpGetNextCluster(Vcb, CurrentCluster, &CurrentCluster);
        if (!VEA_SUCCESS(Status))
        {
            Oip->StatusInformation.Status = Status;
            Oip->StatusInformation.Information = 0;
            ObioCompleteRequest(Oip);
            return Status;
        }
        CurrentClusterIdx++;
    }

    if (FAT32_IS_EOF(CurrentCluster) || CurrentCluster < 2)
    {
        Oip->StatusInformation.Status = STATUS_END_OF_FILE;
        Oip->StatusInformation.Information = 0;
        ObioCompleteRequest(Oip);
        return STATUS_END_OF_FILE;
    }

    ULONG SectorSize        = Vcb->BytesPerSector ? Vcb->BytesPerSector : 512;
    ULONG MaxChunk          = (Vcb->BytesPerCluster > VEAFAT_MAX_IO_CHUNK) ? Vcb->BytesPerCluster : VEAFAT_MAX_IO_CHUNK;
    BOOLEAN IsUserSpaceAddr = ((ULONG_PTR)UserBuffer < 0x80000000UL);

    /* 2. Alokasikan BounceBuffer SEKALI saja di luar loop */
    PUCHAR BounceBuffer = NULL;
    if (IsUserSpaceAddr || (OffsetInCluster % SectorSize != 0) || (BytesToRead % SectorSize != 0))
    {
        ULONG BounceAllocSize = ALIGN_UP_BY(MaxChunk + SectorSize, SectorSize);
        BounceBuffer = (PUCHAR)UlAllocatePoolWithTag(NonPagedPool, BounceAllocSize, 'bVaF');
        if (!BounceBuffer)
        {
            Oip->StatusInformation.Status = STATUS_INSUFFICIENT_RESOURCES;
            Oip->StatusInformation.Information = 0;
            ObioCompleteRequest(Oip);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    /* 3. Baca data dengan menggabungkan cluster yang berurutan (contiguous coalescing) */
    while (BytesRead < BytesToRead && !FAT32_IS_EOF(CurrentCluster) && CurrentCluster >= 2)
    {
        ULONG RunStartCluster  = CurrentCluster;
        ULONG RunBytes         = Vcb->BytesPerCluster - OffsetInCluster;
        ULONG RemainingBytes   = BytesToRead - BytesRead;
        ULONG NextCluster      = 0;
        BOOLEAN HasNextCluster = FALSE;

        while (RunBytes + Vcb->BytesPerCluster <= MaxChunk &&
               RunBytes < RemainingBytes)
        {
            Status = FatpGetNextCluster(Vcb, CurrentCluster, &NextCluster);
            if (!VEA_SUCCESS(Status) || FAT32_IS_EOF(NextCluster) || NextCluster < 2)
            {
                HasNextCluster = TRUE;
                break;
            }

            if (NextCluster != CurrentCluster + 1)
            {
                HasNextCluster = TRUE;
                break;
            }

            CurrentCluster = NextCluster;
            CurrentClusterIdx++;
            RunBytes += Vcb->BytesPerCluster;
        }

        ULONG ChunkSize = RemainingBytes;
        if (ChunkSize > RunBytes)
        {
            ChunkSize = RunBytes;
        }

        ULONGLONG RunStartLba = FAT32_CLUSTER_TO_LBA(Vcb, RunStartCluster);
        LARGE_INTEGER VolumeByteOffset;
        VolumeByteOffset.QuadPart = (RunStartLba * SectorSize) + OffsetInCluster;

        ULONG DiskReadLength    = ChunkSize;
        BOOLEAN UseBounceBuffer = FALSE;
        PUCHAR DiskBuffer       = UserBuffer + BytesRead;
        ULONG SectorOffset      = 0;

        if (IsUserSpaceAddr || (DiskReadLength % SectorSize != 0) || (VolumeByteOffset.QuadPart % SectorSize != 0))
        {
            UseBounceBuffer           = TRUE;
            SectorOffset              = (ULONG)(VolumeByteOffset.QuadPart % SectorSize);
            DiskReadLength            = ALIGN_UP_BY(ChunkSize + SectorOffset, SectorSize);
            VolumeByteOffset.QuadPart = ALIGN_DOWN_BY(VolumeByteOffset.QuadPart, SectorSize);
            DiskBuffer                = BounceBuffer;
        }

        POIP OipLocal;
        KEVENT Event;
        OIP_INFORMATION InformationBlock;

        KsInitializeEvent(&Event, NotificationEvent, FALSE);

        OipLocal = ObioBuildSynchronusFsdRequest(
            OIP_MJ_READ,
            Vcb->TargetDeviceObject,
            DiskBuffer,
            DiskReadLength,
            &VolumeByteOffset,
            &Event,
            &InformationBlock
        );

        if (!OipLocal)
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        Status = ObioCallObject(Vcb->TargetDeviceObject, OipLocal);

        if (Status == STATUS_PENDING)
        {
            KsWaitForSingleObject(&Event);
            Status = InformationBlock.Status;
        }

        if (!VEA_SUCCESS(Status))
        {
            break;
        }

        if (UseBounceBuffer)
        {
            RtlCopyMemory(UserBuffer + BytesRead, BounceBuffer + SectorOffset, ChunkSize);
        }

        BytesRead       += ChunkSize;
        OffsetInCluster  = 0;

        if (BytesRead < BytesToRead)
        {
            if (HasNextCluster)
            {
                CurrentCluster = NextCluster;
                if (FAT32_IS_EOF(CurrentCluster) || CurrentCluster < 2)
                {
                    break;
                }
                CurrentClusterIdx++;
            }
            else
            {
                Status = FatpGetNextCluster(Vcb, CurrentCluster, &CurrentCluster);
                if (!VEA_SUCCESS(Status) || FAT32_IS_EOF(CurrentCluster) || CurrentCluster < 2)
                {
                    break;
                }
                CurrentClusterIdx++;
            }
        }
    }

    if (BounceBuffer)
    {
        UlFreePool(BounceBuffer);
    }

    Oip->StatusInformation.Information = BytesRead;
    Oip->StatusInformation.Status = (BytesRead > 0) ? STATUS_SUCCESS : Status;

    /* 4. Update CurrentByteOffset & simpan posisi cluster terakhir ke CCB */
    if (FileObject)
    {
        FileObject->CurrentByteOffset.QuadPart = ByteOffset.QuadPart + BytesRead;

        if (Ccb)
        {
            Ccb->CurrentByteOffset = (ULONG)(ByteOffset.QuadPart + BytesRead);
            if (CurrentCluster >= 2 && !FAT32_IS_EOF(CurrentCluster))
            {
                Ccb->CachedFirstCluster = Fcb->FirstCluster;
                Ccb->LastClusterIndex   = CurrentClusterIdx;
                Ccb->LastClusterNumber  = CurrentCluster;
            }
        }
    }

    ObioCompleteRequest(Oip);
    return (BytesRead > 0) ? STATUS_SUCCESS : Status;
}

/* PUBLIC */

VEASTATUS
VEAPI
FatMjRead(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK Stack
)
{
    PFILE_OBJECT FileObject = Stack->FileObject ? Stack->FileObject : Oip->OriginalFileObject;
    PVEAFAT_FCB Fcb = NULL;
    PFAT32_DEVICE_EXTENSION DevExt = EXTENSION_FROM_DEVOBJ(DeviceObject);
    ULONG Length = Stack->Parameters.ReadWrite.Length;
    LARGE_INTEGER ByteOffset = Stack->Parameters.ReadWrite.Offset;

    if (!FileObject || !FileObject->FsContext)
    {
        DPRINT("VEAFAT: ERROR: No File Object or FsContext\n");
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        Oip->StatusInformation.Information = 0;
        ObioCompleteRequest(Oip);
        return STATUS_INVALID_PARAMETER;
    }

    Fcb = (PVEAFAT_FCB)FileObject->FsContext;

    if (Length == 0)
    {
        Oip->StatusInformation.Information = 0;
        Oip->StatusInformation.Status = STATUS_SUCCESS;
        ObioCompleteRequest(Oip);
        return STATUS_SUCCESS;
    }

    if (ByteOffset.QuadPart >= Fcb->FileSize)
    {
        Oip->StatusInformation.Status = STATUS_END_OF_FILE;
        Oip->StatusInformation.Information = 0;
        ObioCompleteRequest(Oip);
        return STATUS_END_OF_FILE;
    }

    return FatpReadFile(
        DevExt,
        Oip,
        Stack,
        Fcb,
        FileObject,
        ByteOffset,
        Length
    );
}

VEASTATUS
VEAPI
FatMjWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK Stack
)
{
    PFAT32_DEVICE_EXTENSION Vcb = EXTENSION_FROM_DEVOBJ(DeviceObject);
    PFILE_OBJECT FileObject     = Stack->FileObject ? Stack->FileObject : Oip->OriginalFileObject;
    ULONG Length                = Stack->Parameters.ReadWrite.Length;
    LARGE_INTEGER ByteOffset    = Stack->Parameters.ReadWrite.Offset;
    PUCHAR UserBuffer           = (PUCHAR)Oip->UserBuffer;
    PVEAFAT_FCB Fcb;
    VEASTATUS Status            = STATUS_SUCCESS;

    if (!UserBuffer)
    {
        UserBuffer = (PUCHAR)Oip->AssociatedOip.SystemBuffer;
    }

    if (Vcb->IsCdo || !FileObject || !FileObject->FsContext || (!UserBuffer && Length > 0))
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        Oip->StatusInformation.Information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    if (Vcb->VolumeFlags & VCB_STATE_READ_ONLY)
    {
        Oip->StatusInformation.Status = STATUS_MEDIA_WRITE_PROTECTED;
        Oip->StatusInformation.Information = 0;
        return STATUS_MEDIA_WRITE_PROTECTED;
    }

    Fcb = (PVEAFAT_FCB)FileObject->FsContext;

    if (Fcb->IsVolumeFcb || (Fcb->Attributes & (FAT_ATTR_DIRECTORY | FAT_ATTR_READ_ONLY)))
    {
        Oip->StatusInformation.Status = STATUS_ACCESS_DENIED;
        Oip->StatusInformation.Information = 0;
        return STATUS_ACCESS_DENIED;
    }

    if (Length == 0)
    {
        Oip->StatusInformation.Status = STATUS_SUCCESS;
        Oip->StatusInformation.Information = 0;
        return STATUS_SUCCESS;
    }

    KsWaitForSingleObject(&Vcb->VolumeMutex);

    ULONGLONG EndOffset    = (ULONGLONG)ByteOffset.QuadPart + Length;
    ULONG RequiredClusters = FAT32_BYTES_TO_CLUSTERS(Vcb, EndOffset);
    ULONG ExistingClusters = 0;
    ULONG LastCluster      = 0;
    ULONG CurrentCluster   = Fcb->FirstCluster;

    /* 1. Hitung cluster yang sudah dimiliki file & cari ujung rantainya */
    while (CurrentCluster >= 2 && !FAT32_IS_EOF(CurrentCluster))
    {
        ExistingClusters++;
        LastCluster = CurrentCluster;

        ULONG NextCluster = 0;
        Status = FatpGetNextCluster(Vcb, CurrentCluster, &NextCluster);
        if (!VEA_SUCCESS(Status))
        {
            KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
            Oip->StatusInformation.Status = Status;
            Oip->StatusInformation.Information = 0;
            return Status;
        }
        CurrentCluster = NextCluster;
    }

    /* 2. Perpanjang rantai cluster jika ukuran baru melewati kapasitas saat ini */
    while (ExistingClusters < RequiredClusters)
    {
        ULONG NewCluster = 0;
        Status = FatpAllocateCluster(Vcb, &NewCluster);
        if (!VEA_SUCCESS(Status))
        {
            KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
            Oip->StatusInformation.Status = Status;
            Oip->StatusInformation.Information = 0;
            return Status;
        }

        if (Fcb->FirstCluster < 2 || FAT32_IS_EOF(Fcb->FirstCluster))
        {
            Fcb->FirstCluster = NewCluster;
        }
        else
        {
            Status = FatpSetNextCluster(Vcb, LastCluster, NewCluster);
            if (!VEA_SUCCESS(Status))
            {
                KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
                Oip->StatusInformation.Status = Status;
                Oip->StatusInformation.Information = 0;
                return Status;
            }
        }

        LastCluster = NewCluster;
        ExistingClusters++;
    }

    /* 3. Lompat ke cluster awal tempat ByteOffset berada */
    ULONG ClustersToSkip    = (ULONG)(ByteOffset.QuadPart / Vcb->BytesPerCluster);
    ULONG OffsetInCluster   = (ULONG)(ByteOffset.QuadPart % Vcb->BytesPerCluster);
    CurrentCluster          = Fcb->FirstCluster;

    while (ClustersToSkip > 0 && CurrentCluster >= 2 && !FAT32_IS_EOF(CurrentCluster))
    {
        Status = FatpGetNextCluster(Vcb, CurrentCluster, &CurrentCluster);
        if (!VEA_SUCCESS(Status))
        {
            KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
            Oip->StatusInformation.Status = Status;
            Oip->StatusInformation.Information = 0;
            return Status;
        }
        ClustersToSkip--;
    }

    /* 4. Tulis data per cluster (gunakan Read-Modify-Write jika tidak selaras sektor) */
    ULONG BytesWritten = 0;
    ULONG SectorSize   = Vcb->BytesPerSector ? Vcb->BytesPerSector : 512;

    while (BytesWritten < Length && CurrentCluster >= 2 && !FAT32_IS_EOF(CurrentCluster))
    {
        ULONGLONG ClusterLba            = FAT32_CLUSTER_TO_LBA(Vcb, CurrentCluster);
        ULONGLONG VolumeByteOffset      = (ClusterLba * SectorSize) + OffsetInCluster;
        ULONG BytesAvailableInCluster   = Vcb->BytesPerCluster - OffsetInCluster;
        ULONG ChunkSize                 = Length - BytesWritten;

        if (ChunkSize > BytesAvailableInCluster)
        {
            ChunkSize = BytesAvailableInCluster;
        }

        if ((VolumeByteOffset % SectorSize != 0) || (ChunkSize % SectorSize != 0))
        {
            ULONG SectorOffset          = (ULONG)(VolumeByteOffset % SectorSize);
            ULONGLONG AlignedByteOffset = ALIGN_DOWN_BY(VolumeByteOffset, SectorSize);
            ULONG AlignedLength         = ALIGN_UP_BY(ChunkSize + SectorOffset, SectorSize);
            ULONGLONG StartLba          = AlignedByteOffset / SectorSize;
            ULONG SectorCount           = AlignedLength / SectorSize;

            PUCHAR BounceBuffer = (PUCHAR)UlAllocatePoolWithTag(NonPagedPool, AlignedLength, 'wVaF');
            if (!BounceBuffer)
            {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            /* Baca dulu sektor eksisting agar byte di luar rentang penulisan tetap utuh */
            Status = FatReadDiskSectors(Vcb->TargetDeviceObject, StartLba, SectorCount, SectorSize, BounceBuffer);
            if (VEA_SUCCESS(Status))
            {
                RtlCopyMemory(BounceBuffer + SectorOffset, UserBuffer + BytesWritten, ChunkSize);
                Status = FatWriteDiskSectors(Vcb->TargetDeviceObject, StartLba, SectorCount, SectorSize, BounceBuffer);
            }

            UlFreePoolWithTag(BounceBuffer, 'wVaF');
            if (!VEA_SUCCESS(Status))
            {
                break;
            }
        }
        else
        {
            ULONGLONG StartLba = VolumeByteOffset / SectorSize;
            ULONG SectorCount  = ChunkSize / SectorSize;

            Status = FatWriteDiskSectors(
                Vcb->TargetDeviceObject,
                StartLba,
                SectorCount,
                SectorSize,
                UserBuffer + BytesWritten
            );
            if (!VEA_SUCCESS(Status))
            {
                break;
            }
        }

        BytesWritten    += ChunkSize;
        OffsetInCluster  = 0;

        if (BytesWritten < Length)
        {
            Status = FatpGetNextCluster(Vcb, CurrentCluster, &CurrentCluster);
            if (!VEA_SUCCESS(Status))
            {
                break;
            }
        }
    }

    /* 5. Update ukuran file di FCB, CCB, dan entri direktori di disk */
    if (BytesWritten > 0)
    {
        ULONGLONG NewEnd = (ULONGLONG)ByteOffset.QuadPart + BytesWritten;
        if (NewEnd > Fcb->FileSize)
        {
            Fcb->FileSize = (ULONG)NewEnd;
        }

        if (FileObject->FsContext2)
        {
            PVEAFAT_CCB Ccb = (PVEAFAT_CCB)FileObject->FsContext2;
            Ccb->CurrentByteOffset = (ULONG)NewEnd;
        }

        FatpUpdateDirectoryEntry(Vcb, Fcb);
    }

    KsReleaseMutex(&Vcb->VolumeMutex, FALSE);

    Oip->StatusInformation.Information = BytesWritten;
    Oip->StatusInformation.Status = (BytesWritten > 0) ? STATUS_SUCCESS : Status;

    ObioCompleteRequest(Oip);
    return (BytesWritten > 0) ? STATUS_SUCCESS : Status;
}

/* EOF */