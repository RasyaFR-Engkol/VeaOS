/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : proc.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Process Supporting for Mm
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 19-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file proc.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
MmInitializeProcessDirectoryBase(
    IN PEPROCESS Process
)
{
    if (!Process) 
    {
        return STATUS_INVALID_PARAMETER;
    }

    Process->Pcb.DirectoryTableBase = MmAllocatePhysicalPage();
    if (!Process->Pcb.DirectoryTableBase)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    PULONG_PTR VirtualAddress = (PULONG_PTR)MmMapIoSpace(
        Process->Pcb.DirectoryTableBase, 
        PAGE_SIZE, 
        MmCached
    );

    if (!VirtualAddress)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    RtlZeroMemory(VirtualAddress, PAGE_SIZE);

    PULONG SystemPageDir = (PULONG)MM_GET_PD();
    for (ULONG i = 768; i < 1024; i++)
    {
        VirtualAddress[i] = SystemPageDir[i];
    }

    ULONG Flags = VirtualAddress[1023] & 0xFFF;
    VirtualAddress[1023] = Process->Pcb.DirectoryTableBase | Flags;

    MmUnmapIoSpace(VirtualAddress, PAGE_SIZE);

    return STATUS_SUCCESS;
}