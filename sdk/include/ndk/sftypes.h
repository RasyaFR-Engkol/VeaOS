#pragma once

#include "procbind.h"

/* Prefix VEA menandakan kita bikin ini */

typedef struct _VEA_SECURITY_ID_DESCRIPTOR
{
    UCHAR Revision;       // Versi struktur SID (misal: 1)
    UCHAR Authority;      // 0 = Null, 1 = VeaOS Local, 2 = POSIX, 3 = System
    USHORT Flags;         // Penanda khusus (misal: IsGroup, IsRestricted)
    ULONG MachineID;      // ID Unik Mesin/Domain
    ULONG UserID;         // Relative ID (RID) milik User/Group
} VEA_SID, *PVEA_SID;

typedef enum _ACE_TYPE
{
    DENY = 0,
    ALLOW = 1
} ACE_TYPE;

typedef struct _VEA_ACCESS_CONTROL_ENTRY
{
    ACE_TYPE AceType;        // 0 = DENY, 1 = ALLOW
    UCHAR AceFlags;       // Aturan pewarisan (Inheritance ke sub-folder)
    USHORT AceSize;       // Ukuran struct ini di memori (penting untuk iterasi)
    ULONG AccessMask;     // Bitmask izin (READ, WRITE, DELETE, dsb)
    VEA_SID TargetSid;    // Untuk siapa aturan ini berlaku?
} VEA_ACE, *PVEA_ACE;

typedef struct _VEA_ACCESS_CONTROL_LIST
{
    UCHAR Revision;
    UCHAR Reserved;
    USHORT AclSize;       // Total ukuran memori (Header + Semua ACE)
    USHORT AceCount;      // Berapa banyak aturan ACE di dalam list ini?
    USHORT Reserved2;     // Padding agar struct sejajar 8-byte
    // Di belakang struct ini di memori, berjejer array dari VEA_ACE
} VEA_ACL, *PVEA_ACL;

typedef struct _VEA_SECURITY_DESCRIPTOR
{
    UCHAR Revision;
    UCHAR ControlFlags;   // Penanda: apakah DACL ada? apakah pakai Self-Relative?
    USHORT Reserved;
    PVEA_SID Owner;       // Penunjuk ke SID Pemilik (Hak Mutlak)
    PVEA_SID Group;       // Penunjuk ke SID Grup Utama Pemilik
    PVEA_ACL Dacl;        // Penunjuk ke daftar aturan (Discretionary ACL)
} VEA_SECURITY_DESCRIPTOR, *PVEA_SECURITY_DESCRIPTOR;

typedef struct _LUID
{
    ULONG LowPart;
    LONG HighPart;
} LUID, *PLUID;


typedef struct _GENERIC_MAPPING {
    ULONG GenericRead;
    ULONG GenericWrite;
    ULONG GenericExecute;
    ULONG GenericAll;
} GENERIC_MAPPING, *PGENERIC_MAPPING;

#define READ_CONTROL            (0x00020000L)

#define VEA_DELETE                   0x00010000L
#define VEA_READ_CONTROL             0x00020000L
#define VEA_WRITE_DAC                0x00040000L
#define VEA_WRITE_OWNER              0x00080000L
#define VEA_SYNCHRONIZE              0x00100000L
#define VEA_STANDARD_RIGHTS_REQUIRED 0x000F0000L
#define VEA_STANDARD_RIGHTS_READ     READ_CONTROL
#define VEA_STANDARD_RIGHTS_WRITE    READ_CONTROL
#define VEA_STANDARD_RIGHTS_EXECUTE  READ_CONTROL
#define VEA_STANDARD_RIGHTS_ALL      0x001F0000L
#define VEA_SPECIFIC_RIGHTS_ALL      0x0000FFFFL
#define VEA_ACCESS_SYSTEM_SECURITY   0x01000000L
#define VEA_MAXIMUM_ALLOWED          0x02000000L
#define VEA_GENERIC_READ             0x80000000L
#define VEA_GENERIC_WRITE            0x40000000L
#define VEA_GENERIC_EXECUTE          0x20000000L
#define VEA_GENERIC_ALL              0x10000000L

typedef ULONG ACCESS_MASK, *PACCESS_MASK;

typedef BOOLEAN SECURITY_CONTEXT_TRACKING_MODE, *PSECURITY_CONTEXT_TRACKING_MODE;

typedef enum _SECURITY_IMPERSONATION_LEVEL {
    SecurityAnonymous,
    SecurityIdentification,
    SecurityImpersonation,
    SecurityDelegation
} SECURITY_IMPERSONATION_LEVEL, * PSECURITY_IMPERSONATION_LEVEL;

typedef struct _SECURITY_QUALITY_OF_SERVICE {
    ULONG Length;
    SECURITY_IMPERSONATION_LEVEL ImpersonationLevel;
    SECURITY_CONTEXT_TRACKING_MODE ContextTrackingMode;
    BOOLEAN EffectiveOnly;
} SECURITY_QUALITY_OF_SERVICE, *PSECURITY_QUALITY_OF_SERVICE;

/* Revision standar VeaOS Security */
#define VEA_ACL_REVISION                 ('S')
#define VEA_SECURITY_DESCRIPTOR_REVISION ('S')

/* ControlFlags untuk VEA_SECURITY_DESCRIPTOR (8-bit UCHAR) */
#define VEA_SD_OWNER_DEFAULTED           0x01
#define VEA_SD_GROUP_DEFAULTED           0x02
#define VEA_SD_DACL_PRESENT              0x04
#define VEA_SD_DACL_DEFAULTED            0x08
#define VEA_SD_SACL_PRESENT              0x10
#define VEA_SD_SACL_DEFAULTED            0x20
#define VEA_SD_SELF_RELATIVE             0x80

/* AceFlags untuk pewarisan (Inheritance) */
#define VEA_OBJECT_INHERIT_ACE           0x01
#define VEA_CONTAINER_INHERIT_ACE        0x02
#define VEA_NO_PROPAGATE_INHERIT_ACE     0x04
#define VEA_INHERIT_ONLY_ACE             0x08
#define VEA_INHERITED_ACE                0x10

#define CONST_LUID(x1, x2) { (ULONG)(x1), (LONG)(x2) }

#define SF_MIN_WELL_KNOWN_PRIVILEGE            2
#define SF_CREATE_TOKEN_PRIVILEGE              2
#define SF_ASSIGNPRIMARYTOKEN_PRIVILEGE        3
#define SF_LOCK_MEMORY_PRIVILEGE               4
#define SF_INCREASE_QUOTA_PRIVILEGE            5
#define SF_UNSOLICITED_INPUT_PRIVILEGE         6
#define SF_TCB_PRIVILEGE                       7
#define SF_SECURITY_PRIVILEGE                  8
#define SF_TAKE_OWNERSHIP_PRIVILEGE            9
#define SF_LOAD_DRIVER_PRIVILEGE               10
#define SF_SYSTEM_PROFILE_PRIVILEGE            11
#define SF_SYSTEMTIME_PRIVILEGE                12
#define SF_PROF_SINGLE_PROCESS_PRIVILEGE       13
#define SF_INC_BASE_PRIORITY_PRIVILEGE         14
#define SF_CREATE_PAGEFILE_PRIVILEGE           15
#define SF_CREATE_PERMANENT_PRIVILEGE          16
#define SF_BACKUP_PRIVILEGE                    17
#define SF_RESTORE_PRIVILEGE                   18
#define SF_SHUTDOWN_PRIVILEGE                  19
#define SF_DEBUG_PRIVILEGE                     20
#define SF_AUDIT_PRIVILEGE                     21
#define SF_SYSTEM_ENVIRONMENT_PRIVILEGE        22
#define SF_CHANGE_NOTIFY_PRIVILEGE             23
#define SF_REMOTE_SHUTDOWN_PRIVILEGE           24
#define SF_UNDOCK_PRIVILEGE                    25
#define SF_SYNC_AGENT_PRIVILEGE                26
#define SF_ENABLE_DELEGATION_PRIVILEGE         27
#define SF_MANAGE_VOLUME_PRIVILEGE             28
#define SF_IMPERSONATE_PRIVILEGE               29
#define SF_CREATE_GLOBAL_PRIVILEGE             30
#define SF_TRUSTED_CREDMAN_ACCESS_PRIVILEGE    31
#define SF_RELABEL_PRIVILEGE                   32
#define SF_INC_WORKING_SET_PRIVILEGE           33
#define SF_TIME_ZONE_PRIVILEGE                 34
#define SF_CREATE_SYMBOLIC_LINK_PRIVILEGE      35
#define SF_MAX_WELL_KNOWN_PRIVILEGE            35
#define SF_MAXIMUM_PRIVILEGE_LIMIT             0x3C

#define SF_PRIVILEGE_BIT(PrivId) (1ULL << (PrivId))

/* Deklarasi Global Well-Known Privilege LUIDs */
extern const LUID SfCreateTokenPrivilege;
extern const LUID SfAssignPrimaryTokenPrivilege;
extern const LUID SfLockMemoryPrivilege;
extern const LUID SfIncreaseQuotaPrivilege;
extern const LUID SfTcbPrivilege;
extern const LUID SfSecurityPrivilege;
extern const LUID SfTakeOwnershipPrivilege;
extern const LUID SfLoadDriverPrivilege;
extern const LUID SfSystemProfilePrivilege;
extern const LUID SfSystemtimePrivilege;
extern const LUID SfProfileSingleProcessPrivilege;
extern const LUID SfIncreaseBasePriorityPrivilege;
extern const LUID SfCreatePagefilePrivilege;
extern const LUID SfCreatePermanentPrivilege;
extern const LUID SfBackupPrivilege;
extern const LUID SfRestorePrivilege;
extern const LUID SfShutdownPrivilege;
extern const LUID SfDebugPrivilege;
extern const LUID SfAuditPrivilege;
extern const LUID SfSystemEnvironmentPrivilege;
extern const LUID SfChangeNotifyPrivilege;
extern const LUID SfRemoteShutdownPrivilege;
extern const LUID SfUndockPrivilege;
extern const LUID SfSyncAgentPrivilege;
extern const LUID SfEnableDelegationPrivilege;
extern const LUID SfManageVolumePrivilege;
extern const LUID SfImpersonatePrivilege;
extern const LUID SfCreateGlobalPrivilege;
extern const LUID SfTrustedCredmanPrivilege;
extern const LUID SfRelabelPrivilege;
extern const LUID SfIncreaseWorkingSetPrivilege;
extern const LUID SfTimeZonePrivilege;
extern const LUID SfCreateSymbolicLinkPrivilege;

#define TOKEN_ASSIGN_PRIMARY    (0x0001)
#define TOKEN_DUPLICATE         (0x0002)
#define TOKEN_IMPERSONATE       (0x0004)
#define TOKEN_QUERY             (0x0008)
#define TOKEN_QUERY_SOURCE      (0x0010)
#define TOKEN_ADJUST_PRIVILEGES (0x0020)
#define TOKEN_ADJUST_GROUPS     (0x0040)
#define TOKEN_ADJUST_DEFAULT    (0x0080)
#define TOKEN_ADJUST_SESSIONID  (0x0100)
#define TOKEN_ALL_ACCESS        (VEA_STANDARD_RIGHTS_REQUIRED | 0x01FF)

#define SE_PRIVILEGE_ENABLED_BY_DEFAULT (0x00000001L)
#define SE_PRIVILEGE_ENABLED            (0x00000002L)
#define SE_PRIVILEGE_REMOVED            (0x00000004L)

typedef enum _TOKEN_TYPE {
    TokenPrimary = 1,
    TokenImpersonation
} TOKEN_TYPE, *PTOKEN_TYPE;

typedef struct _SID_AND_ATTRIBUTES {
    PVEA_SID Sid;
    ULONG Attributes;
} SID_AND_ATTRIBUTES, *PSID_AND_ATTRIBUTES;

typedef struct _LUID_AND_ATTRIBUTES {
    LUID Luid;
    ULONG Attributes;
} LUID_AND_ATTRIBUTES, *PLUID_AND_ATTRIBUTES;

typedef struct _TOKEN_USER {
    SID_AND_ATTRIBUTES User;
} TOKEN_USER, *PTOKEN_USER;

typedef struct _TOKEN_GROUPS {
    ULONG GroupCount;
    SID_AND_ATTRIBUTES Groups[1];
} TOKEN_GROUPS, *PTOKEN_GROUPS;

typedef struct _TOKEN_PRIVILEGES {
    ULONG PrivilegeCount;
    LUID_AND_ATTRIBUTES Privileges[1];
} TOKEN_PRIVILEGES, *PTOKEN_PRIVILEGES;

typedef struct _TOKEN_OWNER {
    PVEA_SID Owner;
} TOKEN_OWNER, *PTOKEN_OWNER;

typedef struct _TOKEN_PRIMARY_GROUP {
    PVEA_SID PrimaryGroup;
} TOKEN_PRIMARY_GROUP, *PTOKEN_PRIMARY_GROUP;

typedef struct _TOKEN_DEFAULT_DACL {
    PVEA_ACL DefaultDacl;
} TOKEN_DEFAULT_DACL, *PTOKEN_DEFAULT_DACL;

typedef struct _TOKEN_SOURCE {
    CHAR SourceName[8];
    LUID SourceIdentifier;
} TOKEN_SOURCE, *PTOKEN_SOURCE;

typedef struct _VEA_TOKEN
{
    VEA_SID UserSid;              // Identitas utama User yang login
    VEA_SID PrimaryGroup;         // Grup utama user
    VEA_SID OwnerSid;
    ULONGLONG PrivilegesPresent;   // Bitmask privilege yang dimiliki token (bit 2..60)
    ULONGLONG PrivilegesEnabled;   // Bitmask privilege yang sedang aktif dipakai
    ULONG PrivilegeMask;          // Hak istimewa sistem (misal: Boleh Reboot OS?)
    ULONG GroupCount;             // Jumlah grup tambahan yang diikuti
    PVEA_SID *SupplementaryGroups;// Array dari SID Grup tambahan
    PVEA_ACL DefaultDacl;         // ACL bawaan jika aplikasi ini bikin file baru
    LUID TokenId;
    LUID AuthenticationId;
    LARGE_INTEGER ExpirationTime;
    TOKEN_TYPE TokenType;
    SECURITY_IMPERSONATION_LEVEL ImpersonationLevel;
    TOKEN_SOURCE TokenSource;
} VEA_TOKEN, *PVEA_TOKEN;

extern PVEA_SID SfSystemSid;
extern PVEA_SID SfAdministratorSid;
extern PVEA_TOKEN SfSystemToken;
extern ULONGLONG SfGlobalLuidCounter;

/* EOF */
