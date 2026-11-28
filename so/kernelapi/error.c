/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : error.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : KernelAPI error
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 28-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file error.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
SetLastError(
    IN DWORD dwErrCode
)
{
    RtlSetLastVeapiError(dwErrCode);
}

DWORD
VEAPI
GetLastError(VOID)
{
    return RtlGetLastVeapiError();
}

ULONG
VEAPI
BaseSetLastVeaError(
    IN VEASTATUS Status
)
{
    ULONG ErrorCode = RtlVeaStatusToUserError(Status);
    SetLastError(ErrorCode);
    return ErrorCode;
}

/* EOF */