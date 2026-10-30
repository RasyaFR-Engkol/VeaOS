#include <veakrnl.h>

VOID
VEAPI
PtIdleLoop(VOID)
{   
    // There is nothing for a while now
    for(;;) 
    {
        __Start();
        __Halt();
    }
}

ULONG_PTR
VEAFAST
PtpSaveAndDisableInterrupts(VOID)
{
    ULONG_PTR Flags = __ReadEFlags();
    __asm__ __volatile__ ("cli" ::: "memory");
    return Flags;
}

VOID
VEAFAST
PtpRestoreInterrupts(ULONG_PTR Flags)
{
    __WriteEFlags(Flags);
}
