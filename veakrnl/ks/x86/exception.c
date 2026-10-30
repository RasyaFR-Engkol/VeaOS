#include <veakrnl.h>

VOID
VEAPI
HctInitializeDoubleFault(VOID);

VOID
VEAPI
KsiDispatchException(
    IN PKTRAP_FRAME TrapFrame,
    IN ULONG Vector
);

VOID
VEAPI
ks_initialize_exception(VOID)
{
    HctInitializeIsr();

    for (ULONG i = 0; i < 0x19; i++)
    {
        HctRegisterInterruptToIsr(i, KsiDispatchException);
    }

    /* Initialize Double Fault */
    HctInitializeDoubleFault();
}

VOID
VEAPI
KsiDispatchException(
    IN PKTRAP_FRAME TrapFrame,
    IN ULONG Vector
)
{
    if (KsiDispatchSehException(TrapFrame, STATUS_ACCESS_VIOLATION))
    {
        return;
    }

    if (Vector == KS_EXCEPTION_PAGE_FAULT)
    {
        BOOLEAN Success = FALSE;
        ULONG Cr2;

        __asm__ __volatile__(
            "mov %%cr2, %0"
            : "=r" (Cr2)
        );

        // Pakai ErrCode dari KTRAP_FRAME
        BOOLEAN WriteFault = (TrapFrame->ErrCode & 2) != 0;
        BOOLEAN WasPresent = (TrapFrame->ErrCode & 1) != 0;
        
        if (MmAccessFault(Cr2, WriteFault, WasPresent))
        {
            return; // Page fault ter-resolve, IRET ke instruksi sebelumnya
        }

        KsBugCheckEx(PAGE_FAULT_IN_NONPAGED_AREA, (ULONG_PTR)Cr2, Vector, TrapFrame->ErrCode, 0);
        return;
    }
    else if (Vector == KS_EXCEPTION_GENERAL_PROTECTION && (TrapFrame->SegCs & RPL_MASK) == MODE_USER)
    {
        // Handle #GP yang disebabkan userland (SegCs padanan dari Cs)
    }
    else if (Vector == KS_EXCEPTION_BREAKPOINT)
    {
        // Pass TrapFrame ke Kdp
        BOOLEAN Handled = 0; //KdpHandleDebug(TrapFrame);
        if (Handled)
        {
            return;
        }
    }

    KsBugCheckEx(KMODE_EXCEPTION_NOT_HANDLED, Vector, TrapFrame->ErrCode, 0, 0);
}