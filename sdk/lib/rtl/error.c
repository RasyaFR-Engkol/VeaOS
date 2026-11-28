/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : error.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Error Implementation
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <rtl.h>
#include <usererror.h>
#include <teb.h>

/* Revision History ------------------------------------------------------
 * DATE       : 28-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file error.c
 * --------------------------------------------------------------------- */

typedef struct _VEA_ERROR_TABLE {
    ULONG Start;
    ULONG End;
    const DWORD *Table;
} VEA_ERROR_TABLE, *PVEA_ERROR_TABLE;

#define VEA_ERR_BASE_0  0xC0000001UL
#define VEA_ERR_END_0   0xC000009BUL

static const DWORD VeaStatusTable0[VEA_ERR_END_0 - VEA_ERR_BASE_0] = {
    [STATUS_UNSUCCESSFUL           - VEA_ERR_BASE_0] = ERROR_GEN_FAILURE,
    [STATUS_NOT_IMPLEMENTED        - VEA_ERR_BASE_0] = ERROR_CALL_NOT_IMPLEMENTED,
    [STATUS_INVALID_INFO_CLASS     - VEA_ERR_BASE_0] = ERROR_INVALID_PARAMETER,
    [STATUS_INFO_LENGTH_MISMATCH   - VEA_ERR_BASE_0] = ERROR_BAD_LENGTH,
    [STATUS_ACCESS_VIOLATION       - VEA_ERR_BASE_0] = ERROR_NOACCESS,
    [STATUS_INVALID_HANDLE         - VEA_ERR_BASE_0] = ERROR_INVALID_HANDLE,
    [STATUS_INVALID_PARAMETER      - VEA_ERR_BASE_0] = ERROR_INVALID_PARAMETER,
    [STATUS_NO_SUCH_DEVICE         - VEA_ERR_BASE_0] = ERROR_FILE_NOT_FOUND,
    [STATUS_NO_SUCH_FILE           - VEA_ERR_BASE_0] = ERROR_FILE_NOT_FOUND,
    [STATUS_END_OF_FILE            - VEA_ERR_BASE_0] = ERROR_HANDLE_EOF,
    [STATUS_NO_MEMORY              - VEA_ERR_BASE_0] = ERROR_NOT_ENOUGH_MEMORY,
    [STATUS_ACCESS_DENIED          - VEA_ERR_BASE_0] = ERROR_ACCESS_DENIED,
    [STATUS_BUFFER_TOO_SMALL       - VEA_ERR_BASE_0] = ERROR_INSUFFICIENT_BUFFER,
    [STATUS_OBJECT_NAME_INVALID    - VEA_ERR_BASE_0] = ERROR_INVALID_NAME,
    [STATUS_OBJECT_NAME_NOT_FOUND  - VEA_ERR_BASE_0] = ERROR_FILE_NOT_FOUND,
    [STATUS_OBJECT_NAME_COLLISION  - VEA_ERR_BASE_0] = ERROR_ALREADY_EXISTS,
    [STATUS_OBJECT_PATH_NOT_FOUND  - VEA_ERR_BASE_0] = ERROR_PATH_NOT_FOUND,
    [STATUS_SHARING_VIOLATION      - VEA_ERR_BASE_0] = ERROR_SHARING_VIOLATION,
    [STATUS_INSUFFICIENT_RESOURCES - VEA_ERR_BASE_0] = ERROR_NOT_ENOUGH_MEMORY,
};

static const VEA_ERROR_TABLE VeaErrorTable[] = {
    { VEA_ERR_BASE_0, VEA_ERR_END_0, VeaStatusTable0 },
    { 0, 0, NULL }
};

#ifdef BUILD_USER

ULONG
VEAPI
RtlVeaStatusToUserErrorNoTeb(
    IN VEASTATUS Status
)
{
    const VEA_ERROR_TABLE *Entry = VeaErrorTable;

    if (!Status || (Status & 0x20000000))
    {
        return (ULONG)Status;
    }

    /* Normalisasi severity 0xD... (warning/error) ke 0xC... */
    if ((Status & 0xF0000000) == 0xD0000000)
    {
        Status &= ~0x10000000;
    }

    while (Entry->Start)
    {
        if ((ULONG)Status < Entry->Start)
        {
            break;
        }

        if ((ULONG)Status < Entry->End)
        {
            DWORD Mapped = Entry->Table[(ULONG)Status - Entry->Start];
            if (!Mapped)
            {
                goto NoMapping;
            }
            return Mapped;
        }

        Entry++;
    }

    if (HIWORD(Status) == 0xC001 || HIWORD(Status) == 0x8007)
    {
        return LOWORD(Status);
    }

NoMapping:
    KdPrintf("[VEASO/RTL] Unmapped VEASTATUS: 0x%08X\n\r", Status);
    return ERROR_MR_MID_NOT_FOUND;
}

ULONG
VEAPI
RtlVeaStatusToUserError(
    IN VEASTATUS Status
)
{
    PTEB Teb = NtCurrentTeb();

    if (Teb != NULL)
    {
        Teb->LastStatusValue = Status;
    }

    return RtlVeaStatusToUserErrorNoTeb(Status);
}

VOID
VEAPI
RtlSetLastVeapiError(
    IN DWORD Win32Error
)
{
    NtCurrentTeb()->LastErrorValue = Win32Error;
}

DWORD
VEAPI
RtlGetLastVeapiError(VOID)
{
    return NtCurrentTeb()->LastErrorValue;
}

#endif

/* EOF */
