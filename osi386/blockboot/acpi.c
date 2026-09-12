#include <osi386.h>

int IsValidRSDP(LPRSDP Rsdp)
{
    uint8_t sum = 0;
    uint8_t *ptr = (uint8_t *)Rsdp;
    int i;
    
    // Checksum RSDP versi 1.0 (20 byte pertama wajib berjumlah 0 kalau di-modulo 256)
    for (i = 0; i < 20; i++) {
        sum += ptr[i];
    }
    return (sum == 0);
}

LPRSDP find_acpi_rsdp(void)
{
    uint16_t* EBDArea = (uint16_t*)0x040E; // Alamat pointer EBDA di BDA (BIOS Data Area)
    uint16_t EBDASeg = *EBDArea;
    uint32_t Offset;
    LPRSDP Rsdp;

    // 1. CARI DI AREA EBDA (Extended BIOS Data Area)
    if(EBDASeg != 0)
    {
        // Konversi Segment 16-bit EBDA ke Physical Address 32-bit (Shift left 4)
        uint32_t ebda_linear_addr = EBDASeg << 4; 

        // Scan 1 KB pertama dari EBDA, dengan kelipatan 16 byte
        for(Offset = 0; Offset < 1024; Offset += 16)
        {
            Rsdp = (LPRSDP)(ebda_linear_addr + Offset);
            
            if(cmemcmp(Rsdp->Signature, "RSD PTR ", 8) == 0) {
                if(IsValidRSDP(Rsdp)) return Rsdp;
            }
        }
    }

    // 2. CARI DI AREA BIOS ROM (0x000E0000 - 0x000FFFFF)
    // Scan dari E0000 sampai FFFFF dengan kelipatan 16 byte
    for(uint32_t Addr = 0x000E0000; Addr < 0x00100000; Addr += 16)
    {
        Rsdp = (LPRSDP)Addr;
        
        if(cmemcmp(Rsdp->Signature, "RSD PTR ", 8) == 0) {
            if(IsValidRSDP(Rsdp)) return Rsdp;
        }
    }

    return NULL; // Gagal nemu ACPI
}

void* AcpiMapRsdpPointer(LPRSDP physical_rsdp)
{
    if (physical_rsdp == NULL) {
        return NULL;
    }

    // Ukuran RSDP untuk ACPI 1.0 adalah 20 bytes.
    // Ukuran untuk ACPI 2.0+ (XSDP) adalah 36 bytes.
    // Kita map aja 36 bytes biar aman dukung dua-duanya. 
    // Toh LmMapIoSpace bakal alokasi minimal 1 Page (4KB).
    void* virtual_rsdp = LmMapIoSpace((uint32_t)physical_rsdp, 36);

    return virtual_rsdp;
}

PVOID MapAcpiTableWithHeader(ULONG_PTR PhysicalAddress) {
    if (!PhysicalAddress) return NULL;

    // STEP 1: Map seukuran ACPI_HEADER dulu (36 byte) buat ngintip Length
    // (Karena Paging itu berbasis Page 4KB, map 36 byte ini otomatis nge-map 1 Page penuh)
    PACPI_HEADER TempHeader = (PACPI_HEADER)LmMapIoSpace(PhysicalAddress, sizeof(ACPI_HEADER));
    if (!TempHeader) return NULL;

    // STEP 2: Baca ukuran asli tabel dari header
    ULONG FullLength = TempHeader->Length;

    // STEP 3: Map ulang seluruh tabel sesuai ukuran aslinya
    return LmMapIoSpace(PhysicalAddress, FullLength);
}

VOID
VEAPI
AcpiMapExtendedPointer(
    LPRSDP VirtualRsdp
)
{
    if (!VirtualRsdp) return;

    if (VirtualRsdp->Revision >= 2)
    {
        PACPI_RSDP_EXTENDED ExtendedRsdp = (PACPI_RSDP_EXTENDED)VirtualRsdp;

        if (ExtendedRsdp->XsdtAddress != 0) 
        {
            // 1. Map Header XSDT
            PACPI_HEADER XsdtHeader = (PACPI_HEADER)LmMapIoSpace((ULONG_PTR)ExtendedRsdp->XsdtAddress, sizeof(ACPI_HEADER));
            if (!XsdtHeader || XsdtHeader->Length == 0) return;

            // 2. Map Full XSDT
            PACPI_HEADER FullXsdt = (PACPI_HEADER)LmMapIoSpace((ULONG_PTR)ExtendedRsdp->XsdtAddress, XsdtHeader->Length);
            
            // 3. Map Child Tables (MADT, FADT, dll)
            ULONG EntryCount = (FullXsdt->Length - sizeof(ACPI_HEADER)) / 8;
            PULONGLONG Entries = (PULONGLONG)((ULONG_PTR)FullXsdt + sizeof(ACPI_HEADER));
            
            for (ULONG i = 0; i < EntryCount; i++) {
                ULONG_PTR ChildPhys = (ULONG_PTR)Entries[i];
                if (ChildPhys != 0) {
                    PACPI_HEADER ChildHeader = (PACPI_HEADER)LmMapIoSpace(ChildPhys, sizeof(ACPI_HEADER));
                    if (ChildHeader && ChildHeader->Length > 0) {
                        // Map full child table dan TIMPA POINTERNYA dengan Virtual Address
                        PVOID ChildVirt = LmMapIoSpace(ChildPhys, ChildHeader->Length);
                        Entries[i] = (ULONGLONG)(ULONG_PTR)ChildVirt; 
                    }
                }
            }

            // 4. Update RSDP biar nunjuk ke Virtual XSDT
            ExtendedRsdp->XsdtAddress = (ULONGLONG)(ULONG_PTR)FullXsdt;
        }
    }
    else
    {
        // ACPI 1.0 (RSDT)
        if (VirtualRsdp->RsdtAddress != 0)
        {
            // 1. Map Header RSDT
            PACPI_HEADER RsdtHeader = (PACPI_HEADER)LmMapIoSpace((ULONG_PTR)VirtualRsdp->RsdtAddress, sizeof(ACPI_HEADER));
            if (!RsdtHeader || RsdtHeader->Length == 0) return;

            // 2. Map Full RSDT
            PACPI_HEADER FullRsdt = (PACPI_HEADER)LmMapIoSpace((ULONG_PTR)VirtualRsdp->RsdtAddress, RsdtHeader->Length);
            
            // 3. Map Child Tables (MADT, FADT, dll)
            ULONG EntryCount = (FullRsdt->Length - sizeof(ACPI_HEADER)) / 4;
            PULONG Entries = (PULONG)((ULONG_PTR)FullRsdt + sizeof(ACPI_HEADER));
            
            for (ULONG i = 0; i < EntryCount; i++) {
                ULONG_PTR ChildPhys = (ULONG_PTR)Entries[i];
                if (ChildPhys != 0) {
                    PACPI_HEADER ChildHeader = (PACPI_HEADER)LmMapIoSpace(ChildPhys, sizeof(ACPI_HEADER));
                    if (ChildHeader && ChildHeader->Length > 0) {
                        // Map full child table dan TIMPA POINTERNYA dengan Virtual Address
                        PVOID ChildVirt = LmMapIoSpace(ChildPhys, ChildHeader->Length);
                        Entries[i] = (ULONG)(ULONG_PTR)ChildVirt; 
                    }
                }
            }

            // 4. Update RSDP biar nunjuk ke Virtual RSDT
            VirtualRsdp->RsdtAddress = (ULONG)(ULONG_PTR)FullRsdt;
        }
    }
    extern PAGE_ENTRY *g_KernelPageDirectory;
    // Mapping LAPIC (Local APIC) dari Fisik 0xFEE00000 ke Virtual 0xFFEE0000
    LmMapVirtualMemory(g_KernelPageDirectory, 0xFEE00000, 0xFEE00000, 1);

    // Mapping IOAPIC (I/O APIC) dari Fisik 0xFEC00000 ke Virtual 0xFFEC0000
    LmMapVirtualMemory(g_KernelPageDirectory, 0xFEC00000, 0xFEC00000, 1);
}