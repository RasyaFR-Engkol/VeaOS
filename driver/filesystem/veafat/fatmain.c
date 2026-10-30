/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : fatmain.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PURPOSE
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "veafat.h"

/* Revision History ------------------------------------------------------
 * DATE       : 15-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file fatmain.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
FatObIntHandler(
    IN PVOID Object,
    IN POIP Oip
)
{
    POIP_STACK Stack = ObioGetCurrentOipStackLocation(Oip);

    switch (Stack->MajorFunction)
    {
        case OIP_MJ_READ:
            break;

        case OIP_MJ_WRITE:
            break;

        case OIP_MJ_FILE_SYSTEM_CONTROL:
            return FatMjFsCtl((PDEVICE_OBJECT)Object, Oip, Stack);

        case OIP_MJ_CREATE:
            return FatMjCreate((PDEVICE_OBJECT)Object, Oip, Stack);
            break;

        case OIP_MJ_CLOSE:
            break;

        case OIP_MJ_CLEANUP:
            break;

        case OIP_MJ_DEVICE_CONTROL:
            break;

        case OIP_MJ_INTERNAL_DEVICE_CTL:
            break;
        
        case OIP_MJ_FLUSH_BUFFERS:
            break;

        case OIP_MJ_PNP:
            break;

        case OIP_MJ_QUERY_INFORMATION:
            break;

        case OIP_MJ_SET_INFORMATION:
            break;

        case OIP_MJ_SHUTDOWN:
            break;

        case OIP_MJ_DIRECTORY_CONTROL:
            break;
    }

    DPRINT("STATUS NOT IMPLEMENTED: 0x%x Minor 0x%x\n", Stack->MajorFunction, Stack->MinorFunction);
    Oip->StatusInformation.Status = STATUS_NOT_IMPLEMENTED;
    return STATUS_NOT_IMPLEMENTED;
}

VEASTATUS
VEAPI
DriverEntry(IN PDRIVER_OBJECT DriverObject)
{
    VEASTATUS Status;
    PDEVICE_OBJECT Cdo;
    PFAT32_DEVICE_EXTENSION DevExt;

    /* Attach Interrupt to this object */
    Status = ObioAttachInterruptToThisObject(DriverObject, FatObIntHandler);
    if (!VEA_SUCCESS(Status)) 
    {
        DPRINT("Fat failed to attach interrupt: 0x%x\n", Status);
        return Status;
    }

    /* Create new CDO */
    Status = ObioCreateNewDevice(
        DriverObject,
        sizeof (FAT32_DEVICE_EXTENSION),
        "/FileSystem/VeaFat",
        FILE_DEVICE_DISK_FILE_SYSTEM,
        &Cdo
    );
    if (!VEA_SUCCESS(Status)) 
    {
        DPRINT("Fat failed to create CDO: 0x%x\n", Status);
        return Status;
    }

    /* Setup Device Extension */
    DevExt = (PFAT32_DEVICE_EXTENSION)Cdo->DeviceExtension;
    DevExt->IsCdo = TRUE;
    DevExt->VolumeDeviceObject = Cdo;
    DevExt->Vpb = NULL;
    DevExt->TargetDeviceObject = NULL;

    Cdo->Flags &= ~DO_DEVICE_INITIALIZING;

    /* Register to file system */
    ObioRegisterFileSystem(Cdo);

    return STATUS_SUCCESS;
}