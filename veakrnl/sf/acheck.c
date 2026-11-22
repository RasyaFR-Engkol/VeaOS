#include <veakrnl.h>

VOID
VEAPI
SfMapGenericMask(
    PULONG AccessMask,
    PGENERIC_MAPPING GenericMapping
)
{
    if (!AccessMask || !GenericMapping) return;

    if (*AccessMask & VEA_GENERIC_READ) {
        *AccessMask &= ~VEA_GENERIC_READ;
        *AccessMask |= GenericMapping->GenericRead;
    }

    if (*AccessMask & VEA_GENERIC_WRITE) {
        *AccessMask &= ~VEA_GENERIC_WRITE;
        *AccessMask |= GenericMapping->GenericWrite;
    }

    if (*AccessMask & VEA_GENERIC_EXECUTE) {
        *AccessMask &= ~VEA_GENERIC_EXECUTE;
        *AccessMask |= GenericMapping->GenericExecute;
    }

    if (*AccessMask & VEA_GENERIC_ALL) {
        *AccessMask &= ~VEA_GENERIC_ALL;
        *AccessMask |= GenericMapping->GenericAll;
    }
}

BOOLEAN
VEAPI
SfpMatchSidWithToken(
    PVEA_TOKEN Token,
    PVEA_SID TargetSid
)
{
    if (!Token || !TargetSid) return FALSE;

    if (SfEqualSid(&Token->UserSid, TargetSid)) {
        return TRUE;
    }

    if (Token->SupplementaryGroups) {
        for (ULONG i = 0; i < Token->GroupCount; i++) {
            if (Token->SupplementaryGroups[i] &&
                SfEqualSid(Token->SupplementaryGroups[i], TargetSid)) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

BOOLEAN
VEAPI
SfAccessCheck(
    PVEA_SECURITY_DESCRIPTOR SecurityDescriptor,
    PVEA_TOKEN ClientToken,
    ULONG DesiredAccess,
    PGENERIC_MAPPING GenericMapping,
    PULONG GrantedAccess
)
{
    if (!GrantedAccess) return FALSE;
    *GrantedAccess = 0;

    SfMapGenericMask(&DesiredAccess, GenericMapping);

    if (DesiredAccess == 0)
    {
        return TRUE;
    }

    if (!SecurityDescriptor || !SecurityDescriptor->Dacl)
    {
        *GrantedAccess = DesiredAccess;
        return TRUE;
    }

    if (!ClientToken)
    {
        DPRINT("ClientToken: %p\n", ClientToken);
        KdPrintf("[SF/DENY] ClientToken is NULL! DesiredAccess=0x%X\n", DesiredAccess);
        return FALSE;
    }

    PVEA_ACL Dacl = SecurityDescriptor->Dacl;

    if (Dacl->AceCount == 0)
    {
        KdPrintf("[SF/DENY] DACL AceCount == 0! DesiredAccess=0x%X\n", DesiredAccess);
        return FALSE;
    }

    ULONG RemainingAccess = DesiredAccess;
    ULONG LocalGrantedAccess = 0;
    PVEA_ACE CurrentAce = (PVEA_ACE)(Dacl + 1);

    for (USHORT i = 0; i < Dacl->AceCount; i++)
    {
        if (SfpMatchSidWithToken(ClientToken, &CurrentAce->TargetSid))
        {
            if (CurrentAce->AceType == DENY)
            {
                if (RemainingAccess & CurrentAce->AccessMask)
                {
                    KdPrintf("[SF/DENY] Hit DENY ACE[%u] Mask=0x%X\n", i, CurrentAce->AccessMask);
                    *GrantedAccess = 0;
                    return FALSE;
                }
            }
            else if (CurrentAce->AceType == ALLOW)
            {
                ULONG MatchedAccess = (RemainingAccess & CurrentAce->AccessMask);
                LocalGrantedAccess |= MatchedAccess;
                RemainingAccess &= ~MatchedAccess;

                if (RemainingAccess == 0)
                {
                    *GrantedAccess = LocalGrantedAccess;
                    return TRUE;
                }
            }
        }

        CurrentAce = (PVEA_ACE)((PUCHAR)CurrentAce + CurrentAce->AceSize);
    }

    /* Dump lengkap jika loop selesai tapi akses masih kurang */
    KdPrintf("[SF/DENY] No matching ALLOW ACE! Desired=0x%X, Remaining=0x%X\n",
             DesiredAccess, RemainingAccess);
    KdPrintf("  ClientToken->UserSid : Rev=%c Auth=%u Mach=0x%X User=0x%X\n",
             ClientToken->UserSid.Revision, ClientToken->UserSid.Authority,
             ClientToken->UserSid.MachineID, ClientToken->UserSid.UserID);

    PVEA_ACE FirstAce = (PVEA_ACE)(Dacl + 1);
    KdPrintf("  DACL ACE[0].TargetSid: Rev=%c Auth=%u Mach=0x%X User=0x%X (Mask=0x%X)\n",
             FirstAce->TargetSid.Revision, FirstAce->TargetSid.Authority,
             FirstAce->TargetSid.MachineID, FirstAce->TargetSid.UserID,
             FirstAce->AccessMask);

    *GrantedAccess = 0;
    return FALSE;
}

VOID
VEAPI
SfTestSecuritySubsystem(VOID)
{
    KdPrintf("\n\r================ SF ACCESS CHECK TEST ================\n\r");

    // 1. Buat Token Dummy User 0x1 (Allowed User)
    VEA_TOKEN TokenUser1;
    RtlZeroMemory(&TokenUser1, sizeof(VEA_TOKEN));
    TokenUser1.UserSid.Revision = 'S';
    TokenUser1.UserSid.Authority = 1;
    TokenUser1.UserSid.UserID = 0x1;

    // 2. Buat Token Dummy User 0x2 (Unauthorized User)
    VEA_TOKEN TokenUser2;
    RtlZeroMemory(&TokenUser2, sizeof(VEA_TOKEN));
    TokenUser2.UserSid.Revision = 'S';
    TokenUser2.UserSid.Authority = 1;
    TokenUser2.UserSid.UserID = 0x2;

    // 3. Buat DACL dengan 1 ALLOW ACE untuk User 0x1
    UCHAR AclBuffer[sizeof(VEA_ACL) + sizeof(VEA_ACE)];
    RtlZeroMemory(AclBuffer, sizeof(AclBuffer));

    PVEA_ACL Dacl = (PVEA_ACL)AclBuffer;
    Dacl->AceCount = 1;
    Dacl->AclSize = sizeof(AclBuffer);

    PVEA_ACE Ace1 = (PVEA_ACE)(Dacl + 1);
    Ace1->AceType = ALLOW;
    Ace1->AceSize = sizeof(VEA_ACE);
    Ace1->AccessMask = 0x0001; // Hak akses spesifik (misal: READ_DATA)
    Ace1->TargetSid.Revision = 'S';
    Ace1->TargetSid.Authority = 1;
    Ace1->TargetSid.UserID = 0x1; // Hanya izinkan User 0x1!

    // 4. Bungkus dalam Security Descriptor
    VEA_SECURITY_DESCRIPTOR Sd;
    RtlZeroMemory(&Sd, sizeof(VEA_SECURITY_DESCRIPTOR));
    Sd.Revision = 'S';
    Sd.Dacl = Dacl;

    // Generic Mapping dummy untuk pengujian
    GENERIC_MAPPING DummyMapping = {
        .GenericRead    = 0x0001,
        .GenericWrite   = 0x0002,
        .GenericExecute = 0x0004,
        .GenericAll     = 0x000F
    };

    ULONG GrantedAccess = 0;
    BOOLEAN AccessAllowed = FALSE;

    // --- UJI COBA 1: User 0x1 meminta VEA_GENERIC_READ ---
    AccessAllowed = SfAccessCheck(&Sd, &TokenUser1, VEA_GENERIC_READ, &DummyMapping, &GrantedAccess);
    if (AccessAllowed && GrantedAccess == 0x0001) {
        KdPrintf("[PASS] Test 1: User 0x1 granted access! (GrantedMask: 0x%X)\n\r", GrantedAccess);
    } else {
        KdPrintf("[FAIL] Test 1: User 0x1 blocked unexpectedly! (Allowed: %d)\n\r", AccessAllowed);
    }

    // --- UJI COBA 2: User 0x2 meminta VEA_GENERIC_READ ---
    GrantedAccess = 0;
    AccessAllowed = SfAccessCheck(&Sd, &TokenUser2, VEA_GENERIC_READ, &DummyMapping, &GrantedAccess);
    if (!AccessAllowed) {
        KdPrintf("[PASS] Test 2: User 0x2 correctly blocked! (STATUS_ACCESS_DENIED)\n\r");
    } else {
        KdPrintf("[FAIL] Test 2: Security Breach! User 0x2 bypass system! (GrantedMask: 0x%X)\n\r", GrantedAccess);
    }

    KdPrintf("=====================================================\n\r\n\r");
}

VEASTATUS
VEAPI
SfCreateAccessStateEx(
    IN PETHREAD Thread OPTIONAL,
    IN PEPROCESS Process,
    IN OUT PACCESS_STATE AccessState,
    IN OUT PAUX_ACCESS_DATA AuxData,
    IN ACCESS_MASK DesiredAccess,
    IN PGENERIC_MAPPING GenericMapping OPTIONAL
)
{
    PEPROCESS TargetProcess = Process;
    PVEA_TOKEN CapturedToken = NULL;

    if (!AccessState || !AuxData)
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(AccessState, sizeof(ACCESS_STATE));
    RtlZeroMemory(AuxData, sizeof(AUX_ACCESS_DATA));

    if (!TargetProcess)
    {
        TargetProcess = Thread ? Thread->ThreadProcess : (PEPROCESS)PtGetCurrentProcess();
    }

    /* Nanti kalau ETHREAD sudah punya ImpersonationToken, cek Thread dulu di sini */
    if (TargetProcess)
    {
        CapturedToken = TargetProcess->Token;
    }

    AccessState->OriginalDesiredAccess = DesiredAccess;

    if (GenericMapping)
    {
        AuxData->GenericMapping = *GenericMapping;
        SfMapGenericMask(&DesiredAccess, GenericMapping);
    }

    AccessState->Token                   = CapturedToken;
    AccessState->RemainingDesiredAccess  = DesiredAccess;
    AccessState->PreviouslyGrantedAccess = 0;
    AccessState->AuxData                 = AuxData;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
SfCreateAccessState(
    IN OUT PACCESS_STATE AccessState,
    IN OUT PAUX_ACCESS_DATA AuxData,
    IN ACCESS_MASK DesiredAccess,
    IN PGENERIC_MAPPING GenericMapping OPTIONAL
)
{
    return SfCreateAccessStateEx(
        (PETHREAD)PtGetCurrentThread(),
        (PEPROCESS)PtGetCurrentProcess(),
        AccessState,
        AuxData,
        DesiredAccess,
        GenericMapping
    );
}

VOID
VEAPI
SfDeleteAccessState(
    IN PACCESS_STATE AccessState
)
{
    if (!AccessState) return;

    /* Kalau nanti Token pakai reference counting (ObReferenceObject), dereference di sini */
    AccessState->Token = NULL;
    AccessState->AuxData = NULL;
}

/* EOF */