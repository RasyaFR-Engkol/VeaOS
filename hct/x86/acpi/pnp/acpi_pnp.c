/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : acpi_pnp.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : ACPI_PNP
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 06-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file acpi_pnp.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
AcpiPnpInterruptHandler(
    IN PVOID Object,
    IN POIP OIP
)
{
    switch(OIP->CurrentStackLocation->MajorFunction)
    {
        case OIP_MJ_PNP:
        {

        }

        case OIP_MJ_POWER:
        {
            
        }
    }
}

VEASTATUS
VEAPI
AcpiPnpDriverEntry(IN PDRIVER_OBJECT Object)
{
    if(!Object) return STATUS_INVALID_PARAMETER;

    return ObioAttachInterruptToThisObject((PVOID)Object, AcpiPnpInterruptHandler);
}