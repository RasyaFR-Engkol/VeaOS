/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : rmextendsys.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Extending Syscall
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 20-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file rmextendsys.c
 * --------------------------------------------------------------------- */

BOOLEAN
VEAPI
RmAddSystemServiceTable(
    IN PULONG_PTR Base,
    IN PULONG Count OPTIONAL,
    IN ULONG Limit,
    IN PUCHAR Number,
    IN ULONG Index
)
{
    if ((Index > SSDT_MAX_ENTRIES - 1) || 
        (KsServiceTableDescriptor[Index].Base) || 
        (KsServiceTableDescriptorShadow[Index].Base))
    {
        return FALSE;
    }

    KsServiceTableDescriptorShadow[Index].Base = Base;
    KsServiceTableDescriptorShadow[Index].Limit = Limit;
    KsServiceTableDescriptorShadow[Index].Number = Number;
    KsServiceTableDescriptorShadow[Index].Count = Count;
    return TRUE;
}