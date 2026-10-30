#include <veakrnl.h>
#include "apic.h"

KIRQL KiCurrentIrql = PASSIVE_LEVEL;
KIRQL KiTprIrql     = PASSIVE_LEVEL;

VOID
VEAPI
ApicInitLazyIrql(VOID)
{
    __asm__ __volatile__("cli");

    // The loader initializes the PCR at PASSIVE_LEVEL; keep all IRQL state aligned.
    KiCurrentIrql = PASSIVE_LEVEL;
    KiTprIrql     = PASSIVE_LEVEL;

    ApicWrite(APIC_REG_TPR, IrqlToApicVector(PASSIVE_LEVEL));
    KeGetPcr()->Irql = PASSIVE_LEVEL;
}

KIRQL
VEAPI
HctRaiseIrql(KIRQL NewIrql)
{
    KIRQL OldIrql = KiCurrentIrql;
    KIRQL IrqlFromPcr = KeGetCurrentIrql();

    if (NewIrql < OldIrql)
    {
        DPRINT("UNSYNCHRONIZED IRQL: %d vs %d\n", OldIrql, IrqlFromPcr);
        KsBugCheck(IRQL_NOT_LESS_OR_EQUAL);
    }

    //
    // 100% LAZY IRQL:
    // Cukup update variabel di RAM! Jangan sentuh TPR APIC sama sekali.
    //
    KiCurrentIrql = NewIrql;
    KeGetPcr()->Irql = KiCurrentIrql;

    return OldIrql;
}

VOID
VEAPI
HctLowerIrql(KIRQL NewIrql)
{
    if (NewIrql > KiCurrentIrql)
    {
        KsBugCheck(IRQL_NOT_GREATER_OR_EQUAL);
    }

    KiCurrentIrql = NewIrql;

    //
    // Saat IRQL TURUN, kita Wajib Sinkronkan Hardware TPR!
    // Jika ada pending Self-IPI di IRR LAPIC, LAPIC akan langsung
    // menembakkannya ke CPU tepat saat TPR ini ditulis.
    //
    KiTprIrql = NewIrql;
    KeGetPcr()->Irql = KiTprIrql;
    ApicWriteIrql(NewIrql);
}

VOID
VEAPI
HctSetIrql(KIRQL Irql)
{
    KiCurrentIrql = Irql;
    KiTprIrql = Irql;
    KeGetPcr()->Irql = KiTprIrql;
    ApicWriteIrql(Irql);
}

KIRQL
VEAPI
HctGetCurrentIrql(VOID)
{
    return KiCurrentIrql;
}

BOOLEAN
VEAPI
HctBeginSystemInterrupt(
    KIRQL InterruptIrql,
    ULONG Vector,
    KIRQL* OldIrql
)
{
    KIRQL CurrentIrql = KiCurrentIrql;
    
    if(CurrentIrql >= InterruptIrql)
    {
        ApicWriteIrql(CurrentIrql);
        KiTprIrql = CurrentIrql;

        ApicEndSystemInterrupt();

        //
        // TODO (Phase 3 - Driver & IOAPIC Support):
        // Tambahkan reverse lookup HalpVectorToIndex untuk membedakan
        // Edge vs Level Triggered dari IOAPIC jika sudah memasang PCI Driver.
        //
        ApicSendSelfIpi((UCHAR)Vector);

        return FALSE;
    }

    *OldIrql = CurrentIrql;
    KiCurrentIrql = InterruptIrql;
    KiTprIrql     = InterruptIrql;
    KeGetPcr()->Irql = InterruptIrql;
    ApicWriteIrql(InterruptIrql);

    return TRUE;
}