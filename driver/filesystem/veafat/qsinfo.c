/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : queryinfo.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Querying FAT32 File Info
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "veafat.h"

/* Revision History ------------------------------------------------------
 * DATE       : 15-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file queryinfo.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
FatMjQueryInformation(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK Stack
)
{
    PFAT32_DEVICE_EXTENSION Vcb = EXTENSION_FROM_DEVOBJ(DeviceObject);
    PFILE_OBJECT FileObject     = Stack->FileObject ? (PFILE_OBJECT)Stack->FileObject : Oip->OriginalFileObject;
    ULONG BufferLength          = Stack->Parameters.QueryFile.Length;
    FILE_INFORMATION_CLASS InfoClass = Stack->Parameters.QueryFile.FileInformationClass;
    PVOID Buffer                = Oip->AssociatedOip.SystemBuffer ? Oip->AssociatedOip.SystemBuffer : Oip->UserBuffer;
    PVEAFAT_FCB Fcb;
    PVEAFAT_CCB Ccb;

    if (Vcb->IsCdo || !FileObject || !FileObject->FsContext || !Buffer)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        Oip->StatusInformation.Information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    Fcb = (PVEAFAT_FCB)FileObject->FsContext;
    Ccb = (PVEAFAT_CCB)FileObject->FsContext2;
    RtlZeroMemory(Buffer, BufferLength);

    switch (InfoClass)
    {
        case FileBasicInformation:
        {
            if (BufferLength < sizeof(FILE_BASIC_INFORMATION))
            {
                Oip->StatusInformation.Status = STATUS_BUFFER_TOO_SMALL;
                return STATUS_BUFFER_TOO_SMALL;
            }

            PFILE_BASIC_INFORMATION BasicInfo = (PFILE_BASIC_INFORMATION)Buffer;
            BasicInfo->FileAttributes = Fcb->Attributes ? Fcb->Attributes : FILE_ATTRIBUTE_NORMAL;

            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = sizeof(FILE_BASIC_INFORMATION);
            return STATUS_SUCCESS;
        }

        case FileStandardInformation:
        {
            if (BufferLength < sizeof(FILE_STANDARD_INFORMATION))
            {
                Oip->StatusInformation.Status = STATUS_BUFFER_TOO_SMALL;
                return STATUS_BUFFER_TOO_SMALL;
            }

            PFILE_STANDARD_INFORMATION StdInfo = (PFILE_STANDARD_INFORMATION)Buffer;
            BOOLEAN IsDir = BooleanFlagOn(Fcb->Attributes, FAT_ATTR_DIRECTORY);

            StdInfo->EndOfFile.QuadPart      = IsDir ? 0 : Fcb->FileSize;
            StdInfo->AllocationSize.QuadPart = IsDir ? 0 : ALIGN_UP_BY((ULONGLONG)Fcb->FileSize, Vcb->BytesPerCluster);
            StdInfo->NumberOfLinks           = 1;
            StdInfo->DeletePending           = Fcb->DeletePending;
            StdInfo->Directory               = IsDir;

            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = sizeof(FILE_STANDARD_INFORMATION);
            return STATUS_SUCCESS;
        }

        case FilePositionInformation:
        {
            if (BufferLength < sizeof(FILE_POSITION_INFORMATION))
            {
                Oip->StatusInformation.Status = STATUS_BUFFER_TOO_SMALL;
                return STATUS_BUFFER_TOO_SMALL;
            }

            PFILE_POSITION_INFORMATION PosInfo = (PFILE_POSITION_INFORMATION)Buffer;
            PosInfo->CurrentByteOffset.QuadPart = Ccb ? Ccb->CurrentByteOffset : 0;

            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = sizeof(FILE_POSITION_INFORMATION);
            return STATUS_SUCCESS;
        }

        case FileInternalInformation:
        {
            if (BufferLength < sizeof(FILE_INTERNAL_INFORMATION))
            {
                Oip->StatusInformation.Status = STATUS_BUFFER_TOO_SMALL;
                return STATUS_BUFFER_TOO_SMALL;
            }

            PFILE_INTERNAL_INFORMATION IntInfo = (PFILE_INTERNAL_INFORMATION)Buffer;
            /* Gabungan ParentDirCluster & DirEntryByteOffset unik per file di FAT32 */
            IntInfo->IndexNumber.QuadPart = ((ULONGLONG)Fcb->ParentDirCluster << 32) | Fcb->DirEntryByteOffset;

            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = sizeof(FILE_INTERNAL_INFORMATION);
            return STATUS_SUCCESS;
        }

        default:
            Oip->StatusInformation.Status = STATUS_INVALID_INFO_CLASS;
            Oip->StatusInformation.Information = 0;
            return STATUS_INVALID_INFO_CLASS;
    }
}

VEASTATUS
VEAPI
FatpDeleteDirectoryEntry(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN PVEAFAT_FCB Fcb
)
{
    if (!Fcb || Fcb->IsVolumeFcb || Fcb->ParentDirCluster < 2)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PUCHAR SectorBuffer = UlAllocatePoolZero(NonPagedPool, Vcb->BytesPerSector, VEAFAT_POOL_TAG);
    if (!SectorBuffer) return STATUS_INSUFFICIENT_RESOURCES;

    /* 1. Tandai entri SFN utama dengan 0xE5 */
    ULONG SectorIndex    = Fcb->DirEntryByteOffset / Vcb->BytesPerSector;
    ULONG OffsetInSector = Fcb->DirEntryByteOffset % Vcb->BytesPerSector;
    ULONGLONG SectorLba  = FAT32_CLUSTER_TO_LBA(Vcb, Fcb->ParentDirCluster) + SectorIndex;

    VEASTATUS Status = FatReadDiskSectors(Vcb->TargetDeviceObject, SectorLba, 1, Vcb->BytesPerSector, SectorBuffer);
    if (!VEA_SUCCESS(Status))
    {
        UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);
        return Status;
    }

    PFAT_DIR_ENTRY SfnEntry = (PFAT_DIR_ENTRY)(SectorBuffer + OffsetInSector);
    SfnEntry->FileName[0] = 0xE5;
    Status = FatWriteDiskSectors(Vcb->TargetDeviceObject, SectorLba, 1, Vcb->BytesPerSector, SectorBuffer);

    /* 2. Mundur ke belakang untuk menandai seluruh entri LFN (0x0F) dengan 0xE5 */
    ULONG ScanOffset = Fcb->DirEntryByteOffset;
    while (VEA_SUCCESS(Status) && ScanOffset >= sizeof(FAT_DIR_ENTRY))
    {
        ScanOffset -= sizeof(FAT_DIR_ENTRY);
        ULONG LfnSectorIdx = ScanOffset / Vcb->BytesPerSector;
        ULONG LfnOffInSec  = ScanOffset % Vcb->BytesPerSector;
        ULONGLONG LfnLba   = FAT32_CLUSTER_TO_LBA(Vcb, Fcb->ParentDirCluster) + LfnSectorIdx;

        Status = FatReadDiskSectors(Vcb->TargetDeviceObject, LfnLba, 1, Vcb->BytesPerSector, SectorBuffer);
        if (!VEA_SUCCESS(Status)) break;

        PFAT_DIR_ENTRY PrevEntry = (PFAT_DIR_ENTRY)(SectorBuffer + LfnOffInSec);
        if (PrevEntry->Attributes != FAT_ATTR_LFN ||
            PrevEntry->FileName[0] == 0x00 ||
            PrevEntry->FileName[0] == 0xE5)
        {
            break;
        }

        BOOLEAN IsLastLfn = (PrevEntry->FileName[0] & FAT_LFN_LAST_ENTRY) != 0;
        PrevEntry->FileName[0] = 0xE5;
        Status = FatWriteDiskSectors(Vcb->TargetDeviceObject, LfnLba, 1, Vcb->BytesPerSector, SectorBuffer);

        if (IsLastLfn) break;
    }

    UlFreePoolWithTag(SectorBuffer, VEAFAT_POOL_TAG);

    /* 3. Bebaskan rantai cluster data milik file/direktori */
    if (VEA_SUCCESS(Status) && Fcb->FirstCluster >= 2 && !FAT32_IS_EOF(Fcb->FirstCluster))
    {
        Status = FatpFreeClusterChain(Vcb, Fcb->FirstCluster);
        Fcb->FirstCluster = 0;
        Fcb->FileSize = 0;
    }

    return Status;
}

VEASTATUS
VEAPI
FatMjSetInformation(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK Stack
)
{
    PFAT32_DEVICE_EXTENSION Vcb = EXTENSION_FROM_DEVOBJ(DeviceObject);
    PFILE_OBJECT FileObject     = Stack->FileObject ? (PFILE_OBJECT)Stack->FileObject : Oip->OriginalFileObject;
    ULONG BufferLength          = Stack->Parameters.SetFile.Length;
    FILE_INFORMATION_CLASS InfoClass = Stack->Parameters.SetFile.FileInformationClass;
    PVOID Buffer                = Oip->AssociatedOip.SystemBuffer ? Oip->AssociatedOip.SystemBuffer : Oip->UserBuffer;
    PVEAFAT_FCB Fcb;
    PVEAFAT_CCB Ccb;
    VEASTATUS Status            = STATUS_SUCCESS;

    if (Vcb->IsCdo || !FileObject || !FileObject->FsContext || !Buffer)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        return STATUS_INVALID_PARAMETER;
    }

    Fcb = (PVEAFAT_FCB)FileObject->FsContext;
    Ccb = (PVEAFAT_CCB)FileObject->FsContext2;

    switch (InfoClass)
    {
        case FilePositionInformation:
        {
            if (BufferLength < sizeof(FILE_POSITION_INFORMATION))
            {
                Oip->StatusInformation.Status = STATUS_BUFFER_TOO_SMALL;
                return STATUS_BUFFER_TOO_SMALL;
            }

            PFILE_POSITION_INFORMATION PosInfo = (PFILE_POSITION_INFORMATION)Buffer;
            if (Ccb)
            {
                Ccb->CurrentByteOffset = (ULONG)PosInfo->CurrentByteOffset.QuadPart;
            }
            Oip->StatusInformation.Status = STATUS_SUCCESS;
            return STATUS_SUCCESS;
        }

        case FileDispositionInformation:
        {
            if (BufferLength < sizeof(FILE_DISPOSITION_INFORMATION))
            {
                Oip->StatusInformation.Status = STATUS_BUFFER_TOO_SMALL;
                return STATUS_BUFFER_TOO_SMALL;
            }

            if (Vcb->VolumeFlags & VCB_STATE_READ_ONLY)
            {
                Oip->StatusInformation.Status = STATUS_MEDIA_WRITE_PROTECTED;
                return STATUS_MEDIA_WRITE_PROTECTED;
            }

            if (Fcb->IsVolumeFcb || Fcb->FirstCluster == Vcb->RootDirStartCluster ||
                (Fcb->Attributes & FAT_ATTR_READ_ONLY))
            {
                Oip->StatusInformation.Status = STATUS_ACCESS_DENIED;
                return STATUS_ACCESS_DENIED;
            }

            PFILE_DISPOSITION_INFORMATION DispInfo = (PFILE_DISPOSITION_INFORMATION)Buffer;
            KsWaitForSingleObject(&Vcb->VolumeMutex);

            if (DispInfo->DeleteFile)
            {
                if ((Fcb->Attributes & FAT_ATTR_DIRECTORY) && !FatpIsDirectoryEmpty(Vcb, Fcb->FirstCluster))
                {
                    KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
                    Oip->StatusInformation.Status = STATUS_DIRECTORY_NOT_EMPTY;
                    return STATUS_DIRECTORY_NOT_EMPTY;
                }
                Fcb->DeletePending = TRUE;
            }
            else
            {
                Fcb->DeletePending = FALSE;
            }

            KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
            Oip->StatusInformation.Status = STATUS_SUCCESS;
            return STATUS_SUCCESS;
        }

        case FileEndOfFileInformation:
        case FileAllocationInformation:
        {
            if (BufferLength < sizeof(FILE_END_OF_FILE_INFORMATION))
            {
                Oip->StatusInformation.Status = STATUS_BUFFER_TOO_SMALL;
                return STATUS_BUFFER_TOO_SMALL;
            }

            if (Fcb->IsVolumeFcb || (Fcb->Attributes & (FAT_ATTR_DIRECTORY | FAT_ATTR_READ_ONLY)))
            {
                Oip->StatusInformation.Status = STATUS_ACCESS_DENIED;
                return STATUS_ACCESS_DENIED;
            }

            ULONG NewSize = (ULONG)((PFILE_END_OF_FILE_INFORMATION)Buffer)->EndOfFile.QuadPart;
            KsWaitForSingleObject(&Vcb->VolumeMutex);

            ULONG NeededClusters = FAT32_BYTES_TO_CLUSTERS(Vcb, NewSize);
            if (NeededClusters == 0)
            {
                if (Fcb->FirstCluster >= 2 && !FAT32_IS_EOF(Fcb->FirstCluster))
                {
                    FatpFreeClusterChain(Vcb, Fcb->FirstCluster);
                }
                Fcb->FirstCluster = 0;
            }
            else
            {
                /* Telusuri rantai sampai cluster ke-NeededClusters, potong sisanya */
                ULONG Count = 0;
                ULONG Curr  = Fcb->FirstCluster;
                ULONG Prev  = 0;

                while (Curr >= 2 && !FAT32_IS_EOF(Curr) && Count < NeededClusters)
                {
                    Prev = Curr;
                    Count++;
                    Status = FatpGetNextCluster(Vcb, Curr, &Curr);
                    if (!VEA_SUCCESS(Status)) break;
                }

                if (VEA_SUCCESS(Status) && Count == NeededClusters && Curr >= 2 && !FAT32_IS_EOF(Curr))
                {
                    FatpSetNextCluster(Vcb, Prev, 0x0FFFFFFF);
                    FatpFreeClusterChain(Vcb, Curr);
                }

                while (VEA_SUCCESS(Status) && Count < NeededClusters)
                {
                    ULONG NewClus = 0;
                    Status = FatpAllocateCluster(Vcb, &NewClus);
                    if (!VEA_SUCCESS(Status)) break;

                    if (Fcb->FirstCluster < 2)
                        Fcb->FirstCluster = NewClus;
                    else
                        FatpSetNextCluster(Vcb, Prev, NewClus);

                    Prev = NewClus;
                    Count++;
                }
            }

            if (VEA_SUCCESS(Status))
            {
                Fcb->FileSize = NewSize;
                Status = FatpUpdateDirectoryEntry(Vcb, Fcb);
            }

            KsReleaseMutex(&Vcb->VolumeMutex, FALSE);
            Oip->StatusInformation.Status = Status;
            return Status;
        }

        default:
            Oip->StatusInformation.Status = STATUS_INVALID_INFO_CLASS;
            return STATUS_INVALID_INFO_CLASS;
    }
}

/* EOF */