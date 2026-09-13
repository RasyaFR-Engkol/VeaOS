#pragma once
#include "obtype.h"
#include <procbind.h>

// ACPI
#pragma pack(push, 1)

typedef struct _ACPI_HEADER {
    CHAR  Signature[4];       // Misal: "RSDT", "XSDT", "APIC"
    ULONG Length;             // <--- UKURAN ASLI TABEL DALAM BYTE!
    UCHAR Revision;
    UCHAR Checksum;
    CHAR  OemId[6];
    CHAR  OemTableId[8];
    ULONG OemRevision;
    ULONG CreatorId;
    ULONG CreatorRevision;
} ACPI_HEADER, *PACPI_HEADER;

typedef struct _ACPI_RSDP {
    CHAR  Signature[8];       // Must be "RSD PTR "
    UCHAR Checksum;
    CHAR  OemId[6];
    UCHAR Revision;           // 0 = ACPI 1.0, 2 = ACPI 2.0+
    ULONG RsdtAddress;        // 32-bit Physical Address RSDT
    
    // Field ACPI 2.0+ (Hanya valid jika Revision >= 2)
    ULONG  Length;            // Ukuran total struktur RSDP v2+
    ULONGLONG XsdtAddress;       // 64-bit Physical Address XSDT
    UCHAR  ExtendedChecksum;
    UCHAR  Reserved[3];
} ACPI_RSDP, *PACPI_RSDP;

#pragma pack(pop)

typedef struct _HCT_ACPI_CACHE_ENTRY {
    CHAR Signature[4];      // Nama tabel, misal: "APIC" (untuk MADT), "FACP"
    PACPI_HEADER Table;     // Pointer ke header tabel aslinya di memory
} HCT_ACPI_CACHE_ENTRY, *PHCT_ACPI_CACHE_ENTRY;
#define HCT_MAX_ACPI_TABLES 32

extern HCT_ACPI_CACHE_ENTRY HctAcpiCache[HCT_MAX_ACPI_TABLES];
extern ULONG HctAcpiTableCount;

typedef enum _HCT_PDO_TYPE {
    HctDeviceRootPdo = 1,
    HctDevicePciHostBridge,
    HctDeviceSystemBoard
} HCT_PDO_TYPE;

typedef struct _HCT_PDO_EXTENSION {
    PDEVICE_OBJECT  Self;           // Back-pointer ke DEVICE_OBJECT milik dirinya sendiri
    HCT_PDO_TYPE    PdoType;        // Jenis PDO
    PCHAR           HardwareID;     // Hardware ID PnP (misal: "ACPI_HAL\PNP0A08")
    
    // Pointer ke Static ACPI Tables yang di-cache HCT
    PACPI_HEADER    MadtTable;
    PACPI_HEADER    McfgTable;
    PACPI_HEADER    FadtTable;

    // Status State
    BOOLEAN         IsStarted;
} HCT_PDO_EXTENSION, *PHCT_PDO_EXTENSION;