/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : ptmgr.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Process Thread Manager
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 02-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file ptmgr.c
 * --------------------------------------------------------------------- */

ANSI_STRING PtVeaSoName = RTL_CONSTANT_ANSI_STRING("/SystemRoot/SystemReserved/veaso.so");

PVOID PtpSystemSoBase;
PVOID PtpSystemSoSection;

VEASTATUS
VEAPI
PtpMapSystemSo(
    IN PEPROCESS Process,
    IN PVOID *DllBase,
    IN BOOLEAN UseLargePages
)
{
    VEASTATUS Status;
    LARGE_INTEGER Offset = {{0,0}};
    SIZE_T ViewSize = 0;
    PVOID ImageBase = 0;

    Status = MmMapViewOfSection(
        PtpSystemSoSection,
        Process,
        (PVOID*)&ImageBase,
        0,
        0,
        &Offset,
        &ViewSize,
        ViewShare,
        0,
        PAGE_READWRITE
    );
    if (!VEA_SUCCESS(Status))
    {
        Status = STATUS_CONFLICTING_ADDRESSES;
    }

    if (DllBase)
    {
        *DllBase = ImageBase;
    }

    return Status;
}

VEASTATUS
VEAPI
PtLocateSystemSo(VOID)
{
    OBJECT_ATTRIBUTES Attributes;
    HANDLE FileHandle, SectionHandle;
    VEASTATUS Status;
    OIP_INFORMATION InformationBlock;

    InitializeObjectAttributes(
        &Attributes,
        &PtVeaSoName,
        0,
        NULL,
        NULL
    );
    
    /* Open VeaSo file */
    Status = VeaOpenFile(
        &FileHandle,
        FILE_READ_ACCESS,
        &Attributes,
        &InformationBlock,
        FILE_SHARE_READ,
        0
    );
    if (!VEA_SUCCESS(Status))
    {
        /* Is that mean no File? */
        KsBugCheckEx(PROCESSTHREAD_INITIALIZATION_FAILED, Status, 0, 0, 0);
    }

    Status = MmCheckSystemImage(FileHandle);
    if (Status == STATUS_IMAGE_CHECKSUM_MISMATCH || Status == STATUS_INVALID_IMAGE_FORMAT || Status == STATUS_INVALID_IMAGE_PROTECT)
    {
        KsBugCheckEx(PROCESSTHREAD_INITIALIZATION_FAILED, Status, 1, 0, 0);
    }

    Status = VeaCreateSection(
        &SectionHandle,
        SECTION_ALL_ACCESS,
        NULL,
        NULL,
        PAGE_EXECUTE,
        SEC_IMAGE,
        FileHandle
    );
    VeaClose(FileHandle);
    if (!VEA_SUCCESS(Status))
    {
        KsBugCheckEx(PROCESSTHREAD_INITIALIZATION_FAILED, Status, 2, 0, 0);
    }

    Status = ObReferenceObjectByHandle(
        SectionHandle,
        SECTION_ALL_ACCESS,
        MmSectionObjectType,
        KernelMode,
        (PVOID*)&PtpSystemSoSection
    );
    VeaClose(SectionHandle);
    if (!VEA_SUCCESS(Status))
    {
        KsBugCheckEx(PROCESSTHREAD_INITIALIZATION_FAILED, Status, 3, 0, 0);
    }

    Status = PtpMapSystemSo((PEPROCESS)PtGetCurrentProcess(), &PtpSystemSoBase, FALSE);
    if (!VEA_SUCCESS(Status))
    {
        KsBugCheckEx(PROCESSTHREAD_INITIALIZATION_FAILED, Status, 4, 0, 0);
    }

    return Status;
}

/* EOF */