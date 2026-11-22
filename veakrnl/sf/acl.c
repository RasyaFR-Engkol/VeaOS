#include <veakrnl.h>

PVEA_ACL
VEAPI
SfCreateDefaultSystemDacl(VOID)
{
    USHORT TotalSize = sizeof(VEA_ACL) + (2 * sizeof(VEA_ACE));
    PVEA_ACL Dacl = (PVEA_ACL)UlAllocatePoolWithTag(NonPagedPool, TotalSize, 'Vacl');
    if (!Dacl) return NULL;

    RtlCreateAcl(Dacl, TotalSize, VEA_ACL_REVISION);
    RtlAddAccessAllowedAce(Dacl, VEA_ACL_REVISION, 0xFFFFFFFF, SfSystemSid);
    RtlAddAccessAllowedAce(Dacl, VEA_ACL_REVISION, 0xFFFFFFFF, SfAdministratorSid);

    return Dacl;
}