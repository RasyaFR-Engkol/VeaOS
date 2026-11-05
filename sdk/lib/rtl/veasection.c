/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : veasection.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Loaded Memory & ELF Section Helper API
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "kdp.h"
#include "mm.h"
#include <rtl.h>
#include "rtlp.h"

// Struct untuk mengembalikan batas-batas lokasi section di RAM


/**
 * @brief Mencari Section pada buffer ELF mentah (misal: Driver .ko / Modul Disk)
 */
VEASTATUS
VEAPI
RtlGetElfSectionBoundsByName(
    IN PELF32_EHDR Header,
    IN PCSTR SectionName,
    OUT PVEA_SECTION_BOUNDS Bounds
)
{
    if (!Header || !SectionName || !Bounds)
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(Bounds, sizeof(VEA_SECTION_BOUNDS));

    PELF32_SHDR TargetShdr = RtlFindElfSectionByName(Header, (PSTR)SectionName);
    if (!TargetShdr)
    {
        return STATUS_NOT_FOUND;
    }

    // Jika ELF bertipe ET_REL (.ko), offset section relatif terhadap buffer/ImageBase
    if (Header->e_type == ET_REL)
    {
        Bounds->BaseAddress = (PVOID)((ULONG_PTR)Header + TargetShdr->sh_offset);
    }
    else
    {
        Bounds->BaseAddress = (PVOID)(ULONG_PTR)TargetShdr->sh_addr;
    }

    Bounds->Size  = TargetShdr->sh_size;
    Bounds->Flags = TargetShdr->sh_flags;

    return STATUS_SUCCESS;
}

/**
 * @brief Mengecek apakah sebuah Virtual Address berada di dalam batas section tertentu
 */
BOOLEAN
VEAPI
RtlIsAddressWithinSection(
    IN PVOID Address,
    IN PVOID SectionStart,
    IN PVOID SectionEnd
)
{
    ULONG_PTR Target = (ULONG_PTR)Address;
    ULONG_PTR Start  = (ULONG_PTR)SectionStart;
    ULONG_PTR End    = (ULONG_PTR)SectionEnd;

    return (Target >= Start && Target < End);
}