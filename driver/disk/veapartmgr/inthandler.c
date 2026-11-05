/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : inthandler.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Pm Int Handler
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "veapm.h"

/* Revision History ------------------------------------------------------
 * DATE       : 16-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file inthandler.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
PmPartitionInt(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack,
    IN PPM_DEVICE_EXTENSION DeviceExtension
)
{
    switch (OipStack->MajorFunction)
    {
        case OIP_MJ_READ:
        case OIP_MJ_WRITE:
        {
            LARGE_INTEGER ByteToRead;
            VEASTATUS Status;

            ByteToRead = OipStack->Parameters.ReadWrite.Offset;
            // DPRINT("[PM] Incoming Offset: 0x%x HIGH 0x%x\n", ByteToRead.LowPart, ByteToRead.HighPart);

            // 2. TAMBAHKAN STARTING OFFSET PARTISI
            OipStack->Parameters.ReadWrite.Offset.QuadPart += DeviceExtension->StartingOffset;

            ByteToRead = OipStack->Parameters.ReadWrite.Offset;
            // DPRINT("[PM] Translated Offset: 0x%x HIGH 0x%x\n", ByteToRead.LowPart, ByteToRead.HighPart);

            // 4. TERUSKAN KE LOWER DRIVER (AHCI) DENGAN MENGKOPI STACK
            POIP_STACK NextStack = ObioGetNextOipStackLocation(Oip);
            if (NextStack)
            {
                RtlCopyMemory(NextStack, OipStack, sizeof(OIP_STACK));
                return ObioGoInterruptObject(DeviceExtension->LowerDeviceObject, Oip);
            }

            Oip->StatusInformation.Status = STATUS_INVALID_DEVICE_REQUEST;
            return STATUS_INVALID_DEVICE_REQUEST;
        }

        case OIP_MJ_DEVICE_CONTROL:
        {
            DPRINT("Device Control\n");
            return PmDeviceControl(Oip, OipStack, DeviceExtension);
        }

        case OIP_MJ_PNP:
        {
            /* Handle PnP request untuk PDO partisi */
            break;
        }

        ObioSkipCurrentOipStackLocation(Oip);
        return ObioGoInterruptObject(DeviceExtension->LowerDeviceObject, Oip);
    }

    DPRINT("Invalid device request: 0x%x Minor 0x%x\n", OipStack->MajorFunction, OipStack->MinorFunction);
    Oip->StatusInformation.Status = STATUS_INVALID_DEVICE_REQUEST;
    return STATUS_INVALID_DEVICE_REQUEST;
}