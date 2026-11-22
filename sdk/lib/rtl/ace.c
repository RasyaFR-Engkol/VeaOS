/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : ace.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Security ACE Module
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <rtl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 17-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file ace.c
 * --------------------------------------------------------------------- */

static VEASTATUS
RtlpAddKnownAce(
    IN OUT PVEA_ACL Acl,
    IN ULONG AceRevision,
    IN UCHAR AceFlags,
    IN ACE_TYPE AceType,
    IN ACCESS_MASK AccessMask,
    IN PVEA_SID Sid
)
{
    PUCHAR CurrentPtr;
    PUCHAR AclEnd;
    PVEA_ACE NewAce;
    USHORT NewAceSize = sizeof(VEA_ACE);

    if (!Acl || !RtlValidSid(Sid))
        return STATUS_INVALID_PARAMETER;

    if (AceRevision != VEA_ACL_REVISION || Acl->Revision != VEA_ACL_REVISION)
        return STATUS_REVISION_MISMATCH;

    /* Cari ujung akhir dari ACE yang sudah ada di dalam ACL */
    CurrentPtr = (PUCHAR)(Acl + 1);
    AclEnd     = (PUCHAR)Acl + Acl->AclSize;

    for (USHORT i = 0; i < Acl->AceCount; i++)
    {
        PVEA_ACE ExistingAce = (PVEA_ACE)CurrentPtr;
        if (CurrentPtr + ExistingAce->AceSize > AclEnd)
            return STATUS_INVALID_ACL;

        CurrentPtr += ExistingAce->AceSize;
    }

    /* Pastikan sisa ruang di buffer ACL cukup untuk ACE baru */
    if (CurrentPtr + NewAceSize > AclEnd)
        return STATUS_ALLOTTED_SPACE_EXCEEDED;

    NewAce = (PVEA_ACE)CurrentPtr;
    NewAce->AceType    = AceType;
    NewAce->AceFlags   = AceFlags;
    NewAce->AceSize    = NewAceSize;
    NewAce->AccessMask = AccessMask;
    NewAce->TargetSid  = *Sid;

    Acl->AceCount++;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
RtlGetAce(
    IN PVEA_ACL Acl,
    IN ULONG AceIndex,
    OUT PVOID *Ace
)
{
    PUCHAR CurrentPtr;
    PVEA_ACE CurrentAce;

    if (!Acl || !Ace || !RtlValidAcl(Acl))
        return STATUS_INVALID_PARAMETER;

    if (AceIndex >= Acl->AceCount)
        return STATUS_INVALID_PARAMETER;

    CurrentPtr = (PUCHAR)(Acl + 1);
    for (ULONG i = 0; i < AceIndex; i++)
    {
        CurrentAce = (PVEA_ACE)CurrentPtr;
        CurrentPtr += CurrentAce->AceSize;
    }

    *Ace = (PVOID)CurrentPtr;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
RtlAddAccessAllowedAceEx(
    IN OUT PVEA_ACL Acl,
    IN ULONG AceRevision,
    IN ULONG AceFlags,
    IN ACCESS_MASK AccessMask,
    IN PVEA_SID Sid
)
{
    return RtlpAddKnownAce(Acl, AceRevision, (UCHAR)AceFlags, ALLOW, AccessMask, Sid);
}

VEASTATUS
VEAPI
RtlAddAccessAllowedAce(
    IN OUT PVEA_ACL Acl,
    IN ULONG AceRevision,
    IN ACCESS_MASK AccessMask,
    IN PVEA_SID Sid
)
{
    return RtlpAddKnownAce(Acl, AceRevision, 0, ALLOW, AccessMask, Sid);
}

VEASTATUS
VEAPI
RtlAddAccessDeniedAceEx(
    IN OUT PVEA_ACL Acl,
    IN ULONG AceRevision,
    IN ULONG AceFlags,
    IN ACCESS_MASK AccessMask,
    IN PVEA_SID Sid
)
{
    return RtlpAddKnownAce(Acl, AceRevision, (UCHAR)AceFlags, DENY, AccessMask, Sid);
}

VEASTATUS
VEAPI
RtlAddAccessDeniedAce(
    IN OUT PVEA_ACL Acl,
    IN ULONG AceRevision,
    IN ACCESS_MASK AccessMask,
    IN PVEA_SID Sid
)
{
    return RtlpAddKnownAce(Acl, AceRevision, 0, DENY, AccessMask, Sid);
}

VEASTATUS
VEAPI
RtlDeleteAce(
    IN OUT PVEA_ACL Acl,
    IN ULONG AceIndex
)
{
    PVEA_ACE TargetAce = NULL;
    VEASTATUS Status;
    PUCHAR NextAcePtr;
    PUCHAR UsedEndPtr;
    SIZE_T BytesToMove;

    Status = RtlGetAce(Acl, AceIndex, (PVOID*)&TargetAce);
    if (!VEA_SUCCESS(Status))
        return Status;

    /* Hitung total byte terpakai untuk menggeser sisa ACE ke depan */
    UsedEndPtr = (PUCHAR)(Acl + 1);
    for (USHORT i = 0; i < Acl->AceCount; i++)
    {
        UsedEndPtr += ((PVEA_ACE)UsedEndPtr)->AceSize;
    }

    NextAcePtr  = (PUCHAR)TargetAce + TargetAce->AceSize;
    BytesToMove = (SIZE_T)(UsedEndPtr - NextAcePtr);

    if (BytesToMove > 0)
    {
        RtlMoveMemory(TargetAce, NextAcePtr, BytesToMove);
    }

    Acl->AceCount--;
    return STATUS_SUCCESS;
}

/* EOF */