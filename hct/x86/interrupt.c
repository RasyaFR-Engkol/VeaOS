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

    // Kirimkan Self-IPI ke LAPIC CPU saat ini
    ApicRequestSoftwareInterrupt(TargetVector);
}

/* IRQL Interrupt Handler*/
VOID
VEAPI
HctpDispatcherHandler(IN PKREGISTER_FRAME Frame)
{
    KIRQL OldIrql;

    // Kita pake mekanisme LazyIrql jadi kita harus BeginSystemInterrupt
    if(!HctBeginSystemInterrupt(DISPATCH_LEVEL, IrqlToApicVector(DISPATCH_LEVEL), &OldIrql))
    {
        // SPURIOUS
        return;
    }

    // EOI agar timer bisa masuk
    ApicEndSystemInterrupt();

    asm volatile("sti");
    KsiDispatchInterrupt();
    asm volatile("cli");

    HctEndSystemInterrupt(OldIrql);
}

VOID
VEAPI
HctpApcHandler(IN PKREGISTER_FRAME Frame)
{

}