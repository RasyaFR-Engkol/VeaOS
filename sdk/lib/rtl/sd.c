/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : sd.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Create Security Descriptor
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <rtl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 17-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file sd.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
RtlCreateSecurityDescriptor(
    OUT PVEA_SECURITY_DESCRIPTOR SecurityDescriptor,
    IN ULONG Revision
)
{
    if (!SecurityDescriptor)
        return STATUS_INVALID_PARAMETER;

    if (Revision != VEA_SECURITY_DESCRIPTOR_REVISION)
        return STATUS_UNKNOWN_REVISION;

    RtlZeroMemory(SecurityDescriptor, sizeof(VEA_SECURITY_DESCRIPTOR));
    SecurityDescriptor->Revision = (UCHAR)Revision;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
RtlSetDaclSecurityDescriptor(
    IN OUT PVEA_SECURITY_DESCRIPTOR SecurityDescriptor,
    IN BOOLEAN DaclPresent,
    IN PVEA_ACL Dacl OPTIONAL,
    IN BOOLEAN DaclDefaulted
)
{
    if (!SecurityDescriptor)
        return STATUS_INVALID_PARAMETER;

    if (SecurityDescriptor->Revision != VEA_SECURITY_DESCRIPTOR_REVISION)
        return STATUS_UNKNOWN_REVISION;

    if (SecurityDescriptor->ControlFlags & VEA_SD_SELF_RELATIVE)
        return STATUS_INVALID_SECURITY_DESCR;

    if (!DaclPresent)
    {
        SecurityDescriptor->ControlFlags &= ~(VEA_SD_DACL_PRESENT | VEA_SD_DACL_DEFAULTED);
        SecurityDescriptor->Dacl = NULL;
        return STATUS_SUCCESS;
    }

    if (Dacl && !RtlValidAcl(Dacl))
        return STATUS_INVALID_ACL;

    SecurityDescriptor->ControlFlags |= VEA_SD_DACL_PRESENT;
    SecurityDescriptor->Dacl = Dacl;

    if (DaclDefaulted)
        SecurityDescriptor->ControlFlags |= VEA_SD_DACL_DEFAULTED;
    else
        SecurityDescriptor->ControlFlags &= ~VEA_SD_DACL_DEFAULTED;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
RtlGetDaclSecurityDescriptor(
    IN PVEA_SECURITY_DESCRIPTOR SecurityDescriptor,
    OUT PBOOLEAN DaclPresent,
    OUT PVEA_ACL *Dacl,
    OUT PBOOLEAN DaclDefaulted
)
{
    if (!SecurityDescriptor || !DaclPresent || !Dacl || !DaclDefaulted)
        return STATUS_INVALID_PARAMETER;

    if (SecurityDescriptor->Revision != VEA_SECURITY_DESCRIPTOR_REVISION)
        return STATUS_UNKNOWN_REVISION;

    if (SecurityDescriptor->ControlFlags & VEA_SD_DACL_PRESENT)
    {
        *DaclPresent   = TRUE;
        *Dacl          = SecurityDescriptor->Dacl;
        *DaclDefaulted = (SecurityDescriptor->ControlFlags & VEA_SD_DACL_DEFAULTED) ? TRUE : FALSE;
    }
    else
    {
        *DaclPresent   = FALSE;
        *Dacl          = NULL;
        *DaclDefaulted = FALSE;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
RtlSetOwnerSecurityDescriptor(
    IN OUT PVEA_SECURITY_DESCRIPTOR SecurityDescriptor,
    IN PVEA_SID Owner OPTIONAL,
    IN BOOLEAN OwnerDefaulted
)
{
    if (!SecurityDescriptor)
        return STATUS_INVALID_PARAMETER;

    if (SecurityDescriptor->Revision != VEA_SECURITY_DESCRIPTOR_REVISION)
        return STATUS_UNKNOWN_REVISION;

    if (Owner && !RtlValidSid(Owner))
        return STATUS_INVALID_SID;

    SecurityDescriptor->Owner = Owner;

    if (OwnerDefaulted)
        SecurityDescriptor->ControlFlags |= VEA_SD_OWNER_DEFAULTED;
    else
        SecurityDescriptor->ControlFlags &= ~VEA_SD_OWNER_DEFAULTED;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
RtlSetGroupSecurityDescriptor(
    IN OUT PVEA_SECURITY_DESCRIPTOR SecurityDescriptor,
    IN PVEA_SID Group OPTIONAL,
    IN BOOLEAN GroupDefaulted
)
{
    if (!SecurityDescriptor)
        return STATUS_INVALID_PARAMETER;

    if (SecurityDescriptor->Revision != VEA_SECURITY_DESCRIPTOR_REVISION)
        return STATUS_UNKNOWN_REVISION;

    if (Group && !RtlValidSid(Group))
        return STATUS_INVALID_SID;

    SecurityDescriptor->Group = Group;

    if (GroupDefaulted)
        SecurityDescriptor->ControlFlags |= VEA_SD_GROUP_DEFAULTED;
    else
        SecurityDescriptor->ControlFlags &= ~VEA_SD_GROUP_DEFAULTED;

    return STATUS_SUCCESS;
}

BOOLEAN
VEAPI
RtlValidSecurityDescriptor(
    IN PVEA_SECURITY_DESCRIPTOR SecurityDescriptor
)
{
    if (!SecurityDescriptor) return FALSE;
    if (SecurityDescriptor->Revision != VEA_SECURITY_DESCRIPTOR_REVISION) return FALSE;

    if (SecurityDescriptor->Owner && !RtlValidSid(SecurityDescriptor->Owner))
        return FALSE;

    if (SecurityDescriptor->Group && !RtlValidSid(SecurityDescriptor->Group))
        return FALSE;

    if ((SecurityDescriptor->ControlFlags & VEA_SD_DACL_PRESENT) && SecurityDescriptor->Dacl)
    {
        if (!RtlValidAcl(SecurityDescriptor->Dacl))
            return FALSE;
    }

    return TRUE;
}

ULONG
VEAPI
RtlLengthSecurityDescriptor(
    IN PVEA_SECURITY_DESCRIPTOR SecurityDescriptor
)
{
    ULONG TotalLength = sizeof(VEA_SECURITY_DESCRIPTOR);

    if (!SecurityDescriptor) return 0;

    if (SecurityDescriptor->Owner) TotalLength += sizeof(VEA_SID);
    if (SecurityDescriptor->Group) TotalLength += sizeof(VEA_SID);
    if ((SecurityDescriptor->ControlFlags & VEA_SD_DACL_PRESENT) && SecurityDescriptor->Dacl)
    {
        TotalLength += SecurityDescriptor->Dacl->AclSize;
    }

    return TotalLength;
}

/* EOF */