#include <veakrnl.h>

PVEA_ACL
VEAPI
SfCreateDefaultSystemDacl(VOID)
{
    ULONG AceCount = 2;
    USHORT TotalSize = (USHORT)(sizeof(VEA_ACL) + (AceCount * sizeof(VEA_ACE)));

    PVEA_ACL DACL = (PVEA_ACL)UlAllocatePoolWithTag(NonPagedPool, TotalSize, 'Vacl');
    if(!DACL)
    {
        return NULL;
    }
    
    DACL->Revision = 'S';
    DACL->Reserved = 0;
    DACL->AclSize = TotalSize;
    DACL->AceCount = (USHORT)AceCount;
    DACL->Reserved2 = 0;

    PVEA_ACE Ace1 = (PVEA_ACE)(DACL + 1);
    Ace1->AceType = 1; // 1 = ALLOW
    Ace1->AceFlags = 0;
    Ace1->AceSize = (USHORT)sizeof(VEA_ACE);
    Ace1->AccessMask = 0xFFFFFFFF; // Full Control / Generic All

    if (SfSystemSid)
    {
        Ace1->TargetSid = *SfSystemSid;
    }

    PVEA_ACE Ace2 = (PVEA_ACE)((PUCHAR)Ace1 + Ace1->AceSize);

    Ace2->AceType = 1; // 1 = ALLOW
    Ace2->AceFlags = 0;
    Ace2->AceSize = (USHORT)sizeof(VEA_ACE);
    Ace2->AccessMask = 0xFFFFFFFF; // Full Control / Generic All

    if (SfAdministratorSid)
    {
        Ace2->TargetSid = *SfAdministratorSid;
    }

    return DACL;
}