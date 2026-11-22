/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : sid.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Security Module
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <rtl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 17-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file sid.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
RtlInitializeSid(
    OUT PVEA_SID Sid,
    IN UCHAR Authority,
    IN ULONG MachineID,
    IN ULONG UserID
)
{
    if (!Sid) return STATUS_INVALID_PARAMETER;

    Sid->Revision  = VEA_SECURITY_DESCRIPTOR_REVISION;
    Sid->Authority = Authority;
    Sid->Flags     = 0;
    Sid->MachineID = MachineID;
    Sid->UserID    = UserID;

    return STATUS_SUCCESS;
}

BOOLEAN
VEAPI
RtlValidSid(
    IN PVEA_SID Sid
)
{
    if (!Sid) return FALSE;
    return (Sid->Revision == VEA_SECURITY_DESCRIPTOR_REVISION);
}

ULONG
VEAPI
RtlLengthSid(
    IN PVEA_SID Sid
)
{
    UNREFERENCED_PARAMETER(Sid);
    return sizeof(VEA_SID);
}

BOOLEAN
VEAPI
RtlEqualSid(
    IN PVEA_SID Sid1,
    IN PVEA_SID Sid2
)
{
    if (!Sid1 || !Sid2) return FALSE;
    if (Sid1->Revision  != Sid2->Revision)  return FALSE;
    if (Sid1->Authority != Sid2->Authority) return FALSE;
    if (Sid1->MachineID != Sid2->MachineID) return FALSE;
    if (Sid1->UserID    != Sid2->UserID)    return FALSE;
    return TRUE;
}

VEASTATUS
VEAPI
RtlCopySid(
    IN ULONG DestinationSidLength,
    OUT PVEA_SID DestinationSid,
    IN PVEA_SID SourceSid
)
{
    if (!DestinationSid || !RtlValidSid(SourceSid))
        return STATUS_INVALID_PARAMETER;

    if (DestinationSidLength < sizeof(VEA_SID))
        return STATUS_BUFFER_TOO_SMALL;

    RtlCopyMemory(DestinationSid, SourceSid, sizeof(VEA_SID));
    return STATUS_SUCCESS;
}

/* EOF */