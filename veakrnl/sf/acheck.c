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

    // Kernel mode caller tanpa token / bypass
    if (!ClientToken) return FALSE;

    // 1. Map Generic Access Mask ke Spesifik Access Mask
    SfMapGenericMask(&DesiredAccess, GenericMapping);

    // Jika caller tidak minta hak apa-apa (0)
    if (DesiredAccess == 0)
    {
        return TRUE;
    }

    // 2. NULL Security Descriptor atau NULL DACL = Grant Full Access! (Standard NT Behavior)
    if (!SecurityDescriptor || !SecurityDescriptor->Dacl)
    {
        *GrantedAccess = DesiredAccess;
        return TRUE;
    }

    PVEA_ACL Dacl = SecurityDescriptor->Dacl;

    // Jika DACL kosong (AceCount == 0), tidak ada yang diizinkan
    if (Dacl->AceCount == 0)
    {
        return FALSE;
    }

    ULONG RemainingAccess = DesiredAccess;
    ULONG LocalGrantedAccess = 0;

    // Pointer ke ACE pertama (persis setelah header VEA_ACL)
    PVEA_ACE CurrentAce = (PVEA_ACE)(Dacl + 1);

    // 3. Iterasi seluruh ACE di dalam DACL
    for (USHORT i = 0; i < Dacl->AceCount; i++)
    {
        // Cek apakah TargetSid pada ACE cocok dengan Token Pemanggil
        if (SfpMatchSidWithToken(ClientToken, &CurrentAce->TargetSid))
        {
            if (CurrentAce->AceType == DENY)
            {
                // DENY ACE: Jika bit yang dilarang bertabrakan dengan bit yang diminta -> REJECT!
                if (RemainingAccess & CurrentAce->AccessMask)
                {
                    *GrantedAccess = 0;
                    return FALSE; // Access Denied!
                }
            }
            else if (CurrentAce->AceType == ALLOW)
            {
                // ALLOW ACE: Ambil bit izin yang dicocokkan
                ULONG MatchedAccess = (RemainingAccess & CurrentAce->AccessMask);
                
                LocalGrantedAccess |= MatchedAccess;
                RemainingAccess &= ~MatchedAccess; // Hapus bit yang sudah didapatkan

                // Jika seluruh hak akses yang diminta sudah terpenuhi!
                if (RemainingAccess == 0)
                {
                    *GrantedAccess = LocalGrantedAccess;
                    return TRUE; // Access Granted!
                }
            }
        }

        // Pindah ke ACE berikutnya di memori
        CurrentAce = (PVEA_ACE)((PUCHAR)CurrentAce + CurrentAce->AceSize);
    }

    // Jika loop selesai tapi masih ada hak akses yang belum terpenuhi
    if (RemainingAccess == 0)
    {
        *GrantedAccess = LocalGrantedAccess;
        return TRUE;
    }

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