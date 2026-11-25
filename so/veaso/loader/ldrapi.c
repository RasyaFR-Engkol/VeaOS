/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : ldrapi.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Ldr Module
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>
#include <psdk/resource.h>

/* Revision History ------------------------------------------------------
 * DATE       : 24-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file ldrapi.c
 * --------------------------------------------------------------------- */

PVOID
VEAPI
LdrGetProcedureAddress(
    IN PVOID BaseAddress,
    IN PCSTR ProcedureName
)
{
    /* Tolak handle NULL atau lookup by ordinal (ELF hanya mendukung nama simbol) */
    if (!BaseAddress || !ProcedureName || IS_INTRESOURCE(ProcedureName))
    {
        return NULL;
    }

    /* Pastikan BaseAddress benar-benar menunjuk ke ELF32 header yang valid di RAM */
    if (!RtlValidateElfHeader(BaseAddress, sizeof(ELF32_EHDR)))
    {
        return NULL;
    }

    ULONG_PTR SymAddr = RtlGetElfSymbolAddress(
        (PELF32_EHDR)BaseAddress,
        (ULONG_PTR)BaseAddress,
        ProcedureName
    );

    return (PVOID)SymAddr;
}

/* EOF */