#include <veakrnl.h>
#include "apic/apic.h"

VOID
VEAPI
HctEndSystemInterrupt(KIRQL OldIrql)
{
    ApicEndSystemInterrupt();

    HctLowerIrql(OldIrql);
}

VOID
VEAPI
HctRequestSoftwareInterrupt(IN KIRQL Irql)
{
    UCHAR TargetVector;

    // Software Interrupt hanya valid untuk level perangkat lunak (APC_LEVEL atau DISPATCH_LEVEL)
    if (Irql >= CLOCK_LEVEL)
    {
        KdPrintf("HCT: Cannot request software interrupt for hardware IRQL >= CLOCK_LEVEL (%u)\n\r", Irql);
        return;
    }

    // Konversi level IRQL ke Vektor Interupsi APIC
    TargetVector = IrqlToApicVector(Irql);
    
    // If APC_LEVEL, send request for 0x1F instead
    if (Irql == APC_LEVEL)
    {
        TargetVector = 0x1F;
    }

    // Kirimkan Self-IPI ke LAPIC CPU saat ini
    ApicRequestSoftwareInterrupt(TargetVector);
}

/* IRQL Interrupt Handler*/
VOID
VEAPI
HctpDispatcherHandler(IN PKTRAP_FRAME Frame, IN ULONG Stubs)
{
    KIRQL OldIrql;

    // EOI agar timer bisa masuk
    ApicEndSystemInterrupt();

    __Start();
    KsiDispatchInterrupt();
    __Stop();
}

VOID
VEAPI
HctpApcHandler(IN PKTRAP_FRAME Frame, IN ULONG Stubs)
{
    ApicEndSystemInterrupt();

    __Start();

    KsDeliverApc(KernelMode, NULL, Frame);

    __Stop();
}