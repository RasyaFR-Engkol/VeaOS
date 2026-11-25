#ifndef _VEASAM_H
#define _VEASAM_H

// Well-Known RIDs (Sesuai Windows NT)
#define DOMAIN_USER_RID_ADMIN           0x000001F4  // 500
#define DOMAIN_USER_RID_GUEST           0x000001F5  // 501
#define DOMAIN_GROUP_RID_USERS          0x00000201  // 513 (None)
#define DOMAIN_ALIAS_RID_ADMINS         0x00000220  // 544 (Administrators)
#define DOMAIN_ALIAS_RID_USERS          0x00000221  // 545 (Users)
#define DOMAIN_ALIAS_RID_GUESTS         0x00000222  // 546 (Guests)

// UserAccountControl Flags
#define USER_ACCOUNT_DISABLED           0x00000001
#define USER_NORMAL_ACCOUNT             0x00000010
#define USER_DONT_EXPIRE_PASSWORD       0x00000200

// Helper hitung ukuran fisik SID berdasarkan SubAuthorityCount
#define VEA_SID_SIZE(sub_count) (8 + ((sub_count) * sizeof(ULONG)))

// Entry Tabel Direktori untuk blob V dan C (12 byte per entry)
typedef struct __attribute__((packed)) _SAM_V_ENTRY {
    ULONG Offset;    // Offset relatif dari akhir tabel header
    ULONG Length;    // Panjang data dalam byte
    ULONG Flags;     // Biasanya 1 (atau 0 jika kosong)
} SAM_V_ENTRY;

// =========================================================================
// 1. DOMAIN BLOBS (Account & Builtin -> F dan V)
// =========================================================================
typedef struct __attribute__((packed)) _SAM_DOMAIN_F {
    USHORT Revision;                  // 0x0003
    USHORT Unused1;
    ULONG Unused2;
    ULONGLONG CreationTime;
    ULONGLONG DomainModifiedCount;
    ULONGLONG MaxPasswordAge;
    ULONGLONG MinPasswordAge;
    ULONGLONG ForceLogoff;
    ULONGLONG LockoutDuration;
    ULONGLONG LockoutObservationWindow;
    ULONGLONG ModifiedCountAtLastPromotion;
    ULONG NextRid;                   // Mulai dari 0x3E9 (1001) setelah user 0x3E8 dibuat
    ULONG PasswordProperties;
    USHORT MinPasswordLength;
    USHORT PasswordHistoryLength;
    USHORT LockoutThreshold;
    USHORT Unused3;
    ULONG ServerState;               // 1 = DomainEnabled
    ULONG ServerRole;                // 3 = DomainServerRolePrimary
} SAM_DOMAIN_F;

// Index untuk SAM_DOMAIN_V_HEADER.Entries
enum {
    SAM_DOMAIN_V_SEC_DESC    = 0,
    SAM_DOMAIN_V_DOMAIN_NAME = 1,
    SAM_DOMAIN_V_SERVER_NAME = 2,
    SAM_DOMAIN_V_DOMAIN_SID  = 3,       // Menyimpan struct VEA_SID
    SAM_DOMAIN_V_ENTRY_COUNT = 4
};

typedef struct __attribute__((packed)) _SAM_DOMAIN_V_HEADER {
    SAM_V_ENTRY Entries[SAM_DOMAIN_V_ENTRY_COUNT]; // 4 * 12 = 48 byte (0x30)
    // Diikuti langsung oleh variable data payload
} SAM_DOMAIN_V_HEADER;

// =========================================================================
// 2. USER BLOBS (Users\<RID> -> F dan V)
// =========================================================================
typedef struct __attribute__((packed)) _SAM_USER_F {
    USHORT Revision;                  // 0x0003
    USHORT Unused1;
    ULONG Unused2;
    ULONGLONG LastLogon;
    ULONGLONG LastLogoff;
    ULONGLONG PasswordLastSet;
    ULONGLONG AccountExpires;            // 0x7FFFFFFFFFFFFFFFULL (Never)
    ULONGLONG PasswordCanChange;
    ULONGLONG PasswordMustChange;        // 0x7FFFFFFFFFFFFFFFULL (Never)
    ULONG RelativeId;                // RID user (misal 0x3E8)
    ULONG PrimaryGroupId;            // Default 0x201 (513)
    ULONG UserAccountControl;        // Bitmask USER_*
    USHORT CountryCode;
    USHORT CodePage;
    USHORT BadPasswordCount;
    USHORT LogonCount;
    ULONG AdminCount;                // 1 jika masuk grup Administrators
} SAM_USER_F;                           // Total: 80 byte (0x50)

// Index untuk SAM_USER_V_HEADER.Entries
enum {
    SAM_USER_V_SEC_DESC     = 0,
    SAM_USER_V_USERNAME     = 1,        // UTF-16LE
    SAM_USER_V_FULLNAME     = 2,        // UTF-16LE
    SAM_USER_V_COMMENT      = 3,        // UTF-16LE
    SAM_USER_V_USER_COMMENT = 4,
    SAM_USER_V_RESERVED1    = 5,
    SAM_USER_V_HOMEDIR      = 6,        // UTF-16LE
    SAM_USER_V_HOMEDIR_CONN = 7,
    SAM_USER_V_SCRIPT_PATH  = 8,        // UTF-16LE
    SAM_USER_V_PROFILE_PATH = 9,        // UTF-16LE
    SAM_USER_V_WORKSTATIONS = 10,
    SAM_USER_V_HOURS_ALLOWED= 11,
    SAM_USER_V_RESERVED2    = 12,
    SAM_USER_V_LM_HASH      = 13,       // 16 byte (kosong di NT modern)
    SAM_USER_V_NT_HASH      = 14,       // 16 byte MD4(UTF-16LE(password))
    SAM_USER_V_RESERVED3    = 15,
    SAM_USER_V_RESERVED4    = 16,
    SAM_USER_V_ENTRY_COUNT  = 17
};

typedef struct __attribute__((packed)) _SAM_USER_V_HEADER {
    SAM_V_ENTRY Entries[SAM_USER_V_ENTRY_COUNT]; // 17 * 12 = 204 byte (0xCC)
    // Diikuti langsung oleh variable data payload
} SAM_USER_V_HEADER;

// =========================================================================
// 3. ALIAS BLOB (Aliases\<RID> -> C)
// =========================================================================
enum {
    SAM_ALIAS_C_SEC_DESC    = 0,
    SAM_ALIAS_C_NAME        = 1,        // UTF-16LE (misal "Administrators")
    SAM_ALIAS_C_COMMENT     = 2,        // UTF-16LE
    SAM_ALIAS_C_MEMBERS     = 3,        // Array VEA_SID anggota berurutan
    SAM_ALIAS_C_ENTRY_COUNT = 4
};

typedef struct __attribute__((packed)) _SAM_ALIAS_C_HEADER {
    USHORT    Revision;               // 0x0001
    USHORT    Unused1;
    ULONG    Unused2;
    ULONGLONG    CreationTime;
    ULONGLONG    DomainModifiedCount;
    ULONG    MemberCount;            // Jumlah SID anggota
    SAM_V_ENTRY Entries[SAM_ALIAS_C_ENTRY_COUNT]; // 4 * 12 = 48 byte
} SAM_ALIAS_C_HEADER;                   // Total header: 76 byte

#endif // _VEASAM_H