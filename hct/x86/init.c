#include <veakrnl.h>
#include "apic/apic.h"

VOID
VEAPI
HctInitializeProcessor(PBLOCK_BOOT_2 BlockBoot, ULONG ProcessorNumber)
{
    HctpSetupProcessorIdentity(ProcessorNumber);

    AcpiCachingTableToHct(BlockBoot);

    ApicInitializeSubsystem();
}

VOID
VEAPI
HctDisableLegacyPic(VOID)
{
    //
    // Tulis 0xFF ke Data Port Master (0x21) dan Slave (0xA1) PIC 8259
    // Ini akan mem-mask (mematikan) seluruh 16 IRQ legacy PIC.
    //
    IoWritePortByte(0x21, 0xFF);
    IoWritePortByte(0xA1, 0xFF);
}

BOOLEAN
VEAPI
HctInitSystem(ULONG BootPhase)
{
    switch (BootPhase)
    {
        case 0:
        {
            HctDisableLegacyPic();
            
            ApicInitializeTimerInterrupt();

            ApicInitLazyIrql();

            ApicStartTimer(250);

            return TRUE;
        }

        case 1:
        {
            
        }

        default:
            KsBugCheck(UNEXPECTED_INITIALIZATION_CALL);
            return FALSE;
    }
}