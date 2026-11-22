/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : acl.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Security ACL Module
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <rtl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 17-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file acl.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
RtlCreateAcl(
    OUT PVEA_ACL Acl,
    IN ULONG AclSize,
    IN ULONG AclRevision
)
{
    if (!Acl)
        return STATUS_INVALID_PARAMETER;

    if (AclRevision != VEA_ACL_REVISION)
        return STATUS_REVISION_MISMATCH;

    if (AclSize < sizeof(VEA_ACL) || AclSize > 0xFFFF)
        return STATUS_BUFFER_TOO_SMALL;

    RtlZeroMemory(Acl, AclSize);
    Acl->Revision = (UCHAR)AclRevision;
    Acl->AclSize  = (USHORT)AclSize;
    Acl->AceCount = 0;

    return STATUS_SUCCESS;
}

BOOLEAN
VEAPI
RtlValidAcl(
    IN PVEA_ACL Acl
)
{
    PUCHAR CurrentPtr;
    PUCHAR EndPtr;
    PVEA_ACE CurrentAce;

    if (!Acl) return FALSE;
    if (Acl->Revision != VEA_ACL_REVISION) return FALSE;
    if (Acl->AclSize < sizeof(VEA_ACL)) return FALSE;

    CurrentPtr = (PUCHAR)(Acl + 1);
    EndPtr     = (PUCHAR)Acl + Acl->AclSize;

    for (USHORT i = 0; i < Acl->AceCount; i++)
    {
        if (CurrentPtr + sizeof(VEA_ACE) > EndPtr)
            return FALSE;

        CurrentAce = (PVEA_ACE)CurrentPtr;
        if (CurrentAce->AceSize < sizeof(VEA_ACE))
            return FALSE;

        if (CurrentPtr + CurrentAce->AceSize > EndPtr)
            return FALSE;

        if (!RtlValidSid(&CurrentAce->TargetSid))
            return FALSE;

        CurrentPtr += CurrentAce->AceSize;
    }

    return TRUE;
}

/* EOF */