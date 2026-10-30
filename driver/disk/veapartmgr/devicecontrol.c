/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : devicecontrol.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : PURPOSE
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "veapm.h"

/* Revision History ------------------------------------------------------
 * DATE       : 17-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file devicecontrol.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
PmDeviceControl(
    IN POIP Oip,
    IN POIP_STACK OipStack,
    IN PPM_DEVICE_EXTENSION DevExt
)
{
    ULONG IoControlCode = OipStack->Parameters.DeviceControl.IoControlCode;
    ULONG OutputLength = OipStack->Parameters.DeviceControl.OutputBufferLength;

    switch (IoControlCode)
    {
        case IOCTL_DISK_GET_DRIVE_GEOMETRY:
        {
            DPRINT("IOCTL_DISK_GET_DRIVE_GEOMETRY\n");
            if (OutputLength < sizeof(DISK_GEOMETRY))
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

            // Copy cached DISK_GEOMETRY ke UserBuffer milik caller (VeaFat)
            RtlCopyMemory(Oip->UserBuffer, &DevExt->DiskGeometry, sizeof(DISK_GEOMETRY));

            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = sizeof(DISK_GEOMETRY);
            return STATUS_SUCCESS;
        }

        default:
        {
            // Kalau ada IOCTL lain yang gak di-handle PDO, lempar ke lower device (NVMe)
            ObioSkipCurrentOipStackLocation(Oip);
            return ObioCallObject(DevExt->LowerDeviceObject, Oip);
        }
    }
}