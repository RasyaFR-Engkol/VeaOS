// VEAFAT HEADER

#pragma once
#include <veaddk.h>

#define VEAFAT_POOL_TAG 'taFV'

#define VCB_STATE_MOUNTED       0x00000001
#define VCB_STATE_DIRTY         0x00000002
#define VCB_STATE_READ_ONLY     0x00000004

typedef struct _FAT32_DEVICE_EXTENSION
{
    BOOLEAN             IsCdo;
    PDEVICE_OBJECT      VolumeDeviceObject; // VDO milik VeaFat sendiri
    PDEVICE_OBJECT      TargetDeviceObject; // PDO Storage/Partition di bawahnya (tempat nembak OIP Read/Write)
    PVPB                Vpb;                // Volume Parameter Block terikat

    ULONG               BytesPerSector;     // Sektor size (512, 2048, 4096)
    ULONG               SectorsPerCluster;  // Sektor per cluster (1, 2, 4, 8, 16, 32, 64)
    ULONG               BytesPerCluster;    // BytesPerSector * SectorsPerCluster (Cache kalkulasi)
    
    ULONG               ReservedSectors;    // Jumlah sektor sebelum FAT1
    ULONG               NumberOfFats;       // Jumlah tabel FAT (biasanya 2)
    ULONG               FatSizeSectors;     // FATSz32 (Jumlah sektor per tabel FAT)
    
    ULONG               RootDirStartCluster;// Cluster awal Root Directory (biasanya cluster 2)
    USHORT              FSInfoSector;       // Sektor lokasi FSInfo (biasanya sektor 1)
    USHORT              BackupBootSector;   // Sektor lokasi backup VBR (biasanya sektor 6)
    
    ULONGLONG           FatStartLba;        // LBA Sektor Awal FAT1
    ULONGLONG           DataStartLba;       // LBA Sektor Awal Cluster 2 (Data Area)
    ULONG               TotalClusters;      // Total data cluster yang ada di partisi
    ULONG               TotalSectors;       // Total sektor volume

    ULONG               FreeClusterCount;   // Cached free cluster count (0xFFFFFFFF kalau belum tau)
    ULONG               NextFreeCluster;    // Hint cluster kosong terakhir buat percepat alokasi
    BOOLEAN             FsInfoDirty;        // TRUE jika info free cluster di RAM berubah & belum sync ke disk

    LIST_ENTRY          FcbListHead;        // Linked list seluruh FCB (File Control Block) yang aktif
    KSPIN_LOCK          FcbListLock;        // Lock untuk amanin FcbListHead dari interrupt/multithread
    
    KMUTEX              VolumeMutex;        // Mutex utama untuk serialisasi akses alokasi FAT & Mount
    ULONG               VolumeFlags;        // Flag VCB_STATE_*
} FAT32_DEVICE_EXTENSION, *PFAT32_DEVICE_EXTENSION;

// Convert Cluster Number ke Absolute LBA Sektor
#define FAT32_CLUSTER_TO_LBA(Vcb, Cluster) \
    ((Vcb)->DataStartLba + (ULONGLONG)((Cluster) - 2) * (Vcb)->SectorsPerCluster)

// Convert Byte Offset di dalam File ke Cluster Index
#define FAT32_BYTES_TO_CLUSTERS(Vcb, Bytes) \
    ((ULONG)(((Bytes) + (Vcb)->BytesPerCluster - 1) / (Vcb)->BytesPerCluster))

// Cek apakah Cluster bernilai End-Of-Chain (EOC) di FAT32
#define FAT32_IS_EOF(Cluster) \
    ((Cluster) >= 0x0FFFFFF8)

#define MERGE_CLUSTER_HI_N_LO(high, low) \
    (((unsigned long)(high) << 16) | (unsigned long)(low))

// Attribute berkas FAT
#define FAT_ATTR_READ_ONLY      0x01
#define FAT_ATTR_HIDDEN         0x02
#define FAT_ATTR_SYSTEM         0x04
#define FAT_ATTR_VOLUME_ID      0x08
#define FAT_ATTR_DIRECTORY      0x10
#define FAT_ATTR_ARCHIVE        0x20
#define FAT_ATTR_LFN            (FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID) // 0x0F

#define FAT_LFN_LAST_ENTRY      0x40
#define FAT_LFN_SEQ_MASK        0x1F

#pragma pack (push, 1)

typedef struct _FAT32_VBR {
    UCHAR  JmpBoot[3];
    UCHAR  OemName[8];
    USHORT BytesPerSector;
    UCHAR  SectorsPerCluster;
    USHORT ReservedSectors;
    UCHAR  NumberOfFats;
    USHORT RootDirEntries;      // Nilainya harus 0 di FAT32
    USHORT TotalSectors16;      // Nilainya harus 0 di FAT32
    UCHAR  Media;
    USHORT FatSize16;           // Nilainya harus 0 di FAT32
    USHORT SectorsPerTrack;
    USHORT Heads;
    ULONG  HiddenSectors;
    ULONG  TotalSectors32;
    
    // FAT32 Extended Section
    ULONG  FatSize32;
    USHORT ExtFlags;
    USHORT FSVersion;
    ULONG  RootCluster;
    USHORT FSInfoSector;        // Biasanya sektor 1
    USHORT BackupBootSector;    // Biasanya sektor 6
    UCHAR  Reserved[12];
    UCHAR  DriveNumber;
    UCHAR  Reserved1;
    UCHAR  BootSignature;       // 0x29
    ULONG  VolumeId;
    UCHAR  VolumeLabel[11];
    UCHAR  FileSystemType[8];   // "FAT32   "
    UCHAR  BootCode[420];
    USHORT MagicSignature;      // 0xAA55
} FAT32_VBR, *PFAT32_VBR;

typedef struct _FAT32_FSINFO {
    ULONG  LeadSignature;       // 0x41615252 ("RraA")
    UCHAR  Reserved1[480];
    ULONG  StrucSignature;      // 0x61417272 ("rrAa")
    ULONG  FreeClusterCount;    // 0xFFFFFFFF kalau belum terhitung
    ULONG  NextFreeCluster;     // Hint cluster kosong terakhir
    UCHAR  Reserved2[12];
    ULONG  TrailSignature;      // 0xAA550000
} FAT32_FSINFO, *PFAT32_FSINFO;

typedef struct _FAT_DIR_ENTRY {
    UCHAR  FileName[11];        // 8 byte nama + 3 byte ekstensi
    UCHAR  Attributes;          // 0x01: ReadOnly, 0x02: Hidden, 0x10: Directory, dll.
    UCHAR  NtReserved;
    UCHAR  CreationTimeTenth;
    USHORT CreationTime;
    USHORT CreationDate;
    USHORT LastAccessDate;
    USHORT FirstClusterHigh;    // 16-bit teratas cluster awal
    USHORT WriteTime;
    USHORT WriteDate;
    USHORT FirstClusterLow;     // 16-bit terbawah cluster awal
    ULONG  FileSize;            // Ukuran file dalam byte
} FAT_DIR_ENTRY, *PFAT_DIR_ENTRY;

typedef struct _FAT_LFN_ENTRY {
    UCHAR  SequenceNumber;      // Sequence number (1..20), bit 6 (0x40) = entry LFN terakhir
    WCHAR  Name1[5];            // 5 karakter UTF-16LE pertama
    UCHAR  Attributes;          // Harus 0x0F (FAT_ATTR_LFN)
    UCHAR  Type;                // Reserved, selalu 0x00
    UCHAR  Checksum;            // Checksum dari Short File Name (SFN) 8.3
    WCHAR  Name2[6];            // 6 karakter UTF-16LE berikutnya
    USHORT FirstClusterLow;     // Harus 0x0000
    WCHAR  Name3[2];            // 2 karakter UTF-16LE terakhir
} FAT_LFN_ENTRY, *PFAT_LFN_ENTRY;

#pragma pack (pop)

typedef struct _VEAFAT_FCB {
    PFAT32_DEVICE_EXTENSION Vcb;                // Pointer balik ke VCB (Device Extension)
    ULONG               FirstCluster;           // Cluster awal file
    ULONG               FileSize;               // Ukuran file
    UCHAR               Attributes;             // Attribute file (Directory/Archive/dll)
    ANSI_STRING         FileName;               // Nama file yang udah di-resolve

    // Info lokasi Directory Entry file ini di parent folder (buat update FileSize/Time)
    ULONG               ParentDirCluster;       // Cluster folder induknya
    ULONG               DirEntryByteOffset;     // Offset byte lokasi FAT_DIR_ENTRY-nya

    LONG                RefCount;               // Berapa handle/referensi yang lagi buka file ini
    LIST_ENTRY          FcbListEntry;           // Linked list node buat Vcb->FcbListHead
    KSPIN_LOCK          FcbLock;                // Lock lokal FCB

    BOOLEAN             IsVolumeFcb;
} VEAFAT_FCB, *PVEAFAT_FCB;

typedef struct _VEAFAT_CCB {
    PVEAFAT_FCB         Fcb;                    // Nunjuk ke FCB utamanya
    ULONG               CurrentByteOffset;      // Current read/write offset
    ANSI_STRING         DirectorySearchFilter;  // Buat OIP_MJ_DIRECTORY_CONTROL (misal "*.*")
} VEAFAT_CCB, *PVEAFAT_CCB;

#define EXTENSION_FROM_DEVOBJ(DevObj) (PFAT32_DEVICE_EXTENSION)DevObj->DeviceExtension

VEASTATUS
VEAPI
FatMjFsCtl(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack
);


VOID
VEAPI
FatReferenceFcb(
    IN PVEAFAT_FCB Fcb
);

VOID
VEAPI
FatDereferenceFcb(
    IN PVEAFAT_FCB Fcb
);

VEASTATUS
VEAPI
FatMjCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN POIP Oip,
    IN POIP_STACK OipStack
);

VEASTATUS
FatReadDiskSectors(
    IN PDEVICE_OBJECT TargetDevice,
    IN ULONGLONG Lba,
    IN ULONG SectorCount,
    IN ULONG BytesPerSector,
    OUT PVOID Buffer
);

VOID
FatpFormatSfn(
    IN PANSI_STRING InputName,
    OUT UCHAR Sfn[11]
);

VEASTATUS
VEAPI
FatpGetNextCluster(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN ULONG CurrentCluster,
    OUT PULONG NextCluster
);

VEASTATUS
VEAPI
FatpSearchDirectory(
    IN PFAT32_DEVICE_EXTENSION Vcb,
    IN ULONG DirStartCluster,
    IN PANSI_STRING ComponentName,
    OUT PFAT_DIR_ENTRY OutDirEntry,
    OUT PULONG OutParentCluster,
    OUT PULONG OutEntryOffset
);

UCHAR
FatpCalculateSfnChecksum(
    IN const UCHAR Sfn[11]
);

BOOLEAN
FatpCompareLfnWithAnsi(
    IN PWCHAR LfnBuffer,
    IN PANSI_STRING ComponentName
);

VOID
FatpExtractLfnChars(
    IN PFAT_LFN_ENTRY LfnEntry,
    OUT PWCHAR TargetSlot
);

/* EOF */