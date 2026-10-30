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
    PULONG FatTable;

    if (!Vcb || !NextCluster || Vcb->BytesPerSector > sizeof(FatBuffer))
    {
        return STATUS_INVALID_PARAMETER;
    }

    FatBuffer = (PUCHAR)UlAllocatePoolZero(
        NonPagedPool,
        4096,
        'FAT '
    );
    if (!FatBuffer)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    /* Hitung LBA sektor FAT dan offset byte di dalam sektor tersebut */
    FatSectorLba = Vcb->FatStartLba + ((ULONGLONG)CurrentCluster * 4 / Vcb->BytesPerSector);
    FatOffsetInSector = (CurrentCluster * 4) % Vcb->BytesPerSector;

    /* Tembak OIP Read sektor FAT ke TargetDeviceObject */
    Status = FatReadDiskSectors(
        Vcb->TargetDeviceObject,
        FatSectorLba,
        1,
        Vcb->BytesPerSector,
        FatBuffer
    );
    
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* Cast buffer byte ke PULONG */
    FatTable = (PULONG)FatBuffer;

    /* Top 4-bit di FAT32 di-reserved, ambil 28-bit bawahnya (0x0FFFFFFF) */
    *NextCluster = FatTable[FatOffsetInSector / 4] & 0x0FFFFFFF;

    UlFreePoolWithTag(FatBuffer, 'FAT ');

    return STATUS_SUCCESS;
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
    PUCHAR UserBuffer = (PUCHAR)Oip->UserBuffer;

    if (ByteOffset.QuadPart >= Fcb->FileSize)
    {
        Oip->StatusInformation.Status = STATUS_END_OF_FILE;
        Oip->StatusInformation.Information = 0;
        return STATUS_END_OF_FILE;
    }

    if ((ByteOffset.QuadPart + Length) > Fcb->FileSize)
    {
        BytesToRead = (ULONG)(Fcb->FileSize - ByteOffset.QuadPart);
    }

    ULONG ClustersToSkip = (ULONG)(ByteOffset.QuadPart / Vcb->BytesPerCluster);
    ULONG OffsetInFirstCluster = (ULONG)(ByteOffset.QuadPart % Vcb->BytesPerCluster);

    CurrentCluster = Fcb->FirstCluster;

    while (ClustersToSkip > 0 && !FAT32_IS_EOF(CurrentCluster) && CurrentCluster != 0)
    {
        Status = FatpGetNextCluster(Vcb, CurrentCluster, &CurrentCluster);
        if (!VEA_SUCCESS(Status))
        {
            Oip->StatusInformation.Status = Status;
            Oip->StatusInformation.Information = 0;
            return Status;
        }
        ClustersToSkip--;
    }

    if (FAT32_IS_EOF(CurrentCluster) || CurrentCluster == 0)
    {
        Oip->StatusInformation.Status = STATUS_END_OF_FILE;
        Oip->StatusInformation.Information = 0;
        return STATUS_END_OF_FILE;
    }

    while (BytesRead < BytesToRead && !FAT32_IS_EOF(CurrentCluster) && CurrentCluster != 0)
    {
        ULONGLONG ClusterLba = FAT32_CLUSTER_TO_LBA(Vcb, CurrentCluster);
        LARGE_INTEGER VolumeByteOffset;

        VolumeByteOffset.QuadPart = (ClusterLba * Vcb->BytesPerSector) + OffsetInFirstCluster;

        ULONG BytesAvailableInCluster = Vcb->BytesPerCluster - OffsetInFirstCluster;
        ULONG ChunkSize = BytesToRead - BytesRead;

        if (ChunkSize > BytesAvailableInCluster)
        {
            ChunkSize = BytesAvailableInCluster;
        }

        /* Make Sub-OIP
           TODO: Would be cool if we use AssociatedOip 
        */
        POIP OipLocal;
        KEVENT Event;
        OIP_INFORMATION InformationBlock;

        KsInitializeEvent(&Event, NotificationEvent, FALSE);

        OipLocal = ObioBuildSynchronusFsdRequest(
            OIP_MJ_READ,
            Vcb->TargetDeviceObject,
            UserBuffer + BytesRead,
            ChunkSize,
            &VolumeByteOffset,
            &Event,
            &InformationBlock
        );

        if (!OipLocal)
        {
            Oip->StatusInformation.Status = STATUS_INSUFFICIENT_MEMORY;
            return STATUS_INSUFFICIENT_MEMORY;
        }

        Status = ObioCallObject(
            Vcb->TargetDeviceObject,
            OipLocal
        );

        if (Status == STATUS_PENDING)
        {
            KsWaitForSingleObject(&Event);
            Status = InformationBlock.Status;
        }

        if (!VEA_SUCCESS(Status))
        {
            break;
        }

        BytesRead += ChunkSize;
        OffsetInFirstCluster = 0;

        if (BytesRead < BytesToRead)
        {
            Status = FatpGetNextCluster(Vcb, CurrentCluster, &CurrentCluster);
            if (!VEA_SUCCESS(Status))
            {
                break;
            }
        }
    }

    Oip->StatusInformation.Information = BytesRead;
    Oip->StatusInformation.Status = (BytesRead > 0) ? STATUS_SUCCESS : Status;

    if (FileObject && FileObject->FsContext2)
    {
        PVEAFAT_CCB Ccb = (PVEAFAT_CCB)FileObject->FsContext2;
        Ccb->CurrentByteOffset = (ULONG)(ByteOffset.QuadPart + BytesRead);
    }

    return Oip->StatusInformation.Status;
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
    PFILE_OBJECT FileObject = Stack->FileObject;
    PVEAFAT_FCB Fcb = NULL;
    PFAT32_DEVICE_EXTENSION DevExt = EXTENSION_FROM_DEVOBJ(DeviceObject);
    ULONG Length = Stack->Parameters.ReadWrite.Length;
    LARGE_INTEGER ByteOffset = Stack->Parameters.ReadWrite.Offset;

    /* If No FileObject, or FsContext */
    if (!FileObject || !FileObject->FsContext)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        return STATUS_INVALID_PARAMETER;
    }

    /* Get File Control Object */
    Fcb = (PVEAFAT_FCB)FileObject->FsContext;

    /* Length 0 just say it succeed */
    if (Length == 0)
    {
        Oip->StatusInformation.Information = 0;
        Oip->StatusInformation.Status = STATUS_SUCCESS;
        return STATUS_SUCCESS;
    }

    /* Check if ByteOffset is exceeding file size */
    if (ByteOffset.QuadPart >= Fcb->FileSize)
    {
        /* End of File reached */
        Oip->StatusInformation.Status = STATUS_END_OF_FILE;
        Oip->StatusInformation.Information = 0;
        return STATUS_END_OF_FILE;
    }

    /* Pass this to internal function */
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