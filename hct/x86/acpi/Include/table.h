/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : table.h
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : ACPI Table
 ----- Effective since 09-2026 til FOREVER  ------------------------------- */

#pragma once

#include "procbind.h"

/* Revision History ------------------------------------------------------
 * DATE       : 21-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : table.h
 * --------------------------------------------------------------------- */

#pragma pack(push, 1)

/* ACPI RSDP v 1.0 */
typedef struct _HCT_RSDP {
    char Signature[8];      // Harus berisi "RSD PTR "
    UCHAR Checksum;         // Jumlah semua byte (0-19) jika ditambah harus = 0 (mod 256)
    char OemId[6];
    UCHAR Revision;         // 0 untuk ACPI 1.0, 2 untuk ACPI 2.0+
    ULONG RsdtAddress;      // Alamat fisik 32-bit dari RSDT (Tabel utama ACPI)
} HCT_RSDP, *PHCT_RSDP;

/* EXTENDED ACPI RSDP */
typedef struct _HCT_ACPI_RSDP_EXTENDED {
    HCT_RSDP RsdpOld;
    
    // Field ACPI 2.0+ (Hanya valid jika Revision >= 2)
    ULONG  Length;            // Ukuran total struktur RSDP v2+
    ULONGLONG XsdtAddress;       // 64-bit Physical Address XSDT
    UCHAR  ExtendedChecksum;
    UCHAR  Reserved[3];
} HCT_ACPI_RSDP_EXTENDED, *PHCT_ACPI_RSDP_EXTENDED;

/* ACPI Header */
typedef struct _HCT_ACPI_HEADER
{
    CHAR Signature[4];
    ULONG Length;
    UCHAR  Revision;
    UCHAR  Checksum;
    CHAR OemId[6];
    CHAR OemTableId[8];
    ULONG OemRevision;
    CHAR AslCompilerId[4];
    ULONG AslCompilerRevision;
} HCT_ACPI_HEADER, *PHCT_ACPI_HEADER;

typedef struct _ACPI_GAS{
    UCHAR  AddressSpaceId; // 0 = System Memory, 1 = System I/O
    UCHAR  RegisterBitWidth;
    UCHAR  RegisterBitOffset;
    UCHAR  AccessSize;
    ULONG64 Address;
} ACPI_GAS, *PACPI_GAS;

typedef struct _HCT_ACPI_FADT_TABLE
{
    HCT_ACPI_HEADER AcpiHeader;

    ULONG FirmwareControl;
    ULONG Dsdt;
    UCHAR Reserved1;
    UCHAR PreferredPMProfile;
    USHORT SCIInterrupt;
    ULONG SMICMDPort;
    UCHAR AcpiEnable;
    UCHAR AcpiDisable;
    UCHAR S4BiosReq;
    UCHAR PStateControl;
    ULONG PM1AEventBlock;
    ULONG PM1BEventBlock;
    ULONG PM1AControlBlock;
    ULONG PM1BControlBlock;
    ULONG PM2ControlBlock;
    ULONG PM1TimerBlock;
    ULONG GPE0Block;
    ULONG GPE1Block;
    UCHAR PM1EventLength;
    UCHAR PM1ControlLength;
    UCHAR PM2ControlLength;
    UCHAR PMTimerLength;
    UCHAR GPE0Length;
    UCHAR GPE1Length;
    UCHAR GPE1Base;
    UCHAR CStateControl;
    USHORT WorstC2Latency;
    USHORT WorstC3Latency;
    USHORT FlushSize;
    USHORT FlushStride;
    UCHAR DutyOffset;
    UCHAR DutyWidth;
    UCHAR DayAlarm;
    UCHAR MountAlarm;
    UCHAR Century;
    USHORT BootArchitectureFlags;
    UCHAR Reserved2;
    ULONG Flags;
    ACPI_GAS ResetRegister;
    UCHAR ResetValue;
    USHORT ArmBootArch;
    UCHAR MinorVersion;
    ULONG64 X_FirmwareControl;
    ULONG64 X_Dsdt;
    ACPI_GAS X_Pm1aEventBlock;
    ACPI_GAS X_Pm1bEventBlock;
    ACPI_GAS X_Pm1aControlBlock;
    ACPI_GAS X_Pm1bControlBlock;
    ACPI_GAS X_Pm2ControlBlock;
    ACPI_GAS X_PmTimerBlock;
    ACPI_GAS X_Gpe0Block;
    ACPI_GAS X_Gpe1Block;
} HCT_ACPI_FADT_TABLE, *PHCT_ACPI_FADT_TABLE;

typedef struct _HCT_ACPI_SDT_TABLE
{
    HCT_ACPI_HEADER AcpiHeader;
    UCHAR AMLData[];
} HCT_ACPI_SSDT_TABLE, 
  *PHCT_ACPI_SSDT_TABLE,
  HCT_ACPI_DSDT_TABLE,
  *PHCT_ACPI_DSDT_TABLE;
  
#pragma pack(pop)
