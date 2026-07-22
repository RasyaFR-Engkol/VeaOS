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

    struct _VIDEO_BOOT
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
