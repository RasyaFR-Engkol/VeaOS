#include <veakrnl.h>

ULONG HctAcpiTableCount = 0;
HCT_ACPI_CACHE_ENTRY HctAcpiCache[HCT_MAX_ACPI_TABLES];

VOID
VEAPI
HctpCacheAllAcpiTables(PACPI_RSDP Rsdp)
{
    PACPI_HEADER RootTable = NULL;
    ULONG EntrySize = 0;     // 4 bytes untuk RSDT, 8 bytes untuk XSDT
    ULONG EntryCount = 0;
    PVOID EntryPointers = NULL;

    if (!Rsdp) {
        KsBugCheck(HARDWARE_NO_ACPI_AVAILABLE);
    }

    if (Rsdp->Revision >= 2 && Rsdp->XsdtAddress != 0) {
        // ACPI 2.0+: Gunakan XSDT
        RootTable = (PACPI_HEADER)(ULONG_PTR)Rsdp->XsdtAddress;
        EntrySize = 8;
    } else {
        // ACPI 1.0: Fallback ke RSDT
        RootTable = (PACPI_HEADER)(ULONG_PTR)Rsdp->RsdtAddress;
        EntrySize = 4;
    }

    EntryCount = (RootTable->Length - sizeof(ACPI_HEADER)) / EntrySize;
    EntryPointers = (PVOID)((ULONG_PTR)RootTable + sizeof(ACPI_HEADER));

    for (ULONG i = 0; i < EntryCount; i++) {
        PACPI_HEADER CurrentTable = NULL;

        // Ekstrak physical address berdasarkan ukuran pointernya
        if (EntrySize == 4) {
            ULONG TableAddress = *((ULONG*)((ULONG_PTR)EntryPointers + (i * 4)));
            CurrentTable = (PACPI_HEADER)(ULONG_PTR)TableAddress;
        } else {
            ULONGLONG TableAddress = *((PULONGLONG)((ULONG_PTR)EntryPointers + (i * 8)));
            CurrentTable = (PACPI_HEADER)(ULONG_PTR)TableAddress;
        }

        // Jika pointer valid dan slot cache masih tersedia
        if (CurrentTable != NULL && HctAcpiTableCount < HCT_MAX_ACPI_TABLES) {
            // Salin signature 4-byte (agar mudah dicari nanti)
            HctAcpiCache[HctAcpiTableCount].Signature[0] = CurrentTable->Signature[0];
            HctAcpiCache[HctAcpiTableCount].Signature[1] = CurrentTable->Signature[1];
            HctAcpiCache[HctAcpiTableCount].Signature[2] = CurrentTable->Signature[2];
            HctAcpiCache[HctAcpiTableCount].Signature[3] = CurrentTable->Signature[3];
            
            // Simpan pointernya
            HctAcpiCache[HctAcpiTableCount].Table = CurrentTable;
            
            HctAcpiTableCount++;
        }
    }
}

VOID
VEAPI
HctpDumpCachedAcpiTable(VOID) 
{
    KdPrintf("HCT: List of all ACPI Cached Table\n\r\n\r");
    
    for(ULONG i = 0; i < HctAcpiTableCount; i++)
    {
        // 1. Buat buffer 5-byte lokal
        char Sig[5];
        Sig[0] = HctAcpiCache[i].Signature[0];
        Sig[1] = HctAcpiCache[i].Signature[1];
        Sig[2] = HctAcpiCache[i].Signature[2];
        Sig[3] = HctAcpiCache[i].Signature[3];
        Sig[4] = '\0'; // <-- KUNCI KESELAMATAN BIAR NGGAK BUMPING MEMORY!

        // 2. Print dalam satu baris bersih
        KdPrintf("%d . Sign: %s . TablePointer: %p\n\r", 
                 i + 1, 
                 Sig, 
                 HctAcpiCache[i].Table);
    }
}

VOID
VEAPI
AcpiCachingTableToHct(PBLOCK_BOOT_2 BlockBoot)
{
    if (!BlockBoot || !BlockBoot->AcpiTable) return;

    PACPI_RSDP Rsdp = (PACPI_RSDP)BlockBoot->AcpiTable;

    // Hitung size asli RSDP (Memperbaiki 0xFFFF)
    if (Rsdp->Revision < 2) {
        BlockBoot->AcpiTableByteSize = 20; 
    } else {
        BlockBoot->AcpiTableByteSize = Rsdp->Length;
    }

    // Jalankan sistem Caching HCT
    HctpCacheAllAcpiTables(Rsdp);
}