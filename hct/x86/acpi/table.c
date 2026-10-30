#include "Include/table.h"
#include <veakrnl.h>

static PACPI_HEADER HctCachedDsdt = NULL;

PACPI_HEADER
VEAPI
HctFindAcpiTable(const char* Signature)
{
    for (ULONG i = 0; i < HctAcpiTableCount; i++) {
        // Cek 4 huruf signature
        if (HctAcpiCache[i].Signature[0] == Signature[0] &&
            HctAcpiCache[i].Signature[1] == Signature[1] &&
            HctAcpiCache[i].Signature[2] == Signature[2] &&
            HctAcpiCache[i].Signature[3] == Signature[3]) 
        {
            return HctAcpiCache[i].Table;
        }
    }
    return NULL;
}

PVOID
VEAPI
HctGetFadt(IN PVOID Context)
{
    return (PVOID)HctFindAcpiTable("FACP");
}

PVOID
VEAPI
HctGetSsdt(IN PVOID Context, IN ULONG Index)
{
    /* TODO: */
    return NULL;
}

PVOID
VEAPI
HctGetDsdt(IN PVOID Context)
{
    PHCT_ACPI_FADT_TABLE FadtTable;
    ULONG_PTR DsdtPhysical;
    PACPI_HEADER TempHeader;
    ULONG DsdtLength;

    /* 1. Jika sudah pernah di-map, kembalikan dari cache */
    if (HctCachedDsdt != NULL)
    {
        return (PVOID)HctCachedDsdt;
    }

    /* 2. Dapatkan FADT (Karena lokasi DSDT ada di dalam FADT) */
    FadtTable = (PHCT_ACPI_FADT_TABLE)HctGetFadt(Context);
    if (!FadtTable)
    {
        DPRINT("[HCT] ERR: FADT not found, cannot locate DSDT\n");
        return NULL;
    }

    /* 3. Tentukan alamat fisik DSDT (32-bit vs 64-bit) */
    DsdtPhysical = (ULONG_PTR)FadtTable->Dsdt;
    if (FadtTable->AcpiHeader.Revision >= 2 && FadtTable->X_Dsdt != 0)
    {
        DsdtPhysical = (ULONG_PTR)FadtTable->X_Dsdt;
    }

    if (DsdtPhysical == 0)
    {
        DPRINT("[HCT] ERR: DSDT physical address is NULL in FADT\n");
        return NULL;
    }

    /* 4. Map sebagian kecil (hanya Header) untuk membaca Length */
    TempHeader = (PACPI_HEADER)MmMapIoSpace((ULONG)DsdtPhysical, sizeof(ACPI_HEADER), MmCached);
    if (!TempHeader)
    {
        DPRINT("[HCT] ERR: Failed to map DSDT header\n");
        return NULL;
    }

    DsdtLength = TempHeader->Length;
    MmUnmapIoSpace(TempHeader, sizeof(ACPI_HEADER));

    /* Validasi panjang awal */
    if (DsdtLength <= sizeof(ACPI_HEADER))
    {
        DPRINT("[HCT] ERR: Invalid DSDT length: %u\n", DsdtLength);
        return NULL;
    }

    /* 5. Map DSDT secara keseluruhan berdasarkan panjang aslinya */
    HctCachedDsdt = (PACPI_HEADER)MmMapIoSpace((ULONG)DsdtPhysical, DsdtLength, MmCached);
    if (!HctCachedDsdt)
    {
        DPRINT("[HCT] ERR: Failed to map full DSDT\n");
        return NULL;
    }

    /* 6. Validasi Signature 'DSDT' */
    if (HctCachedDsdt->Signature[0] != 'D' || HctCachedDsdt->Signature[1] != 'S' ||
        HctCachedDsdt->Signature[2] != 'D' || HctCachedDsdt->Signature[3] != 'T')
    {
        DPRINT("[HCT] ERR: Invalid DSDT signature mapped\n");
        MmUnmapIoSpace(HctCachedDsdt, DsdtLength);
        HctCachedDsdt = NULL;
        return NULL;
    }

    DPRINT("[HCT] DSDT Mapped Successfully: Phys=0x%p Virt=0x%p Length=%u\n", 
           (PVOID)DsdtPhysical, HctCachedDsdt, DsdtLength);

    return (PVOID)HctCachedDsdt;
}