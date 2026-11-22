#include <veakrnl.h>

static GENERIC_MAPPING SfpTokenMapping =
{
    VEA_STANDARD_RIGHTS_READ    | TOKEN_QUERY | TOKEN_QUERY_SOURCE,
    VEA_STANDARD_RIGHTS_WRITE   | TOKEN_ADJUST_PRIVILEGES | TOKEN_ADJUST_GROUPS | TOKEN_ADJUST_DEFAULT,
    VEA_STANDARD_RIGHTS_EXECUTE | TOKEN_IMPERSONATE,
    TOKEN_ALL_ACCESS
};

POBJECT_TYPE SfTokenObjectType = NULL;

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

PVEA_TOKEN
VEAPI
SfDuplicateToken(
    IN PVEA_TOKEN SourceToken
)
{
    PVEA_TOKEN NewToken;

    if (!SourceToken) return NULL;

    NewToken = (PVEA_TOKEN)UlAllocatePoolZero(NonPagedPool, sizeof(VEA_TOKEN), 'Tkn ');
    if (!NewToken) return NULL;

    RtlCopyMemory(NewToken, SourceToken, sizeof(VEA_TOKEN));

    if (SourceToken->GroupCount > 0 && SourceToken->SupplementaryGroups)
    {
        ULONG GroupsSize = SourceToken->GroupCount * sizeof(PVEA_SID);
        NewToken->SupplementaryGroups = (PVEA_SID*)UlAllocatePoolZero(NonPagedPool, GroupsSize, 'PSID');
        if (NewToken->SupplementaryGroups)
        {
            RtlCopyMemory(NewToken->SupplementaryGroups, SourceToken->SupplementaryGroups, GroupsSize);
        }
        else
        {
            NewToken->GroupCount = 0;
        }
    }

    return NewToken;
}

VOID
VEAPI
SfpCreateTokenObjectType(VOID)
{
    OBJECT_TYPE_INITIALIZER Initializer;
    RtlZeroMemory(&Initializer, sizeof(Initializer));

    Initializer.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    Initializer.DefaultNonPagedPoolCharge = sizeof(VEA_TOKEN);
    Initializer.ObjectSize = sizeof(VEA_TOKEN);
    Initializer.GenericMapping = SfpTokenMapping;
    Initializer.ValidAccessMask = TOKEN_ALL_ACCESS;
    Initializer.AllowAttachByDefault = FALSE;
    
    ObCreateObjectType("Token", &Initializer, &SfTokenObjectType);
}

VEASTATUS
VEAPI
SfpCreateToken(
    OUT PHANDLE TokenHandle,
    IN KPROCESSOR_MODE RequestorMode,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN TOKEN_TYPE TokenType,
    IN SECURITY_IMPERSONATION_LEVEL ImpersonationLevel,
    IN PLUID AuthenticationId,
    IN PLARGE_INTEGER ExpirationTime,
    IN PTOKEN_USER User,
    IN PTOKEN_GROUPS Groups OPTIONAL,
    IN PTOKEN_PRIVILEGES Privileges OPTIONAL,
    IN PTOKEN_OWNER Owner OPTIONAL,
    IN PTOKEN_PRIMARY_GROUP PrimaryGroup,
    IN PTOKEN_DEFAULT_DACL DefaultDacl OPTIONAL,
    IN PTOKEN_SOURCE TokenSource,
    IN BOOLEAN SystemToken,
    OUT PVEA_TOKEN *CreatedToken OPTIONAL
)
{
    PVEA_TOKEN Token = NULL;
    VEASTATUS Status;
    HANDLE hToken = NULL;

    /* 1. Validasi parameter dasar ala NT */
    if (!AuthenticationId || !ExpirationTime || !User || !User->User.Sid ||
        !PrimaryGroup || !PrimaryGroup->PrimaryGroup || !TokenSource)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (!SystemToken && !TokenHandle)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (TokenType != TokenPrimary && TokenType != TokenImpersonation)
    {
        return STATUS_BAD_TOKEN_TYPE;
    }

    if (TokenType == TokenImpersonation &&
        (ImpersonationLevel < SecurityAnonymous || ImpersonationLevel > SecurityDelegation))
    {
        return STATUS_BAD_IMPERSONATION_LEVEL;
    }

    if (!RtlValidSid(User->User.Sid) || !RtlValidSid(PrimaryGroup->PrimaryGroup))
    {
        return STATUS_INVALID_SID;
    }

    /* 2. Alokasi objek VEA_TOKEN lewat Object Manager */
    Status = ObCreateObject(
        RequestorMode,
        SfTokenObjectType,
        ObjectAttributes,
        RequestorMode,
        NULL,
        sizeof(VEA_TOKEN),
        0,
        0,
        (PVOID*)&Token
    );

    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    RtlZeroMemory(Token, sizeof(VEA_TOKEN));

    /* 3. Isi Identitas Utama, TokenId (dari SfGlobalLuidCounter), & Metadata */
    Token->TokenId            = SfpAllocateLocallyUniqueId();
    Token->AuthenticationId   = *AuthenticationId;
    Token->ExpirationTime     = *ExpirationTime;
    Token->TokenType          = TokenType;
    Token->ImpersonationLevel = (TokenType == TokenImpersonation) ? ImpersonationLevel : SecurityAnonymous;
    Token->TokenSource        = *TokenSource;

    RtlCopySid(sizeof(VEA_SID), &Token->UserSid, User->User.Sid);
    RtlCopySid(sizeof(VEA_SID), &Token->PrimaryGroup, PrimaryGroup->PrimaryGroup);

    if (Owner && Owner->Owner)
    {
        if (!RtlValidSid(Owner->Owner))
        {
            ObDereferenceObject(Token);
            return STATUS_INVALID_SID;
        }
        RtlCopySid(sizeof(VEA_SID), &Token->OwnerSid, Owner->Owner);
    }
    else
    {
        Token->OwnerSid = Token->UserSid;
    }

    /* 4. Salin Supplementary Groups (Pointer Array + Data SID dalam 1 alokasi) */
    if (Groups && Groups->GroupCount > 0)
    {
        ULONG Count = Groups->GroupCount;
        SIZE_T PtrArraySize = Count * sizeof(PVEA_SID);
        SIZE_T TotalGroupBytes = PtrArraySize + (Count * sizeof(VEA_SID));

        PUCHAR GroupBlock = (PUCHAR)UlAllocatePoolZero(NonPagedPool, TotalGroupBytes, 'PSID');
        if (!GroupBlock)
        {
            ObDereferenceObject(Token);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Token->GroupCount = Count;
        Token->SupplementaryGroups = (PVEA_SID*)GroupBlock;
        PVEA_SID SidStorage = (PVEA_SID)(GroupBlock + PtrArraySize);

        for (ULONG i = 0; i < Count; i++)
        {
            if (!RtlValidSid(Groups->Groups[i].Sid))
            {
                UlFreePool(GroupBlock);
                Token->SupplementaryGroups = NULL;
                ObDereferenceObject(Token);
                return STATUS_INVALID_SID;
            }

            SidStorage[i] = *(Groups->Groups[i].Sid);
            Token->SupplementaryGroups[i] = &SidStorage[i];
        }
    }

    /* 5. Konversi array LUID_AND_ATTRIBUTES ke Bitmask Privilege 64-bit */
    if (Privileges && Privileges->PrivilegeCount > 0)
    {
        for (ULONG i = 0; i < Privileges->PrivilegeCount; i++)
        {
            LUID PrivLuid = Privileges->Privileges[i].Luid;
            ULONG Attr    = Privileges->Privileges[i].Attributes;

            if (PrivLuid.HighPart == 0 &&
                PrivLuid.LowPart >= SF_MIN_WELL_KNOWN_PRIVILEGE &&
                PrivLuid.LowPart <= SF_MAXIMUM_PRIVILEGE_LIMIT)
            {
                ULONGLONG Bit = SF_PRIVILEGE_BIT(PrivLuid.LowPart);
                Token->PrivilegesPresent |= Bit;

                if (Attr & (SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT))
                {
                    Token->PrivilegesEnabled |= Bit;
                }
            }
        }
    }

    /* 6. Salin DefaultDacl jika disertakan */
    if (DefaultDacl && DefaultDacl->DefaultDacl)
    {
        PVEA_ACL SrcDacl = DefaultDacl->DefaultDacl;
        if (!RtlValidAcl(SrcDacl))
        {
            if (Token->SupplementaryGroups) UlFreePool(Token->SupplementaryGroups);
            ObDereferenceObject(Token);
            return STATUS_INVALID_ACL;
        }

        Token->DefaultDacl = (PVEA_ACL)UlAllocatePoolWithTag(NonPagedPool, SrcDacl->AclSize, 'Vacl');
        if (!Token->DefaultDacl)
        {
            if (Token->SupplementaryGroups) UlFreePool(Token->SupplementaryGroups);
            ObDereferenceObject(Token);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlCopyMemory(Token->DefaultDacl, SrcDacl, SrcDacl->AclSize);
    }

    /* 7. Kembalikan pointer langsung (SystemToken) atau terbitkan Handle ke proses */
    if (SystemToken)
    {
        if (CreatedToken) *CreatedToken = Token;
        return STATUS_SUCCESS;
    }

    Status = ObInsertObject(
        Token,
        NULL,
        DesiredAccess,
        0,
        NULL,
        &hToken
    );

    if (VEA_SUCCESS(Status))
    {
        *TokenHandle = hToken;
        if (CreatedToken) *CreatedToken = Token;
    }

    return Status;
}

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
)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();

    if (!SfSinglePrivilegeCheck(SfCreateTokenPrivilege, PreviousMode))
    {
        return STATUS_PRIVILEGE_NOT_HELD;
    }

    return SfpCreateToken(
        TokenHandle,
        PreviousMode,
        DesiredAccess,
        ObjectAttributes,
        TokenType,
        SecurityAnonymous,
        AuthenticationId,
        ExpirationTime,
        User,
        Groups,
        Privileges,
        Owner,
        PrimaryGroup,
        DefaultDacl,
        TokenSource,
        FALSE,
        NULL
    );
}

/* EOF */