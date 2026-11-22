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

static
VEASTATUS
VEAPI
KspMapExceptionVectorToStatus(
    IN ULONG Vector
)
{
    switch (Vector)
    {
        case 0:  return STATUS_INTEGER_DIVIDE_BY_ZERO;   /* #DE */
        case 3:  return STATUS_BREAKPOINT;               /* #BP */
        case 6:  return STATUS_ILLEGAL_INSTRUCTION;      /* #UD */
        case 13: return STATUS_PRIVILEGED_INSTRUCTION;   /* #GP (atau STATUS_ACCESS_VIOLATION) */
        case 14: return STATUS_ACCESS_VIOLATION;         /* #PF */
        default: return STATUS_UNHANDLED_EXCEPTION;
    }
}

VOID
VEAPI
KsiDispatchException(
    IN PKTRAP_FRAME TrapFrame,
    IN ULONG Vector
)
{
    BOOLEAN IsUserMode = ((TrapFrame->SegCs & 3) == 3);
    ULONG Cr2 = 0;

    /* 1. Tangani Page Fault (Demand Paging & COW) untuk User maupun Kernel */
    if (Vector == KS_EXCEPTION_PAGE_FAULT)
    {
        __asm__ __volatile__("mov %%cr2, %0" : "=r" (Cr2));

        BOOLEAN WriteFault = (TrapFrame->ErrCode & 2) != 0;
        BOOLEAN WasPresent = (TrapFrame->ErrCode & 1) != 0;

        if (MmAccessFault(Cr2, WriteFault, WasPresent))
        {
            return;
        }
    }

    /* 2. Jalur Ring 3 (User Mode): jangan pernah BugCheck kernel! */
    if (IsUserMode)
    {
        VEASTATUS ExitStatus = KspMapExceptionVectorToStatus(Vector);

        // TODO:
        // jika user-mode SEH / KsiUserExceptionDispatcher sudah aktif,
        // lempar konteks ke Ring 3 di sini sebelum memutuskan terminate. 

        DPRINT("[EX/USER] Process '%s' crashed! Vector=%u, EIP=0x%08X, CR2=0x%08X, Err=0x%X -> Terminating (0x%08X)\n",
               ((PEPROCESS)PtGetCurrentProcess())->ImageFileName,
               Vector,
               TrapFrame->Eip,
               Cr2,
               TrapFrame->ErrCode,
               ExitStatus);

        VzTerminateProcess(NtCurrentProcess(), ExitStatus);
        return;
    }

    /* 3. Jalur Ring 0 (Kernel Mode) */
    if (Vector == KS_EXCEPTION_BREAKPOINT)
    {
        if (KdpHandleDebug(TrapFrame, Vector))
        {
            return;
        }
    }

    if (KsiDispatchSehException(TrapFrame, KspMapExceptionVectorToStatus(Vector)))
    {
        return;
    }

    if (Vector == KS_EXCEPTION_PAGE_FAULT)
    {
        KsBugCheckEx(PAGE_FAULT_IN_NONPAGED_AREA, (ULONG_PTR)Cr2, Vector, TrapFrame->ErrCode, 0);
        return;
    }

    KsBugCheckEx(KMODE_EXCEPTION_NOT_HANDLED, Vector, TrapFrame->ErrCode, TrapFrame->Eip, 0);
}

/* EOF */