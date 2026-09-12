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

typedef struct _VEA_TOKEN
{
    VEA_SID UserSid;              // Identitas utama User yang login
    VEA_SID PrimaryGroup;         // Grup utama user
    ULONG PrivilegeMask;          // Hak istimewa sistem (misal: Boleh Reboot OS?)
    ULONG GroupCount;             // Jumlah grup tambahan yang diikuti
    PVEA_SID *SupplementaryGroups;// Array dari SID Grup tambahan
    PVEA_ACL DefaultDacl;         // ACL bawaan jika aplikasi ini bikin file baru
    LUID AuthenticationId;
} VEA_TOKEN, *PVEA_TOKEN;

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

extern PVEA_SID SfSystemSid;
extern PVEA_SID SfAdministratorSid;
extern PVEA_TOKEN SfSystemToken;

typedef ULONG ACCESS_MASK;
