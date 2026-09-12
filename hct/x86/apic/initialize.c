#include <veakrnl.h>

#define LAPIC_ID_OFFSET     0x020
#define LAPIC_TPR_OFFSET    0x080 // Task Priority Register (Kunci IRQL!)
#define LAPIC_EOI_OFFSET    0x0B0 // End of Interrupt
#define LAPIC_SIVR_OFFSET   0x0F0 // Spurious Interrupt Vector Register

volatile PULONG ApicLapicBase = NULL;

static VOID
ApicDumpStatus(VOID)
{
    if (ApicLapicBase == NULL) {
        KdPrintf("APIC: LAPIC base not mapped yet\n\r");
        return;
    }

    ULONG LapicId = ApicLapicBase[LAPIC_ID_OFFSET / 4];
    ULONG Tpr = ApicLapicBase[LAPIC_TPR_OFFSET / 4];
    ULONG Eoi = ApicLapicBase[LAPIC_EOI_OFFSET / 4];
    ULONG Sivr = ApicLapicBase[LAPIC_SIVR_OFFSET / 4];

    KdPrintf("APIC: ID=0x%x TPR=0x%x EOI=0x%x SIVR=0x%x\n\r",
             LapicId, Tpr, Eoi, Sivr);
}

VOID 
VEAPI 
ApicEnableLapic(VOID)
{
    if (ApicLapicBase == NULL) 
    {
        KdPrintf("HCT: LAPIC Base Address belum di-map!\n\r");
        return;
    }

    // 1. Ambil Spurious Interrupt Vector Register saat ini
    ULONG Sivr = ApicLapicBase[LAPIC_SIVR_OFFSET / 4]; // Dibagi 4 karena tipe pointernya ULONG (4 byte)

    // 2. Set Bit 8 (APIC Software Enable) dan isi Vector dengan 0xFF
    Sivr |= 0x100; // Bit 8 = 1
    Sivr |= 0x0FF; // Spurious Vector = 255
    ApicLapicBase[LAPIC_SIVR_OFFSET / 4] = Sivr;

    // 3. Reset TPR ke 0 (Setara dengan IRQL PASSIVE_LEVEL di hardware)
    ApicLapicBase[LAPIC_TPR_OFFSET / 4] = 0;

    // Opsional: Kirim sinyal EOI (End of Interrupt) buat ngereset state gantung dari Bootloader
    ApicLapicBase[LAPIC_EOI_OFFSET / 4] = 0;

    ApicDumpStatus();
}

ULONG
VEAPI
ApicFindLapicAddress(VOID)
{
    ULONGLONG ApicMsr = HctReadMsr(IA32_APIC_BASE_MSR);

    if ((ApicMsr & IA32_APIC_BASE_MSR_ENABLE) == 0) {
        ApicMsr |= IA32_APIC_BASE_MSR_ENABLE;
        HctWriteMsr(IA32_APIC_BASE_MSR, ApicMsr);
        
        kdp_print("APIC: Global Enable bit was off. Forced it on via WRMSR.\n\r");
    }

    ULONG PhysicalLapic = (ULONG)(ApicMsr & 0xFFFFF000);
    return PhysicalLapic;
}

VOID
VEAPI
ApicInitializeSubsystem(VOID)
{
    ULONG PhysicalAddr = ApicFindLapicAddress();
    
    // The virtual is already mapped
    ApicLapicBase = (volatile PULONG)PhysicalAddr;

    if(ApicLapicBase == NULL)
    {
        kdp_print("APIC: Failed mapping MMIO LAPIC to VAD!\n\r");
        return;
    }

    KdPrintf("APIC: mapped LAPIC at %p (phys 0x%x)\n\r", ApicLapicBase, PhysicalAddr);
    ApicEnableLapic();
}