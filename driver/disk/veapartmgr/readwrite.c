/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : readwrite.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Pm Read Write
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "veapm.h"

/* Revision History ------------------------------------------------------
 * DATE       : 12-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file readwrite.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
PmpAskDiskGeometry(
    IN PPM_DEVICE_EXTENSION DevExt
)
{
    PDEVICE_OBJECT Target;
    POIP Oip;
    POIP_STACK Stack;
    DISK_GEOMETRY Geometry;
    OIP_INFORMATION IoStatus;
    KEVENT Event;
    VEASTATUS Status;

    if (DevExt == NULL || DevExt->LowerDeviceObject == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    Target = DevExt->LowerDeviceObject;
    RtlZeroMemory(&Geometry, sizeof(Geometry));
    RtlZeroMemory(&IoStatus, sizeof(IoStatus));

    Oip = ObioAllocateOip(1);
    if (Oip == NULL) {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    KsInitializeEvent(&Event, NotificationEvent, FALSE);
    Oip->UserBuffer = &Geometry;
    Oip->AssociatedOip.SystemBuffer = &Geometry;
    Oip->UserEvent = &Event;
    Oip->UserInfoBlock = &IoStatus;

    Stack = ObioGetNextOipStackLocation(Oip);
    Stack->MajorFunction = OIP_MJ_DEVICE_CONTROL;
    Stack->TargetObject = Target;
    Stack->Parameters.DeviceControl.IoControlCode =
        IOCTL_DISK_GET_DRIVE_GEOMETRY;
    Stack->Parameters.DeviceControl.InputBufferLength = 0;
    Stack->Parameters.DeviceControl.OutputBufferLength = sizeof(Geometry);

    Status = ObioGoInterruptObject(Target, Oip);
    if (Status == STATUS_PENDING) {
        KsWaitForSingleObject(&Event);
        Status = IoStatus.Status;
    } else {
        IoStatus = Oip->StatusInformation;
    }

    ObioFreeOip(Oip);

    if (!VEA_SUCCESS(Status)) {
        return Status;
    }

    if (IoStatus.Information < sizeof(Geometry) ||
        Geometry.BytesPerSector == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    DevExt->BytesPerSector = Geometry.BytesPerSector;
    DevExt->DiskGeometry = Geometry;
    PM_INFO("Disk geometry: sector size %u bytes\n",
            DevExt->BytesPerSector);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
PmpAskHarddiskNumber(
    IN PPM_DEVICE_EXTENSION DevExt
)
{
    PDEVICE_OBJECT Target;
    POIP Oip;
    ULONG HarddiskNumber = 0;
    OIP_INFORMATION IoStatus;
    KEVENT Event;
    VEASTATUS Status;
    BOOLEAN RequestPending;

    if (DevExt == NULL || DevExt->LowerDeviceObject == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Target = DevExt->LowerDeviceObject;
    RtlZeroMemory(&IoStatus, sizeof(IoStatus));
    KsInitializeEvent(&Event, NotificationEvent, FALSE);

    Oip = ObioBuildDeviceIoControlRequest(
        IOCTL_DISK_GET_HARDDISK_NUMBER,
        Target,
        NULL,
        0,
        &HarddiskNumber,
        sizeof(HarddiskNumber),
        FALSE,
        &Event,
        &IoStatus
    );
    if (Oip == NULL)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    Status = ObioGoInterruptObject(Target, Oip);
    RequestPending = (Status == STATUS_PENDING);
    if (RequestPending)
    {
        KsWaitForSingleObject(&Event);
        Status = IoStatus.Status;
    }
    else
    {
        IoStatus = Oip->StatusInformation;
        ObioFreeOip(Oip);
    }

    if (!VEA_SUCCESS(Status))
    {
        PM_ERROR("Harddisk-number query failed: 0x%x\n", Status);
        return Status;
    }

    if (IoStatus.Information < sizeof(HarddiskNumber))
    {
        PM_ERROR("Harddisk-number query returned %u bytes; expected %u\n",
                 (ULONG)IoStatus.Information,
                 (ULONG)sizeof(HarddiskNumber));
        return STATUS_INVALID_PARAMETER;
    }

    DevExt->HarddiskNum = HarddiskNumber;
    PM_INFO("Disk stack reports Harddisk%u\n", HarddiskNumber);
    return STATUS_SUCCESS;
}

VEASTATUS VEAPI
PmpReadSectors(
    IN PPM_DEVICE_EXTENSION DevExt,
    IN ULONGLONG StartLba,
    IN ULONG SectorCount,
    OUT PVOID Buffer)              /* nonpaged, >= SectorCount * BytesPerSector */
{
    PDEVICE_OBJECT Target = DevExt->LowerDeviceObject;
    ULONG Length = SectorCount * DevExt->BytesPerSector;
    LARGE_INTEGER Offset;
    KEVENT Event;
    OIP_INFORMATION Iosb;
    POIP Oip;
    VEASTATUS Status;

    if (!DevExt->BytesPerSector || !SectorCount)
        return STATUS_INVALID_PARAMETER;

    Offset.QuadPart = StartLba * DevExt->BytesPerSector;
    KsInitializeEvent(&Event, NotificationEvent, FALSE);
    RtlZeroMemory(&Iosb, sizeof(Iosb));

    Oip = ObioBuildSynchronusFsdRequest(
        OIP_MJ_READ, Target, Buffer, Length, &Offset, &Event, &Iosb);
    if (!Oip)
        return STATUS_NO_RESOURCES;

    PM_INFO("Calling object %p\n", Target);

    Status = ObioGoInterruptObject(Target, Oip);
    if (Status == STATUS_PENDING)
    {
        PM_INFO("Calling object pending\n");
        KsWaitForSingleObject(&Event);     /* TODO: timeout */
        Status = Iosb.Status;              /* hasil sebenarnya */
    }

    PM_INFO("Calling object success\n");

    if (VEA_SUCCESS(Status) && Iosb.Information != Length)
    {
        PM_ERROR("Read success but Partial Read\n");
        Status = STATUS_UNSUCCESSFUL;
    }

    return Status;
}

