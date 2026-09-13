#include <veakrnl.h>

PVEA_TOKEN
VEAPI
SfCreateSystemGenericToken(VOID)
{
    PVEA_TOKEN Token;

    Token = UlAllocatePoolZero(NonPagedPool, sizeof(VEA_TOKEN), 'Tkn ');
    if (!Token) return NULL;

    Token->UserSid = *SfSystemSid;

    // Grup Admin
    Token->GroupCount = 1;
    Token->SupplementaryGroups = UlAllocatePoolZero(NonPagedPool, sizeof(PVEA_SID), 'PSID');
    Token->SupplementaryGroups[0] = SfAdministratorSid;

    Token->PrivilegeMask = 0xFFFFFFFF;

    Token->DefaultDacl = SfCreateDefaultSystemDacl();

    Token->AuthenticationId.LowPart = 999;
    Token->AuthenticationId.HighPart = 0;

    return Token;
}