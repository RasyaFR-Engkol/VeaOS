#include <veakrnl.h>

PVEA_SID
VEAPI
SfAllocateAndInitializeSid(
    ULONG MachineID, 
    ULONG UserID,
    ULONG Authority
)
{
    PVEA_SID VeaSID = (PVEA_SID)UlAllocatePoolZero(NonPagedPool, sizeof(VEA_SID), 'SfId');
    if(!VeaSID)
    {
        return NULL;
    }

    VeaSID->Revision = 'S';
    VeaSID->Authority = Authority;
    VeaSID->Flags = 0;
    VeaSID->MachineID = MachineID;
    VeaSID->UserID = UserID;

    return VeaSID;
}

BOOLEAN
VEAPI
SfEqualSid(PVEA_SID SidTarget, PVEA_SID SidSource)
{
    if (!SidTarget || !SidSource)
    {
        return FALSE;
    }

    if (SidTarget->Revision != SidSource->Revision)
    {
        return FALSE;
    }

    if(SidTarget->Authority != SidSource->Authority)
    {
        // sudah pasti SID ini berbeda otoritas
        return FALSE;
    }

    if((SidTarget->MachineID != SidSource->MachineID) || 
       (SidSource->UserID != SidTarget->UserID))
    {
        return FALSE;
    }
    
    // mereka sama
    return TRUE;
}

PVEA_SECURITY_DESCRIPTOR
VEAPI
SfCreateNormalSecurityDescriptor(VOID)
{
    PVEA_SECURITY_DESCRIPTOR Sd = (PVEA_SECURITY_DESCRIPTOR)UlAllocatePoolWithTag(
        NonPagedPool, sizeof(VEA_SECURITY_DESCRIPTOR), 'SdSf'
    );
    if (!Sd) return NULL;

    PVEA_ACL Dacl = (SfSystemToken && SfSystemToken->DefaultDacl)
                    ? SfSystemToken->DefaultDacl
                    : SfCreateDefaultSystemDacl();

    RtlCreateSecurityDescriptor(Sd, VEA_SECURITY_DESCRIPTOR_REVISION);
    RtlSetOwnerSecurityDescriptor(Sd, SfSystemSid, FALSE);
    RtlSetGroupSecurityDescriptor(Sd, SfAdministratorSid, FALSE);
    RtlSetDaclSecurityDescriptor(Sd, TRUE, Dacl, TRUE);

    return Sd;
}