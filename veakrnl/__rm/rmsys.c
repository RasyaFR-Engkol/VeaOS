/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : rmsys.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Vea SYSCALL
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 30-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file rmsys.c
 * --------------------------------------------------------------------- */

extern UCHAR MainSSPT[];
extern ULONG MainNumberOfSysCalls;
extern ULONG_PTR MainSSDT[];

extern VEASTATUS VEAPI KsiSystemCallTrampoline(
    IN PVOID TargetFunction,
    IN PULONG_PTR KernelArgs,
    IN ULONG ParamBytes
);
extern VOID VEAPI NORET KsiFastCallExitHandler(IN PKTRAP_FRAME Frame);
extern VOID VEAPI NORET KsiSystemCallReturn(IN PKTRAP_FRAME Frame);

NORET
VOID
VEAPI
RiServiceExit(IN PKTRAP_FRAME TrapFrame)
{
    PKTHREAD CurrentThread = PtGetCurrentThread();

    if (CurrentThread)
    {
        CurrentThread->PreviousMode = (KPROCESSOR_MODE)TrapFrame->PreviousPreviousMode;
    }

    if (TrapFrame->PreviousPreviousMode == UserMode)
    {
        KsiFastCallExitHandler(TrapFrame);
    }
    else
    {
        KsiSystemCallReturn(TrapFrame);
    }
}

NORET
VOID
VEAPI
RiSyscallDispatcherHandler(
    IN PKTRAP_FRAME TrapFrame,
    IN PVOID Context
)
{
    PKTHREAD CurrentThread = PtGetCurrentThread();
    KPROCESSOR_MODE OldPreviousMode;
    PKTRAP_FRAME OldTrapFrame;
    VEASTATUS Status = STATUS_SUCCESS;
    ULONG SyscallNumber;
    ULONG TableIndex;
    ULONG ServiceIndex;
    UCHAR ParamBytes = 0;
    ULONG_PTR TargetFunction = 0;
    PULONG_PTR UserArgs;
    PKSERVICE_TABLE_DESCRIPTOR Descriptor = NULL;

    UNREFERENCED_PARAMETER(Context);

    SyscallNumber = TrapFrame->Eax;
    TableIndex    = (SyscallNumber >> 12) & 0x3;
    ServiceIndex  = SyscallNumber & 0xFFF;

    OldTrapFrame = CurrentThread->TrapFrame;
    OldPreviousMode = CurrentThread->PreviousMode;
    CurrentThread->TrapFrame = TrapFrame;
    CurrentThread->PreviousMode = (KPROCESSOR_MODE)TrapFrame->PreviousPreviousMode;

    if (CurrentThread->PreviousMode == UserMode)
    {
        /* Jalur SYSENTER: KsiFastCallService sudah mengisi HardwareEsp = EDX + 4 (&Arg1) */
        UserArgs = (PULONG_PTR)((ULONG_PTR)TrapFrame->HardwareEsp + sizeof(ULONG_PTR));
    }
    else
    {
        /* Jalur Kernel Vz* wrapper: Edx langsung menunjuk ke &Arg1 */
        UserArgs = (PULONG_PTR)TrapFrame->Edx;
    }

    /* Hanya mendukung Table 0 (veakrnl) dan Table 1 (veapi / Shadow SSDT) */
    if (TableIndex > 1)
    {
        Status = STATUS_INVALID_SYSTEM_SERVICE;
        goto Exit;
    }

    if (SyscallNumber >= 0x1000)
    {
        if ((ULONG_PTR)CurrentThread->ServiceTable == (ULONG_PTR)KsServiceTableDescriptor)
        {
            Status = PtConvertToGuiThread();
            if (!VEA_SUCCESS(Status))
            {
                goto Exit;
            }
        }

        Descriptor = &KsServiceTableDescriptorShadow[TableIndex];
    }
    else if (CurrentThread->ServiceTable != NULL)
    {
        Descriptor = &((PKSERVICE_TABLE_DESCRIPTOR)CurrentThread->ServiceTable)[TableIndex];
    }
    else
    {
        Descriptor = &KsServiceTableDescriptor[TableIndex];
    }

    /* Pastikan descriptor valid dan ServiceIndex tidak melewati batas tabel terkait */
    if (!Descriptor || !Descriptor->Base || !Descriptor->Number || ServiceIndex >= Descriptor->Limit)
    {
        Status = STATUS_INVALID_SYSTEM_SERVICE;
        goto Exit;
    }

    TargetFunction = Descriptor->Base[ServiceIndex];
    ParamBytes     = Descriptor->Number[ServiceIndex];

    if (!TargetFunction || TargetFunction < 0xC0000000)
    {
        Status = STATUS_NOT_IMPLEMENTED;
        goto Exit;
    }

    ULONG_PTR KernelArgs[16];

    if (ParamBytes > sizeof(KernelArgs))
    {
        Status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    if (ParamBytes > 0)
    {
        if (CurrentThread->PreviousMode == UserMode)
        {
            if ((ULONG_PTR)UserArgs >= 0x80000000 || ((ULONG_PTR)UserArgs + ParamBytes) >= 0x80000000)
            {
                Status = STATUS_ACCESS_VIOLATION;
                goto Exit;
            }
        }

        _VEA_TRY
        {
            RtlCopyMemory(KernelArgs, UserArgs, ParamBytes);
        }
        _VEA_EXCEPT(1)
        {
            Status = STATUS_ACCESS_VIOLATION;
            goto Exit;
        }
        _VEA_END
    }

    /* Eksekusi Syscall via Dynamic ASM Trampoline */
    Status = KsiSystemCallTrampoline((PVOID)TargetFunction, KernelArgs, ParamBytes);

Exit:
    CurrentThread->PreviousMode = OldPreviousMode;
    CurrentThread->TrapFrame = OldTrapFrame;

    TrapFrame->Eax = Status;

    RiServiceExit(TrapFrame);
}

