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

VEASTATUS
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
    ULONG SyscallNumber, ParamCount;
    UCHAR ParamBytes;
    ULONG_PTR TargetFunction;
    PULONG_PTR UserArgs;
    PKSERVICE_TABLE_DESCRIPTOR DescriptorTable;

    if (!CurrentThread || !TrapFrame)
    {
        return STATUS_INVALID_PARAMETER;
    }

    SyscallNumber = TrapFrame->Eax;

    OldTrapFrame = CurrentThread->TrapFrame;
    OldPreviousMode = CurrentThread->PreviousMode;
    CurrentThread->TrapFrame = TrapFrame;
    CurrentThread->PreviousMode = (KPROCESSOR_MODE)TrapFrame->PreviousPreviousMode;

    if (SyscallNumber >= KsiServiceLimit)
    {
        Status = STATUS_INVALID_SYSTEM_SERVICE;
        goto Exit;
    }

    DescriptorTable = (PKSERVICE_TABLE_DESCRIPTOR)CurrentThread->ServiceTable;
    TargetFunction = DescriptorTable->Number[SyscallNumber];
    ParamBytes = MainSSPT[SyscallNumber];

    if (!TargetFunction)
    {
        Status = STATUS_NOT_IMPLEMENTED;
        goto Exit;
    }

    ParamCount = ParamBytes / sizeof(ULONG_PTR);
    UserArgs = (PULONG_PTR)TrapFrame->Edx;

    if (ParamCount == 0)
    {
        typedef VEASTATUS (VEAPI *PNATIVE_SYSCALL_0)(VOID);
        Status = ((PNATIVE_SYSCALL_0)TargetFunction)();
    }
    else
    {
        ULONG_PTR KernelArgs[16];

        // Validasi Alamat Stack User jika PreviousMode == UserMode
        if (CurrentThread->PreviousMode == UserMode)
        {
            if ((ULONG_PTR)UserArgs >= 0x80000000 || ((ULONG_PTR)UserArgs + ParamBytes) >= 0x80000000)
            {
                Status = STATUS_ACCESS_VIOLATION;
                goto Exit;
            }
        }

        // Copy argumen dari User Stack ke Stack Kernel
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

        // Panggil handler dengan argumen dari Kernel Stack
        typedef VEASTATUS (VEAPI *PNATIVE_SYSCALL_FN)(
            ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR,
            ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR,
            ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR
        );

        Status = ((PNATIVE_SYSCALL_FN)TargetFunction)(
            KernelArgs[0],  KernelArgs[1],  KernelArgs[2],  KernelArgs[3],
            KernelArgs[4],  KernelArgs[5],  KernelArgs[6],  KernelArgs[7],
            KernelArgs[8],  KernelArgs[9],  KernelArgs[10], KernelArgs[11]
        );
    }
Exit:
    CurrentThread->PreviousMode = OldPreviousMode;
    CurrentThread->TrapFrame = OldTrapFrame;

    return Status;
}