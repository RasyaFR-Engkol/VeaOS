/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : fsctl.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : FAT32 Fs Control
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "veafat.h"

/* Revision History ------------------------------------------------------
 * DATE       : 17-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file fsctl.c
 * --------------------------------------------------------------------- */

VEASTATUS
FatReadDiskSectors(
    IN PDEVICE_OBJECT TargetDevice,
    IN ULONGLONG Lba,
    IN ULONG SectorCount,
    IN ULONG BytesPerSector,
    OUT PVOID Buffer
)
{
    LARGE_INTEGER ByteOffset;
    POIP Oip;
    OIP_INFORMATION Information;
    KEVENT Event;
    VEASTATUS Status;
    ULONG ReadLength;

    // 1. Hitung total panjang byte yang dibaca
    ReadLength = SectorCount * BytesPerSector;
    ByteOffset.QuadPart = Lba * BytesPerSector;

    // 2. Inisialisasi event lokal untuk menahan eksekusi jika async
    KsInitializeEvent(&Event, NotificationEvent, FALSE);

    // 3. Build OIP dengan total ReadLength & pointer Event
    Oip = ObioBuildSynchronusFsdRequest(
        OIP_MJ_READ,
        TargetDevice,
        Buffer,
        ReadLength,          // Total byte (SectorCount * BytesPerSector)
        &ByteOffset,
        &Event,               // Event buat penanda selesai
        &Information
    );

    if (!Oip) return STATUS_INSUFFICIENT_MEMORY;

    // 4. Lempar OIP ke target storage driver
    Status = ObioCallObject(TargetDevice, Oip);

    // 5. Jika driver disk bekerja secara asynchronous, tunggu sampai event di-signal
    if (Status == STATUS_PENDING)
    {
        KsWaitForSingleObject(&Event);
        Status = Information.Status;
    }

    return Status;
}

VEASTATUS
VEAPI
FatValidateVbr(IN PFAT32_VBR Vbr)
{
    PUCHAR RawBytes = (PUCHAR)Vbr;

    DPRINT("=================== FAT32 VBR DUMP ===================\n");
    
    // Dump 64 byte pertama VBR (Header & OEM Name)
    for (INT i = 0; i < 64; i += 16)
    {
        DPRINT("%04X: %02X %02X %02X %02X %02X %02X %02X %02X - %02X %02X %02X %02X %02X %02X %02X %02X\n",
               i,
               RawBytes[i+0], RawBytes[i+1], RawBytes[i+2], RawBytes[i+3],
               RawBytes[i+4], RawBytes[i+5], RawBytes[i+6], RawBytes[i+7],
               RawBytes[i+8], RawBytes[i+9], RawBytes[i+10], RawBytes[i+11],
               RawBytes[i+12], RawBytes[i+13], RawBytes[i+14], RawBytes[i+15]);
    }

    // Inspect field krusial FAT32
    DPRINT("OEM Name          : %08X\n", Vbr->OemName);
    DPRINT("BytesPerSector    : %u\n", Vbr->BytesPerSector);
    DPRINT("SectorsPerCluster : %u\n", Vbr->SectorsPerCluster);
    DPRINT("ReservedSectors   : %u\n", Vbr->ReservedSectors);
    DPRINT("FatSize32         : %u\n", Vbr->FatSize32);
    DPRINT("MagicSignature    : 0x%04X (Expected: 0xAA55)\n", Vbr->MagicSignature);
    DPRINT("======================================================\n");

    if (Vbr->MagicSignature != 0xAA55 || Vbr->FatSize32 == 0)
    {
        DPRINT("FAT32: Invalid VBR signature or not FAT32\n");
        return STATUS_UNRECOGNIZED_VOLUME;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
FatMjFsCtl(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    VEASTATUS Status;
    PFAT32_DEVICE_EXTENSION CdoExt;
    PFAT32_DEVICE_EXTENSION Vcb;
    PDEVICE_OBJECT TargetDevice;
    PDEVICE_OBJECT Vdo = NULL;
    PVPB Vpb;
    FAT32_VBR Vbr;
    FAT32_FSINFO FsInfo;
    ULONG TotalSectors;
    ULONG DataSectors;
    ULONG TotalClusters;
    ULONG BytesPerSector;

    CdoExt = EXTENSION_FROM_DEVOBJ(DeviceObject);

    /* check for FS operation */
    if (!CdoExt->IsCdo)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_DEVICE_REQUEST;
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (OipStack->MinorFunction == OIP_MN_MOUNT_VOLUME)
    {
        TargetDevice = OipStack->Parameters.MountVolume.Vpb->RealDevice;
        Vpb = OipStack->Parameters.MountVolume.Vpb;

        if (!TargetDevice || !Vpb)
        {
            Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
            return STATUS_INVALID_PARAMETER;
        }

        /* Ask for DISK_GEOMETRY */
        DISK_GEOMETRY DiskGeometry;
        POIP IoctlOip;
        KEVENT IoctlEvent;
        OIP_INFORMATION IoStatusBlock;
        VEASTATUS Status;

        KsInitializeEvent(&IoctlEvent, NotificationEvent, FALSE);

        IoctlOip = ObioBuildDeviceIoControlRequest(
            IOCTL_DISK_GET_DRIVE_GEOMETRY,
            TargetDevice,
            NULL,
            0,
            &DiskGeometry,
            sizeof(DISK_GEOMETRY),
            FALSE,
            &IoctlEvent,
            &IoStatusBlock
        );

        if (!IoctlOip)
        {
            DPRINT("FAT32: Failed to create IOCTL OIP\n");
            return STATUS_INSUFFICIENT_MEMORY;
        }

        Status = ObioCallObject(TargetDevice, IoctlOip);
        if (Status == STATUS_PENDING)
        {
            KsWaitForSingleObject(&IoctlEvent);
            Status = IoStatusBlock.Status;
        }

        if (!VEA_SUCCESS(Status))
        {
            DPRINT("FAT32: Failed to get Disk Geometry: 0x%x\n", Status);
            return Status;
        }

        BytesPerSector = DiskGeometry.BytesPerSector;

        Status = FatReadDiskSectors(TargetDevice, 0, 1, BytesPerSector, &Vbr);
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("FAT32: Failed to read VBR: 0x%x\n", Status);
            Oip->StatusInformation.Status = Status;
            return Status;
        }

        Status = FatValidateVbr(&Vbr);
        if (VEA_SUCCESS(Status))
        {
            PDEVICE_OBJECT Vdo = NULL;
            Status = ObioCreateNewDevice(
                DeviceObject->DriverObject,
                sizeof(FAT32_DEVICE_EXTENSION),
                NULL,
                FILE_DEVICE_DISK_FILE_SYSTEM,
                &Vdo
            );

            if (!VEA_SUCCESS(Status))
            {
                Oip->StatusInformation.Status = Status;
                return Status;
            }

            PFAT32_DEVICE_EXTENSION Vcb = EXTENSION_FROM_DEVOBJ(Vdo);
            RtlZeroMemory(Vcb, sizeof(FAT32_DEVICE_EXTENSION));

            Vcb->IsCdo = FALSE;
            Vcb->VolumeDeviceObject = Vdo;
            Vcb->TargetDeviceObject = TargetDevice;
            Vcb->Vpb = Vpb;

            Vcb->BytesPerSector = Vbr.BytesPerSector;
            Vcb->SectorsPerCluster = Vbr.SectorsPerCluster;
            Vcb->BytesPerCluster = Vcb->BytesPerSector * Vcb->SectorsPerCluster;

            Vcb->ReservedSectors = Vbr.ReservedSectors;
            Vcb->NumberOfFats = Vbr.NumberOfFats;
            Vcb->FatSizeSectors = Vbr.FatSize32;

            Vcb->RootDirStartCluster = Vbr.RootCluster;
            Vcb->FSInfoSector = Vbr.FSInfoSector;
            Vcb->BackupBootSector = Vbr.BackupBootSector;

            Vcb->FatStartLba = Vcb->ReservedSectors;
            Vcb->DataStartLba = Vcb->ReservedSectors + ((ULONGLONG)Vcb->NumberOfFats * Vcb->FatSizeSectors);

            Vcb->TotalSectors = (Vbr.TotalSectors32 != 0) ? Vbr.TotalSectors32 : Vbr.TotalSectors16;
            Vcb->TotalClusters = (ULONG)((Vcb->TotalSectors - Vcb->DataStartLba) / Vcb->SectorsPerCluster);

            Vcb->FreeClusterCount = 0xFFFFFFFF;
            Vcb->NextFreeCluster = 0xFFFFFFFF;
            Vcb->FsInfoDirty = FALSE;

            if (Vcb->FSInfoSector != 0 && Vcb->FSInfoSector != 0xFFFF)
            {
                FAT32_FSINFO FsInfo;
                Status = FatReadDiskSectors(TargetDevice, Vcb->FSInfoSector, 1, Vcb->BytesPerSector, &FsInfo);
                if (VEA_SUCCESS(Status))
                {
                    if (FsInfo.LeadSignature == 0x41615252 && FsInfo.StrucSignature == 0x61417272)
                    {
                        Vcb->FreeClusterCount = FsInfo.FreeClusterCount;
                        Vcb->NextFreeCluster = FsInfo.NextFreeCluster;
                    }
                }
            }

            InitializeListHead(&Vcb->FcbListHead);
            KsInitializeSpinlock(&Vcb->FcbListLock);
            KsInitializeMutex(&Vcb->VolumeMutex, 0);

            Vcb->VolumeFlags = VCB_STATE_MOUNTED;

            Vpb->DeviceObject = Vdo;
            Vpb->Flags |= VPB_MOUNTED;
            Vdo->Flags &= ~DO_DEVICE_INITIALIZING;

            DPRINT("FAT32: Volume successfully mounted! DataLBA: %u, Clusters: %u\n", 
                (ULONG)Vcb->DataStartLba, Vcb->TotalClusters);

            Oip->StatusInformation.Status = STATUS_SUCCESS;
            return STATUS_SUCCESS;
        }
        else
        {
            Oip->StatusInformation.Status = Status;
            return Status;
        }
    }
    else
    {
        DPRINT("FAT32: Unsupported operation: 0x%x\n", OipStack->MinorFunction);
        Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
        return STATUS_NOT_SUPPORTED;
    }
}