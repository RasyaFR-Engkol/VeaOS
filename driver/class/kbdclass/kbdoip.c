/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : kbdoip.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Handling KBD OIP
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "kbdheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 17-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file kbdoip.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
KbdFdoHandle(
    IN PDEVICE_OBJECT Fdo,
    IN POIP Oip
)
{
    PKBD_DEVICE_EXTENSION DevExt = (PKBD_DEVICE_EXTENSION)Fdo->DeviceExtension;
    POIP_STACK Stack = ObioGetCurrentOipStackLocation(Oip);

    switch (Stack->MajorFunction)
    {
        // 
        // Unsure with this Major Function 
        // 

        case OIP_MJ_READ:
        case OIP_MJ_WRITE:
        case OIP_MJ_CREATE:
        case OIP_MJ_CLOSE:
        case OIP_MJ_CLEANUP:
        case OIP_MJ_PNP:
            goto Unimplemented;

        default:
            goto RetToNextDevice;
    }

RetToNextDevice:
    if (DevExt->FdoData.LowerDevice) 
    {
        ObioSkipCurrentOipStackLocation(Oip);
        return ObioCallObject(DevExt->FdoData.LowerDevice, Oip);
    }

Unimplemented:
    Oip->StatusInformation.Status = STATUS_NOT_IMPLEMENTED;
    ObioCompleteRequest(Oip);
    return STATUS_NOT_IMPLEMENTED;
}


VEASTATUS
VEAPI
KbdPdoHandle(
    IN PDEVICE_OBJECT Pdo,
    IN POIP Oip
)
{
    PKBD_DEVICE_EXTENSION DevExt = (PKBD_DEVICE_EXTENSION)Pdo->DeviceExtension;
    POIP_STACK Stack = ObioGetCurrentOipStackLocation(Oip);
    VEASTATUS Status = STATUS_SUCCESS;

    switch (Stack->MajorFunction)
    {
        case OIP_MJ_CREATE:
        case OIP_MJ_CLOSE:
            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = 0;
            ObioCompleteRequest(Oip);
            return STATUS_SUCCESS;

        case OIP_MJ_CLEANUP:
        {
            POIP Pending = (POIP)__InterlockedExchangePointer(
                (volatile void**)&DevExt->PdoData.PendingReadOip,
                NULL
            );
            if (Pending != NULL) {
                Pending->StatusInformation.Status = STATUS_CANCELLED;
                Pending->StatusInformation.Information = 0;
                ObioCompleteRequest(Pending);
            }

            Oip->StatusInformation.Status = STATUS_SUCCESS;
            Oip->StatusInformation.Information = 0;
            ObioCompleteRequest(Oip);
            return STATUS_SUCCESS;
        }

        case OIP_MJ_READ:
        {
            ULONG MaxPackets = Stack->Parameters.ReadWrite.Length / sizeof(KEYBOARD_INPUT_DATA);
            if (MaxPackets == 0 || Oip->AssociatedOip.SystemBuffer == NULL) {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            ULONG Copied = KbdPopInputData(
                DevExt,
                (PKEYBOARD_INPUT_DATA)Oip->AssociatedOip.SystemBuffer,
                MaxPackets
            );

            if (Copied > 0) {
                Oip->StatusInformation.Status = STATUS_SUCCESS;
                Oip->StatusInformation.Information = Copied * sizeof(KEYBOARD_INPUT_DATA);
                ObioCompleteRequest(Oip);
                return STATUS_SUCCESS;
            }

            // Buffer kosong: simpan OIP sebagai pending sampai port driver dapet scancode
            if (!__InterlockedCompareExchangePointer(
                    (volatile void **)&DevExt->PdoData.PendingReadOip,
                    Oip,
                    NULL))
            {
                DPRINT("[KBD] Busy! PendingOip=%p, Self=%p, PortDev=%p\n",
                    DevExt->PdoData.PendingReadOip,
                    DevExt->Self,
                    DevExt->PdoData.PortDevice);
                Status = STATUS_DEVICE_BUSY;
                break;
            }

            return STATUS_PENDING;
        }

        default:
            Status = STATUS_NOT_IMPLEMENTED;
            break;
    }

    Oip->StatusInformation.Status = Status;
    Oip->StatusInformation.Information = 0;
    ObioCompleteRequest(Oip);
    return Status;
}

/* EOF */