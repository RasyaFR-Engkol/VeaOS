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

VOID
VEAPI
PtpCatchCriticalBreak(
    IN PVOID Object,
    IN BOOLEAN IsProcess,
    IN VEASTATUS ExitStatus
)
{
    PEPROCESS Process;
    PETHREAD Thread;
    PCHAR ImageName = "Unknown";

    if (IsProcess)
    {
        Process = (PEPROCESS)Object;
        if (!Process->Settings.BreakOnTermination)
        {
            return;
        }

        if (Process->ImageFileName[0] != '\0')
        {
            ImageName = Process->ImageFileName;
        }

        DPRINT("[PT/CRITICAL] Critical process %p (%s) terminated! Status: 0x%x\n",
               Process, ImageName, ExitStatus);

        /* Jika Kernel Debugger aktif, kasih kesempatan break dulu */
        //if (KdDebuggerEnabled)
        //{
        //    DbgBreakPoint();
        //}

        KsBugCheckEx(
            CRITICAL_PROCESS_DIED,
            0, /* 0 = Process */
            (ULONG_PTR)Process,
            (ULONG_PTR)ImageName,
            (ULONG_PTR)ExitStatus
        );
    }
    else
    {
        Thread = (PETHREAD)Object;
        if (!Thread->Settings.BreakOnTermination)
        {
            return;
        }

        Process = Thread->ThreadProcess;
        if (Process && Process->ImageFileName[0] != '\0')
        {
            ImageName = Process->ImageFileName;
        }

        DPRINT("[PT/CRITICAL] Critical thread %p in process %s terminated! Status: 0x%x\n",
               Thread, ImageName, ExitStatus);

        //if (KdDebuggerEnabled)
        //{
        //    KdpDebugBreak(STATUS_CONTROL_C_EXIT);
        // }

        KsBugCheckEx(
            CRITICAL_PROCESS_DIED,
            1, /* 1 = Thread */
            (ULONG_PTR)Thread,
            (ULONG_PTR)ImageName,
            (ULONG_PTR)ExitStatus
        );
    }
}

/* EOF */