/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : utility.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : NVMe Utility
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "nvmeheader.h"
#include <mm.h>

/* Revision History ------------------------------------------------------
 * DATE       : 10-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file utility.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
NVMeBuildPrp(
    IN PNVME_DEVICE_EXTENSION DevExt,
    IN PVOID Buffer,
    IN PMDL Mdl OPTIONAL,
    IN ULONG Length,
    OUT PULONGLONG Prp1,
    OUT PULONGLONG Prp2,
    OUT PVOID *PrpListVa
)
{
    PHYSICAL_ADDRESS PhysAddr;
    ULONG PageOffset, PageCount, i, MdlPageCount;
    ULONG_PTR PageVa;
    ULONGLONG PagePhysical;
    PULONGLONG PrpList;
    PHYSICAL_ADDRESS PrpListPhys;
    PHYSICAL_ADDRESS HighestPhys;

    UNREFERENCED_PARAMETER(DevExt);

    if (!Buffer || !Prp1 || !Prp2 || !PrpListVa || Length == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *Prp1 = 0;
    *Prp2 = 0;
    *PrpListVa = NULL;

    if (Mdl)
    {
        if (Mdl->Size < sizeof(MDL) ||
            Mdl->ByteOffset >= PAGE_SIZE ||
            Mdl->ByteCount < Length ||
            Buffer != (PUCHAR)Mdl->StartVa + Mdl->ByteOffset)
        {
            return STATUS_INVALID_PARAMETER;
        }

        PageOffset = Mdl->ByteOffset;
        MdlPageCount = (Mdl->Size - sizeof(MDL)) / sizeof(PFN_NUMBER);
    }
    else
    {
        PhysAddr = MmGetPhysicalAddress(Buffer);
        PageOffset = PhysAddr.LowPart & (PAGE_SIZE - 1);
    }

    PageCount = (Length >> PAGE_SHIFT) +
                (((Length & (PAGE_SIZE - 1)) + PageOffset +
                  PAGE_SIZE - 1) >> PAGE_SHIFT);
    if (PageCount == 0 || PageCount > 513 ||
        (Mdl && PageCount > MdlPageCount))
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (Mdl)
    {
        PagePhysical = (ULONGLONG)Mdl->PfnArray[0] << PAGE_SHIFT;
        *Prp1 = PagePhysical + PageOffset;
    }
    else
    {
        PhysAddr = MmGetPhysicalAddress(Buffer);
        *Prp1 = PhysAddr.QuadPart;
    }

    if (PageCount == 1)
    {
        return STATUS_SUCCESS;
    }

    PageVa = (ULONG_PTR)PAGE_ALIGN(Buffer) + PAGE_SIZE;
    if (PageCount == 2)
    {
        if (Mdl)
        {
            *Prp2 = (ULONGLONG)Mdl->PfnArray[1] << PAGE_SHIFT;
        }
        else
        {
            PhysAddr = MmGetPhysicalAddress((PVOID)PageVa);
            *Prp2 = PhysAddr.QuadPart & ~((ULONGLONG)PAGE_SIZE - 1);
        }
        return STATUS_SUCCESS;
    }

    HighestPhys.QuadPart = 0xFFFFFFFFULL;
    PrpList = (PULONGLONG)MmAllocateContiguousMemory(PAGE_SIZE, HighestPhys);
    if (!PrpList)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }
    RtlZeroMemory(PrpList, PAGE_SIZE);

    for (i = 1; i < PageCount; i++)
    {
        if (Mdl)
        {
            PagePhysical = (ULONGLONG)Mdl->PfnArray[i] << PAGE_SHIFT;
        }
        else
        {
            PhysAddr = MmGetPhysicalAddress((PVOID)PageVa);
            PagePhysical = PhysAddr.QuadPart & ~((ULONGLONG)PAGE_SIZE - 1);
            PageVa += PAGE_SIZE;
        }

        PrpList[i - 1] = PagePhysical;
    }

    PrpListPhys = MmGetPhysicalAddress(PrpList);
    *Prp2 = PrpListPhys.QuadPart;
    *PrpListVa = PrpList;

    return STATUS_SUCCESS;
}