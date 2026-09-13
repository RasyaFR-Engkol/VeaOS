/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_pnp_driver.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : As PNP Drive
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 02-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_pnp_driver.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
ObioPnpRootInterruptHandler(
    PVOID Object,
    POIP Oip
)
{
    POIP_STACK Stack = Oip->CurrentStackLocation;

    switch (Stack->MajorFunction)
    {
        case OIP_MJ_CREATE:
        case OIP_MJ_CLOSE:
            Oip->IoStatus = STATUS_SUCCESS;
            break;

        case OIP_MJ_PNP:
            // TODO: handle minor function PnP (start device, query resource, dst)
            // via Stack->Parameters / Stack->MinorFunction kalau lu punya field itu
            Oip->IoStatus = STATUS_SUCCESS;
            break;

        default:
            Oip->IoStatus = STATUS_NOT_SUPPORTED;
            break;
    }

    ObioCompleteRequest(Oip);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObioPnpRootDriverEntry(PDRIVER_OBJECT DriverObject)
{
    if (!DriverObject) return STATUS_INVALID_PARAMETER;

    // Daftarin handler ke DRIVER_OBJECT ini -- ini setara "MajorFunction[]" NT,
    // cuma bentuknya 1 fungsi + switch internal, bukan array dispatch table
    return ObioAttachInterruptToThisObject(DriverObject, ObioPnpRootInterruptHandler);
}