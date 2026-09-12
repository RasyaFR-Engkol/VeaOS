#pragma once

#include "sftypes.h"

PVEA_SID
VEAPI
SfAllocateAndInitializeSid(
    ULONG MachineID, 
    ULONG UserID,
    ULONG Authority
);

PVEA_TOKEN
VEAPI
SfCreateSystemGenericToken(VOID);

BOOLEAN
VEAPI
SfEqualSid(PVEA_SID SidTarget, PVEA_SID SidSource);

#define ACL_TO_ACE(Acl) \
    ((PUCHAR)Acl + Acl->AceSize);

PVEA_ACL
VEAPI
SfCreateDefaultSystemDacl(VOID);

BOOLEAN
VEAPI
SfInitSystem(ULONG BootPhase);

PVEA_SECURITY_DESCRIPTOR
VEAPI
SfCreateNormalSecurityDescriptor(VOID);

BOOLEAN
VEAPI
SfAccessCheck(
    PVEA_SECURITY_DESCRIPTOR SecurityDescriptor,
    PVEA_TOKEN Token,
    ULONG DesiredAccess,
    PGENERIC_MAPPING GenericMapping,
    PULONG GrantedAccess
);
