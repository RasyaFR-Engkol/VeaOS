/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : usercall.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : User Callback
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 21-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file usercall.c
 * --------------------------------------------------------------------- */

static inline VOID
KspWriteSysenterEsp(
    IN ULONG_PTR NewEsp0
)
{
    __asm__ __volatile__ (
        "wrmsr"
        :
        : "c"(MSR_IA32_SYSENTER_ESP),
          "a"((ULONG)(NewEsp0 & 0xFFFFFFFF)),
          "d"(0)
    );
}

NORET
VOID
VEAPI
KspSwitchToUserCallback(
    IN PKCALLOUT_FRAME Frame
)
{
    PKTHREAD CurrentThread = PtGetCurrentThread();
    PKTSSENTRY32 Tss = (PKTSSENTRY32)KeGetPcr()->TSS; /* Sesuaikan jika pointer TSS global */
    ULONG_PTR NewKernelEsp0;

    Frame->SavedTrapFrame     = CurrentThread->TrapFrame;
    Frame->SavedCallbackStack = CurrentThread->CallbackStack;
    Frame->SavedInitialStack  = CurrentThread->InitialStack;
    Frame->SavedEsp0          = Tss ? Tss->Esp0 : (ULONG_PTR)CurrentThread->InitialStack;

    /* Geser batas atas stack kernel ke bawah KCALLOUT_FRAME (align 16-byte) */
    NewKernelEsp0 = ((ULONG_PTR)Frame) & ~0xFUL;

    CurrentThread->CallbackStack = (ULONG_PTR)Frame;
    CurrentThread->InitialStack  = (PVOID)NewKernelEsp0;

    if (Tss)
    {
        Tss->Esp0 = NewKernelEsp0;
    }
    KspWriteSysenterEsp(NewKernelEsp0);

    /* Terbang ke Ring 3 (EIP = UserCallbackDispatcher, ESP = NewStack) */
    RiServiceExit(CurrentThread->TrapFrame);
}

VEASTATUS
VEAPI
KsUserModeCallback(
    IN ULONG RoutineIndex,
    IN PVOID Argument,
    IN ULONG ArgumentLength,
    OUT PVOID *Result,
    OUT PULONG ResultLength
)
{
    PKTHREAD CurrentThread = PtGetCurrentThread();
    PKTRAP_FRAME TrapFrame = CurrentThread->TrapFrame;
    ULONG_PTR OldStack, OldEip, NewStack;
    PVOID ExceptionList = NULL;
    struct _TEB *Teb;
    VEASTATUS CallbackStatus;

    if (!TrapFrame || !KsUserCallbackDispatcher)
    {
        return STATUS_UNSUCCESSFUL;
    }

    OldStack = TrapFrame->HardwareEsp;
    OldEip   = TrapFrame->Eip;

    _VEA_TRY
    {
        /* Alokasikan salinan Argument di stack user-mode (align 4-byte) */
        NewStack = (OldStack - ArgumentLength) & ~3UL;

        if ((NewStack - 24) >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            OldStack >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            _VEA_YIELD(return STATUS_ACCESS_VIOLATION);
        }

        if (Argument && ArgumentLength > 0)
        {
            RtlCopyMemory((PVOID)NewStack, Argument, ArgumentLength);
        }

        /* Susun stack frame masuk KsiUserCallbackDispatcher:
           [NewStack + 0]  = Fake return address (0)
           [NewStack + 4]  = RoutineIndex
           [NewStack + 8]  = Pointer ke salinan Argument di user stack
           [NewStack + 12] = ArgumentLength */
        NewStack -= 24;
        *(PULONG)(NewStack + 0)  = 0;
        *(PULONG)(NewStack + 4)  = RoutineIndex;
        *(PULONG)(NewStack + 8)  = (NewStack + 24);
        *(PULONG)(NewStack + 12) = ArgumentLength;

        Teb = CurrentThread->Teb;
        if (Teb)
        {
            ExceptionList = (PVOID)Teb->NtTib.ExceptionList;
        }

        /* Arahkan TrapFrame ke KsiUserCallbackDispatcher di veaso.so */
        TrapFrame->HardwareEsp = NewStack;
        TrapFrame->Eip         = (ULONG_PTR)KsUserCallbackDispatcher;

        CallbackStatus = KsiCallUserMode(Result, ResultLength);

        if (Teb && CallbackStatus != STATUS_CALLBACK_POP_STACK)
        {
            Teb->NtTib.ExceptionList = ExceptionList;
        }
    }
    _VEA_EXCEPT(1)
    {
        TrapFrame->HardwareEsp = OldStack;
        TrapFrame->Eip         = OldEip;
        return STATUS_ACCESS_VIOLATION;
    }
    _VEA_END

    /* Pulihkan ESP dan EIP user-mode ke titik sebelum callback */
    TrapFrame->HardwareEsp = OldStack;
    TrapFrame->Eip         = OldEip;
    return CallbackStatus;
}

VEASTATUS
VEAPI
VeaCallbackReturn(
    IN PVOID Result OPTIONAL,
    IN ULONG ResultLength,
    IN VEASTATUS CallbackStatus
)
{
    PKTHREAD CurrentThread = PtGetCurrentThread();
    PKCALLOUT_FRAME Frame = (PKCALLOUT_FRAME)CurrentThread->CallbackStack;
    PKTRAP_FRAME TrapFrame;
    PKTSSENTRY32 Tss;

    if (!Frame)
    {
        return STATUS_NO_CALLBACK_ACTIVE;
    }

    /* Setor pointer hasil dari Ring 3 ke variabel output milik pemanggil KeUserModeCallback */
    if (Frame->ResultPtr)
    {
        *(Frame->ResultPtr) = Result;
    }
    if (Frame->ResultLengthPtr)
    {
        *(Frame->ResultLengthPtr) = ResultLength;
    }

    TrapFrame = Frame->SavedTrapFrame;
    Tss = (PKTSSENTRY32)KeGetPcr()->TSS;

    __Stop();

    if (Tss)
    {
        Tss->Esp0 = Frame->SavedEsp0;
    }
    KspWriteSysenterEsp(Frame->SavedEsp0);

    CurrentThread->InitialStack  = Frame->SavedInitialStack;
    CurrentThread->TrapFrame     = TrapFrame;
    CurrentThread->CallbackStack = Frame->SavedCallbackStack;

    __Start();

    /* Unwind stack kernel balik ke dalam KeUserModeCallback */
    KsiCallbackReturn(Frame, CallbackStatus);
    return STATUS_UNSUCCESSFUL;
}

/* EOF */