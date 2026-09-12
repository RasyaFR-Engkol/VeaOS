/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : vktypes.h
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Header file for storing data type for Vk subsystem
 ----- Effective since 07-2026 til FOREVER ------------------------------- */

#pragma once
#include "obtype.h"
#include "procbind.h"
#include "sftypes.h"

/* Revision History ------------------------------------------------------
 * DATE       : 30-07-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file vktypes.h
 * --------------------------------------------------------------------- */

#pragma pack(push, 1)

// This is key header
typedef struct _KEY_FILE_HEADER {
    ULONG Signature;              // Magic Signature File (misal: "KEY1" / 0x3159454B)
    ULONG PrimarySequence;        // Sequence counter utama (untuk validasi flush/write)
    ULONG SecondarySequence;      // Sequence counter cadangan
    CHAR     ReservedInCHAR1[10];    // Area reserved awal
    ULONG Checksum;               // Checksum XOR / CRC32 header
    CHAR     ReservedInCHAR2[4070];  // Padding otomatis agar pas tepat 0x1000 byte (4 KB)
} KEY_FILE_HEADER, *PKEY_FILE_HEADER;

// This is key base block
typedef struct _KEY_BASE_BLOCK {
    ULONG Signature;              // Magic Signature Bin (misal: "BIN1" / 0x314E4942)
    ULONG RelativeOffset;         // Offset relatif blok ini dari awal Base Block (0x0, 0x1000, dst)
    ULONG Size;                   // Ukuran total blok ini (misal: 0x1000 = 4096 byte)
    ULONGLONG LastWriteTime;          // Timestamp modifikasi terakhir (64-bit Epoch / FILETIME)
    ULONG Spare;                  // Alignment / Cadangan
} KEY_BASE_BLOCK, *PKEY_BASE_BLOCK;

// This is key directory node
typedef struct _KEY_DIRECTORY_NODE {
    LONG  CellSize;               // Ukuran sel (Nilai Negatif = Teralokasi / Used)
    USHORT Signature;              // "DN" (Directory Node = 0x4E44)
    USHORT Flags;                  // Flags (misal: 0x01 = Root, 0x02 = ReadOnly)
    ULONG ParentOffset;           // Pointer Offset menunjuk ke Parent Folder
    ULONG FirstSubKeyOffset;      // Pointer Offset menunjuk ke Anak Folder Pertama
    ULONG NextSiblingOffset;      // Pointer Offset menunjuk ke Folder Samping/Sejajar
    ULONG FirstValueOffset;       // Pointer Offset menunjuk ke File/Value Pertama
    ULONG SubKeyCount;            // Total jumlah sub-folder di dalam folder ini
    USHORT NameLength;             // Panjang string nama folder
    CHAR     Name[];                 // Flexible Array Member: String Nama Folder
} KEY_DIRECTORY_NODE, *PKEY_DIRECTORY_NODE;

// this is key file node
typedef struct _KEY_FILE_NODE {
    LONG  CellSize;               // Ukuran sel (Nilai Negatif = Teralokasi / Used)
    USHORT Signature;              // "FN" (File Node = 0x4E46)
    ULONG NextValueOffset;        // Pointer Offset menunjuk ke File/Value Sejajar
    USHORT NameLength;             // Panjang string nama file/value
    ULONG DataPOffset;            // Offset lokasi payload data (atau Inline Data jika data kecil)
    ULONG DataLength;             // Ukuran payload data dalam byte
    ULONG Type;                   // Tipe Data (1 = String, 2 = Int32, 3 = Binary, dll)
    USHORT Flags;                  // Flags tambahan
    CHAR     Name[];                 // Flexible Array Member: String Nama File/Value
} KEY_FILE_NODE, *PKEY_FILE_NODE;

#pragma pack(pop)

// Key Header
#define KEY_HEADER_SIG  0x3159454B  // "KEY1"
#define KEY_BIN_SIG     0x314E4942  // "BIN1"
#define KEY_DIR_SIG     0x4E44      // "DN"
#define KEY_FILE_SIG    0x4E46      // "FN"

// Key Directory Flags
#define KEY_DIR_ROOT (1 << 0)
#define KEY_DIR_READ_ONLY (1 << 1)
#define KEY_DIR_SYSTEM (1 << 2)
#define KEY_DIR_HIDDEN (1 << 3)
#define KEY_DIR_VOLATILE (1 << 4)
#define KEY_DIR_SYMLINK (1 << 5)
#define KEY_DIR_DELETED (1 << 6)

// Key File Flags
#define KEY_FILE_DEFAULT            (1 << 0)
#define KEY_FILE_READ_ONLY          (1 << 1)
#define KEY_FILE_HIDDEN             (1 << 2)
#define KEY_FILE_INLINE             (1 << 3)
#define KEY_FILE_COMPRESSED         (1 << 4)
#define KEY_FILE_VOLATILE           (1 << 10)
#define KEY_FILE_SYMLINK_VOLATILE   (1 << 15)

// Key Type
#define FILE_KEY_NONE           0   // No type / Raw Null
#define FILE_KEY_STRING         1   // String Text (UTF-8 / ASCII)
#define FILE_KEY_DWORD          2   // 32-bit Unsigned Integer (ULONG)
#define FILE_KEY_BINARY         3   // Raw Binary Data / Byte Array Buffer
#define FILE_KEY_MULTI_STRING   4   // Array String (Multiple strings separated by \0)
#define FILE_KEY_QWORD          5   // 64-bit Unsigned Integer (ULONGLONG / Timestamp)
#define FILE_KEY_EXPAND_STRING  6   // String with Environment Variables (%VAR% / $VAR)
#define FILE_KEY_BOOLEAN        7   // Boolean Flag (1 byte: 0 = false, 1 = true)
#define FILE_KEY_GUID           8   // 16-Byte Unique Identifier (UUID / GUID)
#define FILE_KEY_LINK           9   // Only supported for VOLATILE KEY now

//
// Max Volatile Bins
//
#define MAX_VOLATILE_BINS 32

//
// Volatile VK
//
#define VOLATILE_KEY_OFFSET 0x80000000
#define VK_VOLATILE_BIN_MASK    0x0FFF // Masking sisa byte offset

// 
// VK_BIN_SIZE
//
#define VK_BIN_SIZE    0x1000 // 4KB per Bin

//
// VK Disk Block Size
//
#define VK_BLOCK_SIZE  512

//
// Volatile Checker
//
#define VkIsVolatileOffset(Offset)   (((ULONG)(Offset) & VOLATILE_KEY_OFFSET) != 0)
#define VkGetRawOffset(Offset)       ((ULONG)(Offset) & ~VOLATILE_KEY_OFFSET)
#define VkMakeVolatileOffset(Offset) ((ULONG)(Offset) | VOLATILE_KEY_OFFSET)

//
// Max SYMLINK depth is 8
//
#define VK_MAX_SYMLINK_DEPTH 8

// 
// Stable Bin VK Hive
//
typedef struct _VK_STABLE_BIN {
    PVOID  BinBase;        // Alamat awal Bin ini — SEKALI DIALOKASI, GAK PERNAH PINDAH
    ULONG  BinSize;         // Ukuran Bin ini (bin awal = HiveLength dari disk, bin baru = VK_BIN_SIZE)
    ULONG  BinBaseOffset;   // Offset kumulatif awal Bin ini di "ruang virtual" hive
} VK_STABLE_BIN, *PVK_STABLE_BIN;

#define MAX_STABLE_BINS 256

//
// Master Hive Controller
//
typedef struct _VK_HIVE {
    ULONG               Signature;                          // Magic Signature ('VHIV')
    PVOID               BaseAddress;                        // Alamat memori paling awal (KEY_FILE_HEADER)
    ULONG               Length;                             // Total ukuran buffer hive di RAM
    ULONG               FreeOffset;                         // New BIN Free Offset
    PKEY_FILE_HEADER    Header;                             // Pointer langsung ke Header KEY1
    PKEY_BASE_BLOCK     BaseBlock;                          // Pointer ke Blok BIN1 pertama (Offset 0x1000)
    PULONG              DirtyVector;                        // Bitmap tracking alokasi/perubahan block
    ULONG               DirtyVectorSize;                    // Ukuran array DirtyVector
    BOOLEAN             IsReadOnly;                         // Flag status Hive Read-Only
    BOOLEAN             IsDirty;                            // Flag penanda apakah ada data yang belum di-save ke disk
    ANSI_STRING         HiveFilePath;                       // Disk Path tempat Hive disimpan
    VK_STABLE_BIN       StableBins[MAX_STABLE_BINS];
    ULONG               StableBinCount;
    PVOID               VolatileBins[MAX_VOLATILE_BINS];    // Volatile Bins
    ULONG               VolatileBinCount;                   // Volatile Bin count
    ULONG               CurrentBinFreeOffset;               // New Volatile BIN offset 
} VK_HIVE, *PVK_HIVE;

typedef struct _VK_SECKEY
{
    LONG CellSize;            // Basic CellSize
    ULONG Signature;          // 'SK' (0x4B53)
    USHORT Reserved;
    ULONG NextSDOffset;       // HiveOffset to next SD cell
    ULONG ReferenceCount;     // Total of key who share with this SK
    ULONG DescriptorLength;   // SD Payload Size
    UCHAR Revision;             
    UCHAR ControlFlags;       // SE_SELF_RELATIVE (0x8000)
    USHORT DescriptorReserved;
    ULONG OwnerOffset;        // Byte offset dari awal SD ke VEA_SID Owner
    ULONG GroupOffset;        // Byte offset dari awal SD ke VEA_SID Group
    ULONG DaclOffset;         // Byte offset dari awal SD ke VEA_ACL Dacl
    UCHAR Data[1];            // Payload here
} VK_SECKEY, *PVK_SECKEY;

// VkHive Signature
#define VKHIVE_SIG 0x56484B56

// Special Key Object
typedef struct _VK_KEY_OBJECT {
    PVK_HIVE            HiveBase;         // Pointer ke Base Address Hive (RAM/In-Memory File)
    PKEY_DIRECTORY_NODE KeyNode;          // Pointer langsung ke KEY_DIRECTORY_NODE di RAM
    ULONG               KeyOffset;        // Offset relative KeyNode dari awal Base Block
    ANSI_STRING         FullKeyPath;      // Path lengkap (opsional, untuk debugging/lookup)
} VK_KEY_OBJECT, *PVK_KEY_OBJECT;

extern PVK_HIVE VkSystemHive;
extern POBJECT_TYPE VkKeyObjectType;

// Disposition Values
#define REG_CREATED_NEW_KEY     0x00000001
#define REG_OPENED_EXISTING_KEY 0x00000002

// Create Options
#define REG_OPTION_NON_VOLATILE 0x00000000
#define REG_OPTION_VOLATILE     0x00000001

//
// AccessMask
//
#define KEY_QUERY_VALUE         (0x0001)
#define KEY_SET_VALUE           (0x0002)
#define KEY_CREATE_SUB_KEY      (0x0004)
#define KEY_ENUMERATE_SUB_KEYS  (0x0008)
#define KEY_NOTIFY              (0x0010)
#define KEY_CREATE_LINK         (0x0020)

#define KEY_READ_ACCESS         ((VEA_STANDARD_RIGHTS_READ | KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | KEY_NOTIFY) & ~VEA_SYNCHRONIZE)
#define KEY_WRITE_ACCESS        ((VEA_STANDARD_RIGHTS_WRITE | KEY_SET_VALUE | KEY_CREATE_SUB_KEY) & ~VEA_SYNCHRONIZE)
#define KEY_EXECUTE_ACCESS      ((KEY_READ_ACCESS) & ~KEY_NOTIFY)
#define KEY_ALL_ACCESS          ((VEA_STANDARD_RIGHTS_ALL | KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_ENUMERATE_SUB_KEYS | KEY_NOTIFY | KEY_CREATE_LINK) & ~VEA_SYNCHRONIZE)

#define KEY_READ                KEY_READ_ACCESS
#define KEY_WRITE               KEY_WRITE_ACCESS
#define KEY_EXECUTE             KEY_EXECUTE_ACCESS