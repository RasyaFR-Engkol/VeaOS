// DISKA HEADER
#pragma once

#include <veaddk.h>

typedef struct _DISKA_FDO_EXTENSION {
    PDEVICE_OBJECT Self;
    PDEVICE_OBJECT LowerDeviceObject;   // NVMe / AHCI controller driver di bawah
    PDEVICE_OBJECT PhysicalDeviceObject;// PDO asli dari bus controller

    ULONG          DiskNumber;          // N di HarddiskN
    ULONG          SectorSize;          // 512 / 4096
    ULONGLONG      TotalSectors;
    ULONGLONG      DiskSizeInBytes;

    // This is for /Device/HarddiskX/DRX
    ANSI_STRING    DeviceLink; 

    // This if for /Device/HarddiskX/Partition0
    ANSI_STRING    RawPartitionSymbolicLink;

    // This is for ObioRegisterDeviceInterface
    ANSI_STRING    InterfaceSymbolicLink;

    BOOLEAN        IsStarted;
} DISKA_FDO_EXTENSION, *PDISKA_FDO_EXTENSION;

#define D_ERROR(fmt, ...)   DPRINT("D_ERROR: " fmt, ##__VA_ARGS__)
#define D_INFO(fmt, ...)    DPRINT("D_INFO: " fmt, ##__VA_ARGS__)
#define D_WARNING(fmt, ...) DPRINT("D_WARNING: " fmt, ##__VA_ARGS__)
