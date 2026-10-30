/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : main.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Diska Management (Pure Disk Class FDO Driver)
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "diskahdr.h"

/* Revision History ------------------------------------------------------
- DATE       : 11-10-2026
- NAME       : RasyaFR-Engkol
- REVISION   : Creating file main.c
- DATE       : 11-10-2026
- NAME       : RasyaFR-Engkol
- REVISION   : Refactored diska.ko to Pure FDO Driver (separated from veapartmgr)
- --------------------------------------------------------------------- */

ULONG DskNextDiskNum = 0;

ULONG
VEAPI
DskGetNextDiskNumber(VOID)
{
    return __atomic_fetch_add(&DskNextDiskNum, 1, __ATOMIC_SEQ_CST);
}

VEASTATUS
VEAPI
DskReadSectors(
    IN PDISKA_FDO_EXTENSION FdoExt,
    IN ULONGLONG StartLba,
    IN ULONG SectorCount,
    OUT PVOID Buffer
)
{
    PDEVICE_OBJECT Target = FdoExt->LowerDeviceObject;
    ULONG SectorSize = 512; // Default sector size
    ULONG Length = SectorCount * SectorSize;
    LARGE_INTEGER Offset;
    KEVENT Event;
    OIP_INFORMATION Iosb;
    POIP Oip;
    VEASTATUS Status;

    if (!FdoExt || !Target || !Buffer || SectorCount == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Offset.QuadPart = StartLba * SectorSize;
    KsInitializeEvent(&Event, NotificationEvent, FALSE);
    RtlZeroMemory(&Iosb, sizeof(Iosb));

    /* Build Synchronous FSD Read Request ke Driver Controller di bawah (AHCI / NVMe) */
    Oip = ObioBuildSynchronusFsdRequest(
        OIP_MJ_READ,
        Target,
        Buffer,
        Length,
        &Offset,
        &Event,
        &Iosb
    );

    if (!Oip)
    {
        return STATUS_NO_RESOURCES;
    }

    Status = ObioGoInterruptObject(Target, Oip);
    if (Status == STATUS_PENDING)
    {
        KsWaitForSingleObject(&Event);
        Status = Iosb.Status;
    }
    else
    {
        /* Jika dikembalikan synchronous, ambil status dari Iosb jika valid */
        if (VEA_SUCCESS(Status) && Iosb.Status != STATUS_SUCCESS)
        {
            Status = Iosb.Status;
        }
    }

    if (VEA_SUCCESS(Status) && Iosb.Information != Length)
    {
        Status = STATUS_UNSUCCESSFUL; /* Partial read protection */
    }

    return Status;
}

VEASTATUS
VEAPI
DskMjPnp(
    IN PDEVICE_OBJECT Fdo,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PDISKA_FDO_EXTENSION FdoExt = (PDISKA_FDO_EXTENSION)Fdo->DeviceExtension;

    switch (OipStack->MinorFunction)
    {
        case OIP_MN_QUERY_DEVICE_RELATIONS:
            if (!FdoExt->LowerDeviceObject)
            {
                Oip->StatusInformation.Status = STATUS_DEVICE_NOT_READY;
                Oip->StatusInformation.Information = 0;
                return STATUS_DEVICE_NOT_READY;
            }

            ObioSkipCurrentOipStackLocation(Oip);
            return ObioGoInterruptObject(FdoExt->LowerDeviceObject, Oip);

        case OIP_MN_START_DEVICE:
        {
            VEASTATUS Status;
            PVOID HarddiskDirectory;
            OBJECT_ATTRIBUTES Attributes;
            ANSI_STRING HarddiskPath;
            CHAR DirectoryName[32];

            if (FdoExt->IsStarted)
            {
                Oip->StatusInformation.Status = STATUS_SUCCESS;
                Oip->StatusInformation.Information = 0;
                return STATUS_SUCCESS;
            }

            /* Start the device stack below DiskaSys before publishing the disk link. */
            if (!FdoExt->LowerDeviceObject)
            {
                Oip->StatusInformation.Status = STATUS_DEVICE_NOT_READY;
                Oip->StatusInformation.Information = 0;
                return STATUS_DEVICE_NOT_READY;
            }

            ObioSkipCurrentOipStackLocation(Oip);
            Status = ObioGoInterruptObject(FdoExt->LowerDeviceObject, Oip);
            if (!VEA_SUCCESS(Status))
            {
                Oip->StatusInformation.Status = Status;
                return Status;
            }

            Status = RtlStringCbPrintfA(
                DirectoryName,
                sizeof(DirectoryName),
                "/Device/Harddisk%u",
                FdoExt->DiskNumber
            );
            if (!VEA_SUCCESS(Status))
            {
                Oip->StatusInformation.Status = Status;
                return Status;
            }

            RtlInitAnsiString(&HarddiskPath, DirectoryName);
            InitializeObjectAttributes(&Attributes, &HarddiskPath, 0, NULL, NULL);
            Status = ObReferenceObjectByName(&Attributes, NULL, &HarddiskDirectory);
            if (!VEA_SUCCESS(Status))
            {
                D_ERROR("Failed to open Harddisk%u directory: 0x%x\n",
                        FdoExt->DiskNumber,
                        Status);
                Oip->StatusInformation.Status = Status;
                return Status;
            }

            Status = ObCreateSymbolicLinkToObject(
                HarddiskDirectory,
                "Partition0",
                FdoExt->Self
            );
            ObDereferenceObject(HarddiskDirectory);
            if (!VEA_SUCCESS(Status))
            {
                D_ERROR("Failed to create Harddisk%u/Partition0 link: 0x%x\n",
                        FdoExt->DiskNumber,
                        Status);
                Oip->StatusInformation.Status = Status;
                return Status;
            }

            Status = ObioRegisterDeviceInterface(
                FdoExt->PhysicalDeviceObject,
                &VEA_GUID_DEVINTERFACE_DISK,
                NULL,
                &FdoExt->InterfaceSymbolicLink
            );
            if (!VEA_SUCCESS(Status))
            {
                D_ERROR("Failed to register device interface: 0x%x\n", Status);
                Oip->StatusInformation.Status = Status;
                return Status;
            }

            Status = ObioSetDeviceInterfaceState(&FdoExt->InterfaceSymbolicLink, TRUE);
            if (!VEA_SUCCESS(Status))
            {
                D_ERROR("Failed to set device interface state to TRUE: 0x%x\n", Status);
                Oip->StatusInformation.Status = Status;
                return Status;
            }

            FdoExt->IsStarted = TRUE;
            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = 0;
            return STATUS_SUCCESS;
        }

        default:
            D_INFO("Unhandled OIP_MN_*: 0x%x\n", OipStack->MinorFunction);
            return STATUS_NOT_SUPPORTED;
    }
}

VEASTATUS
VEAPI
DskQueryDiskStyle(
    IN PDISKA_FDO_EXTENSION FdoExt,
    IN POIP Oip,
    IN POIP_STACK Stack
)
{
    VEASTATUS Status;
    PUCHAR SectorBuffer;
    PDISK_STYLE OutStyle;
    PVOID UserBuffer;
    ULONG OutputLength = Stack->Parameters.DeviceControl.OutputBufferLength;

    if (OutputLength < sizeof(DISK_STYLE))
    {
        Oip->StatusInformation.Status = STATUS_BUFFER_TOO_SMALL;
        Oip->StatusInformation.Information = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    /* Ambil buffer output (dukung SystemBuffer maupun UserBuffer) */
    UserBuffer = Oip->AssociatedOip.SystemBuffer ? Oip->AssociatedOip.SystemBuffer : Oip->UserBuffer;
    if (!UserBuffer)
    {
        Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
        Oip->StatusInformation.Information = 0;
        return STATUS_INVALID_PARAMETER;
    }

    OutStyle = (PDISK_STYLE)UserBuffer;

    /* Alokasi buffer 1024 byte untuk membaca LBA 0 & LBA 1 */
    SectorBuffer = (PUCHAR)UlAllocatePoolWithTag(NonPagedPool, 1024, TAG_OB);
    if (!SectorBuffer)
    {
        D_ERROR("Buffer allocation for Query Disk Style failed\n");
        Oip->StatusInformation.Status = STATUS_INSUFFICIENT_RESOURCES;
        Oip->StatusInformation.Information = 0;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(SectorBuffer, 1024);

    /* Baca LBA 0 & LBA 1 via DskReadSectors */
    Status = DskReadSectors(FdoExt, 0, 2, SectorBuffer);
    if (!VEA_SUCCESS(Status))
    {
        D_ERROR("DskReadSectors for DiskStyle failed: 0x%x\n", Status);
        UlFreePoolWithTag(SectorBuffer, TAG_OB);
        Oip->StatusInformation.Status = Status;
        Oip->StatusInformation.Information = 0;
        return Status;
    }

    PUCHAR Lba0 = SectorBuffer;
    PUCHAR Lba1 = SectorBuffer + 512;

    /* 1. Cek Signature GPT di LBA 1 ("EFI PART") */
    if (RtlCompareMemory(Lba1, "EFI PART", 8) == 8)
    {
        *OutStyle = DiskStyleGpt;
    }
    /* 2. Cek Signature MBR di LBA 0 (Offset 510: 0x55, 511: 0xAA) */
    else if (Lba0[510] == 0x55 && Lba0[511] == 0xAA)
    {
        *OutStyle = DiskStyleMbr;
    }
    else
    {
        *OutStyle = (DISK_STYLE)-1; // Raw / Unformatted
    }

    UlFreePoolWithTag(SectorBuffer, TAG_OB);

    Oip->StatusInformation.Status = STATUS_SUCCESS;
    Oip->StatusInformation.Information = sizeof(DISK_STYLE);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
DskFdoHandler(
    IN PDEVICE_OBJECT Fdo,
    IN POIP Oip,
    IN POIP_STACK OipStack
)
{
    PDISKA_FDO_EXTENSION FdoExt = (PDISKA_FDO_EXTENSION)Fdo->DeviceExtension;

    switch (OipStack->MajorFunction)
    {
        case OIP_MJ_DEVICE_CONTROL:
            if (OipStack->Parameters.DeviceControl.IoControlCode ==
                IOCTL_DISK_GET_HARDDISK_NUMBER)
            {
                if (OipStack->Parameters.DeviceControl.OutputBufferLength <
                    sizeof(FdoExt->DiskNumber))
                {
                    Oip->StatusInformation.Status = STATUS_BUFFER_TOO_SMALL;
                    Oip->StatusInformation.Information = 0;
                    return STATUS_BUFFER_TOO_SMALL;
                }
                if (Oip->UserBuffer == NULL)
                {
                    Oip->StatusInformation.Status = STATUS_INVALID_PARAMETER;
                    Oip->StatusInformation.Information = 0;
                    return STATUS_INVALID_PARAMETER;
                }

                RtlCopyMemory(
                    Oip->UserBuffer,
                    &FdoExt->DiskNumber,
                    sizeof(FdoExt->DiskNumber)
                );
                Oip->StatusInformation.Status = STATUS_SUCCESS;
                Oip->StatusInformation.Information = sizeof(FdoExt->DiskNumber);
                return STATUS_SUCCESS;
            }
            else if (OipStack->Parameters.DeviceControl.IoControlCode == 
                IOCTL_DISK_GET_DISK_STYLE)
            {
                return DskQueryDiskStyle(
                    FdoExt,
                    Oip,
                    OipStack
                );
            }

            ObioSkipCurrentOipStackLocation(Oip);
            return ObioGoInterruptObject(FdoExt->LowerDeviceObject, Oip);

        case OIP_MJ_READ:
        case OIP_MJ_WRITE:
        case OIP_MJ_INTERNAL_DEVICE_CTL:
        case OIP_MJ_FLUSH_BUFFERS:
        case OIP_MJ_POWER:
        case OIP_MJ_SHUTDOWN:
            /* Pass-through langsung ke controller driver di bawahnya (NVMe / AHCI) */
            ObioSkipCurrentOipStackLocation(Oip);
            return ObioGoInterruptObject(FdoExt->LowerDeviceObject, Oip);

        case OIP_MJ_CREATE:
        case OIP_MJ_CLOSE:
        case OIP_MJ_CLEANUP:
            /* Akses handle langsung ke raw drive \Device\HarddiskN\DRN */
            Oip->StatusInformation.Status = STATUS_SUCCESS;
            ObioCompleteRequest(Oip);
            return STATUS_SUCCESS;

        case OIP_MJ_PNP:
            return DskMjPnp(
                Fdo,
                Oip,
                OipStack
            );

        default:
            ObioSkipCurrentOipStackLocation(Oip);
            return ObioGoInterruptObject(FdoExt->LowerDeviceObject, Oip);
    }

    D_WARNING("Unhandled OIP_MJ: 0x%x and OIP_MN: 0x%x\n", OipStack->MajorFunction, OipStack->MinorFunction);
    return STATUS_NOT_SUPPORTED;
}

VEASTATUS
VEAPI
DskObIntHandler(
    IN PVOID Object,
    IN POIP Oip
)
{
    PDEVICE_OBJECT Fdo = (PDEVICE_OBJECT)Object;
    POIP_STACK OipStack = ObioGetCurrentOipStackLocation(Oip);

    DPRINT("DskObIntHandler()\n");

    return DskFdoHandler(
        Fdo,
        Oip,
        OipStack
    );
}

VEASTATUS
VEAPI
DskAddDevice(IN PDRIVER_OBJECT DriverObject, IN PDEVICE_OBJECT Pdo)
{
    VEASTATUS Status;
    PDEVICE_OBJECT Fdo = NULL;
    PDISKA_FDO_EXTENSION FdoExt = NULL;
    ULONG DiskNumber;
    CHAR HarddiskPath[64];
    OBJECT_ATTRIBUTES ObjectAttr;
    ANSI_STRING HarddiskA;

    if (!DriverObject || !Pdo)
    {
        return STATUS_INVALID_PARAMETER;
    }

    DiskNumber = DskGetNextDiskNumber();
    Status = RtlStringCbPrintfA(
        HarddiskPath,
        sizeof(HarddiskPath),
        "/Device/Harddisk%u",
        DiskNumber
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    RtlInitAnsiString(&HarddiskA, HarddiskPath);

    InitializeObjectAttributes(&ObjectAttr, &HarddiskA, OBJ_PERMANENT, NULL, NULL);
    Status = VeaCreateDirectoryNamespace(NULL, 0x10000, &ObjectAttr);
    if (!VEA_SUCCESS(Status))
    {
        D_ERROR("Failed to create /Device/Harddisk%u: 0x%x", DiskNumber, Status);
        return Status;
    }

    RtlStringCbPrintfA(
        HarddiskPath,
        sizeof(HarddiskPath),
        "Harddisk%u/DR0",
        DiskNumber
    );

    Status = ObioCreateNewDevice(
        DriverObject,
        sizeof(DISKA_FDO_EXTENSION),
        HarddiskPath,
        FILE_DEVICE_DISK,
        &Fdo
    );
    if (!VEA_SUCCESS(Status))
    {
        D_ERROR("Failed to create FDO: 0x%x\n", Status);
        return Status;
    }

    FdoExt = (PDISKA_FDO_EXTENSION)Fdo->DeviceExtension;
    FdoExt->Self = Fdo;
    FdoExt->PhysicalDeviceObject = Pdo;
    FdoExt->IsStarted = FALSE;
    FdoExt->DiskNumber = DiskNumber;
    FdoExt->LowerDeviceObject = ObioAttachObject(Fdo, Pdo);
    if (!FdoExt->LowerDeviceObject)
    {
        D_ERROR("Failed to attach object to stack\n");
        ObioDeleteDevice(Fdo);
        return STATUS_OBJECT_ATTACH_FAIL;
    }

    Fdo->Flags |= FdoExt->LowerDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO);

    Fdo->Flags &= ~DO_DEVICE_INITIALIZING;

    ULONG StrLength = RtlRawStringLength(HarddiskPath);
    FdoExt->DeviceLink.Buffer = UlAllocatePoolZero(NonPagedPool, StrLength + 1, TAG_OB);
    if (FdoExt->DeviceLink.Buffer)
    {
        FdoExt->DeviceLink.Length = StrLength;
        FdoExt->DeviceLink.MaximumLength = StrLength + 1;

        RtlCopyMemory(FdoExt->DeviceLink.Buffer, HarddiskPath, StrLength);
    }
    else
    {
        D_ERROR("Failed to allocate memory for DeviceLink\n");
        return STATUS_INSUFFICIENT_MEMORY;
    }

    D_INFO("Successfully add device diska.ko for %p\n", Pdo);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
DriverEntry(IN PDRIVER_OBJECT DriverObject)
{
    if (!DriverObject)
    {
        return STATUS_INVALID_PARAMETER;
    }

    /* Driver image BSS is not guaranteed to be zeroed by the current loader. */
    DskNextDiskNum = 0;
    DriverObject->AddDeviceRoutine = DskAddDevice;

    return ObioAttachInterruptToThisObject(DriverObject, DskObIntHandler);
}
