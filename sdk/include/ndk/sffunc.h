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
    ((PUCHAR)Acl + Acl->AclSize);

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

PVEA_TOKEN
VEAPI
SfDuplicateToken(
    IN PVEA_TOKEN SourceToken
);

VEASTATUS
VEAPI
SfCreateAccessStateEx(
    IN PETHREAD Thread OPTIONAL,
    IN PEPROCESS Process,
    IN OUT PACCESS_STATE AccessState,
    IN OUT PAUX_ACCESS_DATA AuxData,
    IN ACCESS_MASK DesiredAccess,
    IN PGENERIC_MAPPING GenericMapping OPTIONAL
);

VOID
VEAPI
SfDeleteAccessState(
    IN PACCESS_STATE AccessState
);

BOOLEAN
VEAPI
SfSinglePrivilegeCheck(
    IN LUID PrivilegeValue,
    IN KPROCESSOR_MODE PreviousMode
);

VOID
VEAPI
SfpCreateTokenObjectType(VOID);

LUID
VEAPI
SfpAllocateLocallyUniqueId(VOID);

VEASTATUS
VEAPI
VeaCreateToken(
    OUT PHANDLE TokenHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN TOKEN_TYPE TokenType,
    IN PLUID AuthenticationId,
    IN PLARGE_INTEGER ExpirationTime,
    IN PTOKEN_USER User,
    IN PTOKEN_GROUPS Groups OPTIONAL,
    IN PTOKEN_PRIVILEGES Privileges OPTIONAL,
    IN PTOKEN_OWNER Owner OPTIONAL,
    IN PTOKEN_PRIMARY_GROUP PrimaryGroup,
    IN PTOKEN_DEFAULT_DACL DefaultDacl OPTIONAL,
    IN PTOKEN_SOURCE TokenSource
);

/* EOF */