/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : intelhdamain.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Intel HDA sound
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veaddk.h>
#include "hdaheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 02-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file hdamain.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
HDAOipDispatch(
    IN PVOID Object,
    IN POIP Oip
)
{
    POIP_STACK Stack = ObioGetCurrentOipStackLocation(Oip);
    PDEVICE_OBJECT DeviceObject = (PDEVICE_OBJECT)Object;
    PHDA_DEVICE_EXTENSION DevExt = (PHDA_DEVICE_EXTENSION)DeviceObject;

    switch (Stack->MajorFunction)
    {
        case OIP_MJ_PNP:
        {
            return HDAOipMjPnp(
                Object,
                Oip,
                Stack
            );
            break;
        }

        case OIP_MJ_READ:
        {
            break;
        }

        case OIP_MJ_WRITE:
        {
            return HDAOipMjWrite(
                Object,
                Oip,
                Stack
            );
            break;
        }

        case OIP_MJ_INTERNAL_DEVICE_CTL:
        {
            return HDAOipMjIOCTL(
                Object,
                Oip,
                Stack
            );
            break;
        }

        case OIP_MJ_DEVICE_CONTROL:
        {
            return HDAOipMjDevCtl(
                Object,
                Oip,
                Stack
            );
        }

        case OIP_MJ_POWER:
        {
            return HDAOipMjPower(
                Object,
                Oip,
                Stack
            );
            break;
        }
    }

    Oip->StatusInformation.Status = STATUS_NOT_SUPPORTED;
    return Oip->StatusInformation.Status;
}

VEASTATUS
VEAPI
HDAAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
)
{
    PDEVICE_OBJECT Fdo = NULL;
    PHDA_DEVICE_EXTENSION DevExt = NULL;
    VEASTATUS Status;

    DPRINT("HDA: AddDevice called for PDO %p\n", PhysicalDeviceObject);

    Status = ObioCreateNewDevice(
        DriverObject,
        sizeof(HDA_DEVICE_EXTENSION),
        NULL,                   // Sound device gak perlu nama publik (pake Interface)
        FILE_DEVICE_SOUND,      // Device Type
        &Fdo
    );

    if (!VEA_SUCCESS(Status)) {
        return Status;
    }

    DevExt = (PHDA_DEVICE_EXTENSION)Fdo->DeviceExtension;
    RtlZeroMemory(DevExt, sizeof(HDA_DEVICE_EXTENSION));

    KsInitializeSpinlock(&DevExt->ControllerLock);
    DevExt->PhysicalDeviceObject = PhysicalDeviceObject;
    DevExt->LowerDeviceObject = ObioAttachObject(Fdo, PhysicalDeviceObject);
    if (!DevExt->LowerDeviceObject) {
        ObioDeleteDevice(Fdo);
        return STATUS_UNSUCCESSFUL;
    }

    Fdo->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
DriverEntry(IN PVOID Object)
{
    PDRIVER_OBJECT DriverObject = (PDRIVER_OBJECT)Object;

    DPRINT("Intel HDA DriverEntry\n");
    DriverObject->AddDeviceRoutine = HDAAddDevice;

    return ObioAttachInterruptToThisObject(DriverObject, HDAOipDispatch);
}