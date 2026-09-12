#pragma once

#include "procbind.h"

typedef enum _VEA_MEMORY_TYPE {
    VeaMemoryTypeUsable          = 1, // RAM kosong, siap dipakai kernel
    VeaMemoryTypeReserved        = 2, // Dipakai hardware/BIOS, JANGAN disentuh!
    VeaMemoryTypeAcpiReclaimable = 3, // Tabel ACPI (bisa dihapus kalau udah selesai dibaca)
    VeaMemoryTypeAcpiNvs         = 4, // ACPI Non-Volatile Storage (wajib dipertahankan)
    VeaMemoryTypeBadMemory       = 5  // Sektor RAM rusak
} VEA_MEMORY_TYPE;

typedef struct _VEA_MEMORY_DESCRIPTOR {
    unsigned long long BaseAddress;
    unsigned long long Length;
    unsigned int       Type;
    unsigned int       ExtendedAttributes;
} __attribute__((packed)) VEA_MEMORY_DESCRIPTOR, *PVEA_MEMORY_DESCRIPTOR;

/* Block boot versi 1.0*/
typedef struct _BLOCK_BOOT_1
{
    ULONG Size;
    ULONG Version;

    struct VIDEO_BOOT
    {
        PVOID VideoBootAddress;
        ULONG X, Y, W, H;
        ULONG Sprite;
        PVOID AdditionalInformation[5];
    } VideoBoot;

    PVOID AcpiTable;
    ULONG AcpiTableByteSize;

    UINT MemoryMapCount;
    PVEA_MEMORY_DESCRIPTOR MemoryMap;
} BLOCK_BOOT_1, *PBLOCK_BOOT_1;

#pragma pack(push, 1)

typedef struct _VIDEO_BOOT
{
    PVOID VideoBootAddress;
    ULONG X, Y, W, H;
    ULONG Sprite;
    PVOID AdditionalInformation[5];
} VIDEO_BOOT, *PVIDEO_BOOT;

//
// 1. SMBIOS 2.x Entry Point Structure (Magic: "_SM_")
//
typedef struct _SMBIOS_ENTRY_POINT {
    CHAR AnchorString[4];             // "_SM_"
    UCHAR Checksum;                   // Checksum seluruh EPS
    UCHAR Length;                     // Ukuran struktur ini (biasanya 0x1F)
    UCHAR MajorVersion;               // Versi SMBIOS (misal: 2)
    UCHAR MinorVersion;               // Versi SMBIOS (misal: 8)
    USHORT MaxStructureSize;          // Ukuran struktur terbesar di SMBIOS
    UCHAR EntryPointRevision;         // EPS Revision
    UCHAR FormattedArea[5];           // Reserved
    
    // Intermediate Anchor Header (DMI)
    CHAR IntermediateAnchorString[5]; // "_DMI_"
    UCHAR IntermediateChecksum;       // Checksum bagian DMI
    USHORT TableLength;               // Total panjang seluruh tabel SMBIOS (Bytes)
    ULONG TableAddress;               // Alamat Fisik Tabel SMBIOS di memori!
    USHORT NumberOfStructures;        // Jumlah total struktur/tabel SMBIOS
    UCHAR BcdRevision;                // BCD Revision
} SMBIOS_ENTRY_POINT, *PSMBIOS_ENTRY_POINT;

//
// 2. SMBIOS Header untuk Setiap Tabel (Type 0, Type 1, Type 4, dst)
//
typedef struct _SMBIOS_HEADER {
    UCHAR Type;                       // Type 0 = BIOS, Type 1 = System, Type 4 = CPU, dll.
    UCHAR Length;                     // Ukuran header + field terformat
    USHORT Handle;                    // Unique handle
} SMBIOS_HEADER, *PSMBIOS_HEADER;

typedef enum _BOOT_DEVICE_TYPE {
    BootDeviceTypeUnknown = 0,
    BootDeviceTypeMbr,
    BootDeviceTypeGpt,
    BootDeviceTypeUefiPath
} BOOT_DEVICE_TYPE;

typedef struct _BOOT_DEVICE_IDENTIFIER {
    BOOT_DEVICE_TYPE Type;
    union {
        struct {
            ULONG DiskSignature;     // MBR 4-byte Signature
            ULONG PartitionNumber;   // Partition Index
        } Mbr;
        struct {
            GUID PartitionGuid;      // GPT Unique Partition GUID
        } Gpt;
        struct {
            PVOID DevicePathBuffer;  // Pointer ke raw EFI_DEVICE_PATH
            ULONG DevicePathLength;
        } Uefi;
    } u;
} BOOT_DEVICE_IDENTIFIER, *PBOOT_DEVICE_IDENTIFIER;

typedef struct _BLOCK_BOOT_2
{
    ULONG Size;
    ULONG Version;
    ULONG OsMjVersion;
    ULONG OsMnVersion;
    VIDEO_BOOT VideoBoot;
    PVOID AcpiTable;
    ULONG AcpiTableByteSize;
    PSMBIOS_ENTRY_POINT SmbiosTable;
    UINT MemoryMapCount;
    PVEA_MEMORY_DESCRIPTOR MemoryMap;
    ULONG_PTR KernelStack;
    ULONG_PTR Prcb;
    ULONG_PTR Process;
    ULONG_PTR Thread;
    PVOID SystemHiveBase;
    ULONG SystemHiveLength;
    LIST_ENTRY BootDriverList;
    PSTR VeaBootArgument;
    PSTR VeaBootDriveOption;
    PVOID KernelBase;
    ULONG KernelSize;
    BOOT_DEVICE_IDENTIFIER BootPartitionSignature;
} BLOCK_BOOT_2, *PBLOCK_BOOT_2;

#pragma pack(pop)