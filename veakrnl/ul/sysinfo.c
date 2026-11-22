/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : sysinfo.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : SYSTEM INFO QUERY
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 09-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file sysinfo.c
 * --------------------------------------------------------------------- */

#define QSI_USE(n) QSI##n
#define QSI_DEF(n) \
static VEASTATUS QSI_USE(n) (PVOID Buffer, ULONG Size, PULONG ReqSize)

#define SSI_USE(n) SSI##n
#define SSI_DEF(n) \
static VEASTATUS SSI_USE(n) (PVOID Buffer, ULONG Size)

typedef
struct _QSSI_CALLS
{
    VEASTATUS (* Query) (PVOID,ULONG,PULONG);
    VEASTATUS (* Set) (PVOID,ULONG);
} QSSI_CALLS;

//
// QS    Query & Set
// QX    Query
// XS    Set
// XX    unknown behaviour
//
#define SI_QS(n) [n] = {QSI_USE(n),SSI_USE(n)}
#define SI_QX(n) [n] = {QSI_USE(n),NULL}
#define SI_XS(n) [n] = {NULL,SSI_USE(n)}
#define SI_XX(n) [n] = {NULL,NULL}

QSI_DEF(SystemBasicInformation)
{
    PSYSTEM_BASIC_INFORMATION Sbi = (PSYSTEM_BASIC_INFORMATION)Buffer;

    *ReqSize = sizeof(SYSTEM_BASIC_INFORMATION);

    if (Size != sizeof(SYSTEM_BASIC_INFORMATION))
    {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    RtlZeroMemory(Sbi, Size);
    Sbi->Reserved = 0;
    Sbi->TimerResolution = 0; /* FIXME */
    Sbi->PageSize = PAGE_SIZE;
    Sbi->NumberOfPhysicalPages = MmNumberOfPhysicalPages;
    Sbi->LowestPhysicalPageNumber = (ULONG)0; /* FIXME */
    Sbi->HighestPhysicalPageNumber = MmHighestPhysicalPage;
    Sbi->MinimumUserModeAddress = 0x10000;
    Sbi->AllocationGranularity = _64K;
    Sbi->MaximumUserModeAddress = (ULONG_PTR)MmHighestPhysicalPage;
    Sbi->ActiveProcessorsAffinityMask = 0; /* FIXME */
    Sbi->NumberOfProcessors = 1; /* FIXME */

    return STATUS_SUCCESS;
}

SSI_DEF(SystemExtendServiceTableInformation)
{
    ANSI_STRING UserString;
    ANSI_STRING KernelString;
    PCHAR NameBuffer = NULL;
    PVOID ImageBase = NULL;
    PLOADED_MODULE_LIST_ENTRY ModuleEntry = NULL;
    PDRIVER_INITIALIZE EntryPoint = NULL;
    PKTHREAD CurrentThread;
    KPROCESSOR_MODE PreviousMode;
    VEASTATUS Status;

    if (Size != sizeof(ANSI_STRING))
    {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    RtlCopyMemory(&UserString, Buffer, sizeof(ANSI_STRING));

    if (UserString.Length == 0 || UserString.Buffer == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode == UserMode)
    {
        if ((ULONG_PTR)UserString.Buffer >= (ULONG)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)UserString.Buffer + UserString.Length) >= (ULONG)MM_HIGHEST_USER_ADDRESS)
        {
            return STATUS_ACCESS_VIOLATION;
        }
    }

    /* Salin path string ke kernel pool agar aman dipakai loader modul */
    NameBuffer = UlAllocatePoolWithTag(NonPagedPool, UserString.Length + 1, 'pxeS');
    if (!NameBuffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(NameBuffer, UserString.Buffer, UserString.Length);
    NameBuffer[UserString.Length] = '\0';

    KernelString.Buffer = NameBuffer;
    KernelString.Length = UserString.Length;
    KernelString.MaximumLength = UserString.Length + 1;

    /* Pindah sementara ke KernelMode agar OBJ_KERNEL_HANDLE valid di VeaCreateSection & VeaClose */
    CurrentThread = (PKTHREAD)PtGetCurrentThread();
    CurrentThread->PreviousMode = KernelMode;

    Status = MmLoadSystemImage(
        &KernelString,
        NULL,
        NULL,
        0,
        (PVOID *)&ModuleEntry,
        &ImageBase
    );

    CurrentThread->PreviousMode = PreviousMode;
    UlFreePool(NameBuffer);

    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* Jalankan DriverEntry milik veapi.ko */
    if (!ModuleEntry || !ModuleEntry->EntryPoint)
    {
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    EntryPoint = (PDRIVER_INITIALIZE)ModuleEntry->EntryPoint;
    Status = EntryPoint(NULL);
    DPRINT("VEAPI STATUS: 0x%x\n", Status);
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    return STATUS_SUCCESS;
}

//
// QSI Table Handler
//

static
QSSI_CALLS
CallQS[] = 
{
    SI_QX(SystemBasicInformation),
    SI_XS(SystemExtendServiceTableInformation)
};

#define CALL_QS_COUNT (sizeof(CallQS) / sizeof(CallQS[0]))

VEASTATUS
VEAPI
VeaQuerySystemInformation(
    IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
    OUT PVOID SystemInformation,
    IN ULONG SystemInformationLength,
    OUT PULONG ReturnedLength
)
{
    VEASTATUS Status = STATUS_SUCCESS;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    ULONG RequiredLength = 0;

    if ((ULONG)SystemInformationClass >= CALL_QS_COUNT)
    {
        return STATUS_INVALID_INFO_CLASS;
    }

    if (CallQS[SystemInformationClass].Query == NULL)
    {
        return STATUS_NOT_IMPLEMENTED;
    }

    if (PreviousMode == UserMode)
    {
        if (SystemInformationLength > 0)
        {
            if (!SystemInformation || 
                (ULONG_PTR)SystemInformation >= 0x80000000 ||
                ((ULONG_PTR)SystemInformation + SystemInformationLength) >= 0x80000000)
            {
                return STATUS_ACCESS_VIOLATION;
            }
        }

        if (ReturnedLength)
        {
            if ((ULONG_PTR)ReturnedLength >= 0x80000000)
            {
                return STATUS_ACCESS_VIOLATION;
            }

            _VEA_TRY
            {
                *(volatile ULONG*)ReturnedLength = *(volatile ULONG*)ReturnedLength;
            }
            _VEA_EXCEPT(1)
            {
                return STATUS_ACCESS_VIOLATION;
            }
            _VEA_END
        }
    }

    if (PreviousMode == UserMode)
    {
        _VEA_TRY
        {
            Status = CallQS[SystemInformationClass].Query(
                SystemInformation,
                SystemInformationLength,
                &RequiredLength
            );

            if (ReturnedLength)
            {
                *ReturnedLength = RequiredLength;
            }
        }
        _VEA_EXCEPT(1)
        {
            Status = STATUS_ACCESS_VIOLATION;
        }
        _VEA_END
    }
    else
    {
        Status = CallQS[SystemInformationClass].Query(
            SystemInformation,
            SystemInformationLength,
            &RequiredLength
        );

        if (ReturnedLength)
        {
            *ReturnedLength = RequiredLength;
        }
    }

    return Status;
}

VEASTATUS
VEAPI
VeaSetSystemInformation(
    IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
    IN PVOID SystemInformation,
    IN ULONG SystemInformationLength
)
{
    VEASTATUS Status = STATUS_SUCCESS;
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();

    if ((ULONG)SystemInformationClass >= CALL_QS_COUNT)
    {
        return STATUS_INVALID_INFO_CLASS;
    }

    if (CallQS[SystemInformationClass].Set == NULL)
    {
        return STATUS_NOT_IMPLEMENTED;
    }

    if (PreviousMode == UserMode)
    {
        if (SystemInformationLength > 0)
        {
            if (!SystemInformation ||
                (ULONG_PTR)SystemInformation >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
                ((ULONG_PTR)SystemInformation + SystemInformationLength) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
            {
                return STATUS_ACCESS_VIOLATION;
            }
        }

        _VEA_TRY
        {
            Status = CallQS[SystemInformationClass].Set(
                SystemInformation,
                SystemInformationLength
            );
        }
        _VEA_EXCEPT(1)
        {
            Status = STATUS_ACCESS_VIOLATION;
        }
        _VEA_END
    }
    else
    {
        Status = CallQS[SystemInformationClass].Set(
            SystemInformation,
            SystemInformationLength
        );
    }

    return Status;
}

/* EOF */