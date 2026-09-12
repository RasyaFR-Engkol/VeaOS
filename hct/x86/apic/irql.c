#include <veakrnl.h>
#include "apic.h"

KIRQL KiCurrentIrql = HIGH_LEVEL;
KIRQL KiTprIrql     = HIGH_LEVEL;

VOID
VEAPI
ApicInitLazyIrql(VOID)
{
    __asm__ __volatile__("cli");

    KiCurrentIrql = HIGH_LEVEL;
    KiTprIrql     = HIGH_LEVEL;

    ApicWrite(APIC_REG_TPR, IrqlToApicVector(HIGH_LEVEL));
}

KIRQL
VEAPI
HctRaiseIrql(KIRQL NewIrql)
{
    KIRQL OldIrql = KiCurrentIrql;

    if (NewIrql < OldIrql)
    {
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
    ApicWriteIrql(NewIrql);
    KiTprIrql = NewIrql;
    KeGetPcr()->Irql = KiTprIrql;
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
    ApicWriteIrql(InterruptIrql);

    return TRUE;
}