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

VEASTATUS
VEAPI
LdrGetProcedureAddress(
    IN PVOID BaseAddress,
    IN PANSI_STRING Name OPTIONAL,
    IN ULONG Ordinal OPTIONAL,
    OUT PVOID *ProcedureAddress
)
{
    ULONG_PTR SymAddr;

    if (!BaseAddress || !ProcedureAddress)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *ProcedureAddress = NULL;

    /* ELF hanya mendukung lookup by name, tolak lookup by ordinal */
    if (!Name || !Name->Buffer || Name->Length == 0 || Ordinal != 0)
    {
        return STATUS_PROCEDURE_NOT_FOUND;
    }

    /* Pastikan BaseAddress benar-benar menunjuk ke ELF32 header yang valid di RAM */
    if (!RtlValidateElfHeader(BaseAddress, sizeof(ELF32_EHDR)))
    {
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    SymAddr = RtlGetElfSymbolAddress(
        (PELF32_EHDR)BaseAddress,
        (ULONG_PTR)BaseAddress,
        Name->Buffer
    );

    if (!SymAddr)
    {
        return STATUS_PROCEDURE_NOT_FOUND;
    }

    *ProcedureAddress = (PVOID)SymAddr;
    return STATUS_SUCCESS;
}

/* EOF */