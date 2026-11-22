/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : apc.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : APC Handling
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 18-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file apc.c
 * --------------------------------------------------------------------- */

/* PRIVATE */

VOID
VEAPI
KsCheckForKernelApcDelivery(VOID)
{
    KIRQL OldIrql;

    if (KeGetCurrentIrql() == PASSIVE_LEVEL)
    {
        OldIrql = KeRaiseIrql(APC_LEVEL);
        KsDeliverApc(KernelMode, 0, 0);
        KeLowerIrql(OldIrql);
    }
    else
    {
        PtGetCurrentThread()->ApcState.KernelApcPending = TRUE;
        HctRequestSoftwareInterrupt(APC_LEVEL);
    }
}

VOID
VEAFAST
KiInsertQueueApc(IN PKAPC Apc)
{
    PKTHREAD Thread = Apc->Thread;
    PKAPC_STATE ApcState;
    KPROCESSOR_MODE ApcMode;
    PLIST_ENTRY ListHead, NextEntry;
    PKAPC QueuedApc;
    VEASTATUS Status;
    BOOLEAN RequestInterrupt = FALSE;
    KIRQL OldIrql;

    if (Apc->ApcStateIndex == InsertApcEnvironment)
    {
        Apc->ApcStateIndex = Thread->ApcStateIndex;
    }

    ApcState = Thread->ApcStatePointer[(UCHAR)Apc->ApcStateIndex];
    ApcMode = Apc->ApcMode;

    if (Apc->NormalRoutine)
    {
        if ((ApcMode != KernelMode) /*&&
            (Apc->KernelRoutine == PsExitSpecialApc)*/)
        {
            Thread->ApcState.UserApcPending = TRUE;

            InsertHeadList(&ApcState->ApcListHead[ApcMode],
                           &Apc->ApcListEntry);
        }
        else
        {
            InsertTailList(&ApcState->ApcListHead[ApcMode],
                           &Apc->ApcListEntry);
        }
    }
    else
    {
        ListHead = &ApcState->ApcListHead[ApcMode];
        NextEntry = ListHead->Blink;
        while (NextEntry != ListHead)
        {
            QueuedApc = CONTAINING_RECORD(NextEntry, KAPC, ApcListEntry);

            if (!QueuedApc->NormalRoutine) break;
            NextEntry = NextEntry->Blink;
        }

        InsertHeadList(NextEntry, &Apc->ApcListEntry);
    }

    if (Thread->ApcStateIndex == Apc->ApcStateIndex)
    {
        if (Thread == PtGetCurrentThread())
        {
            if (ApcMode == KernelMode)
            {
                Thread->ApcState.KernelApcPending = TRUE;

                if (!Thread->SpecialApcDisable)
                {
                    HctRequestSoftwareInterrupt(APC_LEVEL);
                }
            }
        }
        else
        {
            OldIrql = KsAcquireDispatcherLock();

            if (ApcMode == KernelMode)
            {
                Thread->ApcState.KernelApcPending = TRUE;

                if (Thread->State == Running)
                {
                    RequestInterrupt = TRUE;
                }
                else if ((Thread->State == Waiting) && 
                        (Thread->WaitIrql == PASSIVE_LEVEL) && 
                        !(Thread->SpecialApcDisable) &&
                        (!(Apc->NormalRoutine) ||
                         (!(Thread->KernelApcDisable) &&
                          !(Thread->ApcState.KernelApcInProgress))))
                {
                    Status = STATUS_KERNEL_APC;

                    PtWakeThreadEx(Thread, Status);
                }
                /* Thread does not support GATE */
            }
            else if ((Thread->State == Waiting) &&
                     (Thread->WaitMode == UserMode) &&
                     ((Thread->Alertable) ||
                      (Thread->ApcState.UserApcPending)))
            {
                Thread->ApcState.UserApcPending = TRUE;
                Status = STATUS_USER_APC;

                PtWakeThreadEx(Thread, Status);
            }

            KsReleaseDispatcherLock(OldIrql);

            if (RequestInterrupt)
            {
                HctRequestSoftwareInterrupt(APC_LEVEL);
            }
        }
    }
}

/* PUBLIC */

VOID
VEAPI
KsInitializeApc(
    IN PKAPC Apc,
    IN PKTHREAD Thread,
    IN KAPC_ENVIRONMENT TargetEnvironment,
    IN PKKERNEL_ROUTINE KernelRoutine,
    IN PKRUNDOWN_ROUTINE RundownRoutine,
    IN PKNORMAL_ROUTINE NormalRoutine,
    IN KPROCESSOR_MODE Mode,
    IN PVOID Context
)
{
    Apc->Type = ApcObject;
    Apc->Size = sizeof(KAPC);

    if (TargetEnvironment == CurrentApcEnvironment)
    {
        Apc->ApcStateIndex = Thread->ApcStateIndex;
    }
    else
    {
        Apc->ApcStateIndex = TargetEnvironment;
    }

    Apc->Thread = Thread;
    Apc->KernelRoutine = KernelRoutine;
    Apc->RundownRoutine = RundownRoutine;
    Apc->NormalRoutine = NormalRoutine;

    if (NormalRoutine)
    {
        Apc->ApcMode = Mode;
        Apc->NormalContext = Context;
    }
    else
    {
        Apc->ApcMode = KernelMode;
        Apc->NormalContext = NULL;
    }

    Apc->Inserted = FALSE;
}

BOOLEAN
VEAPI
KsInsertQueueApc(
    IN PKAPC Apc,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
    PKTHREAD Thread = Apc->Thread;
    KIRQL OldIrql;
    BOOLEAN State = TRUE;

    KsAcquireSpinLock(&Thread->ApcQueueLock, &OldIrql);

    if (!(Thread->ApcQueueable) || (Apc->Inserted))
    {
        State = FALSE;
    }
    else
    {
        Apc->SystemArgument1 = SystemArgument1;
        Apc->SystemArgument2 = SystemArgument2;
        Apc->Inserted = TRUE;

        KiInsertQueueApc(Apc);
    }

    KsReleaseSpinLock(&Thread->ApcQueueLock, OldIrql);
    return State;
}

VOID
VEAPI
KsDeliverApc(
    IN KPROCESSOR_MODE DeliveryMode,
    IN PKEXCEPTION_FRAME ExceptionFrame OPTIONAL,
    IN PKTRAP_FRAME TrapFrame OPTIONAL
)
{
    PKTHREAD Thread = PtGetCurrentThread();
    PKAPC_STATE ApcState;
    PLIST_ENTRY ListHead, Entry;
    PKAPC Apc;
    PKKERNEL_ROUTINE KernelRoutine;
    PKNORMAL_ROUTINE NormalRoutine;
    PVOID NormalContext;
    PVOID SystemArgument1;
    PVOID SystemArgument2;
    KIRQL OldIrql;

    UNREFERENCED_PARAMETER(ExceptionFrame);

    ApcState = Thread->ApcStatePointer[(UCHAR)Thread->ApcStateIndex];

    KsAcquireSpinLock(&Thread->ApcQueueLock, &OldIrql);

    Thread->ApcState.KernelApcPending = FALSE;

    /* Kernel Mode APC */
    while (!IsListEmpty(&ApcState->ApcListHead[KernelMode]))
    {
        if (Thread->SpecialApcDisable)
        {
            break;
        }

        ListHead = &ApcState->ApcListHead[KernelMode];
        Entry = ListHead->Flink;
        Apc = CONTAINING_RECORD(Entry, KAPC, ApcListEntry);

        if (Apc->NormalRoutine &&
            (Thread->KernelApcDisable || Thread->ApcState.KernelApcInProgress))
        {
            break;
        }

        RemoveEntryList(&Apc->ApcListEntry);
        Apc->Inserted = FALSE;

        KernelRoutine = Apc->KernelRoutine;
        NormalRoutine = Apc->NormalRoutine;
        NormalContext = Apc->NormalContext;
        SystemArgument1 = Apc->SystemArgument1;
        SystemArgument2 = Apc->SystemArgument2;

        KsReleaseSpinLock(&Thread->ApcQueueLock, OldIrql);

        KernelRoutine(
            Apc,
            &NormalRoutine,
            &NormalContext,
            &SystemArgument1,
            &SystemArgument2
        );

        if (NormalRoutine)
        {
            Thread->ApcState.KernelApcInProgress = TRUE;

            // Eksekusi Normal Routine Kernel APC
            NormalRoutine(NormalContext, SystemArgument1, SystemArgument2);

            Thread->ApcState.KernelApcInProgress = FALSE;
        }

        KsAcquireSpinLock(&Thread->ApcQueueLock, &OldIrql);
    }

    /* User Mode APC*/
    if ((DeliveryMode == UserMode) &&
        (!IsListEmpty(&ApcState->ApcListHead[UserMode])) &&
        (Thread->ApcState.UserApcPending))
    {
        Thread->ApcState.UserApcPending = FALSE;

        ListHead = &ApcState->ApcListHead[UserMode];
        Entry = ListHead->Flink;
        Apc = CONTAINING_RECORD(Entry, KAPC, ApcListEntry);

        RemoveEntryList(&Apc->ApcListEntry);
        Apc->Inserted = FALSE;

        KernelRoutine = Apc->KernelRoutine;
        NormalRoutine = Apc->NormalRoutine;
        NormalContext = Apc->NormalContext;
        SystemArgument1 = Apc->SystemArgument1;
        SystemArgument2 = Apc->SystemArgument2;

        KsReleaseSpinLock(&Thread->ApcQueueLock, OldIrql);

        KernelRoutine(
            Apc,
            &NormalRoutine,
            &NormalContext,
            &SystemArgument1,
            &SystemArgument2
        );

        if (NormalRoutine && TrapFrame)
        {
            KsiInitializeUserApc(TrapFrame, NormalRoutine, NormalContext, SystemArgument1, SystemArgument2);
        }

        return;
    }

    KsReleaseSpinLock(&Thread->ApcQueueLock, OldIrql);
}

VOID
VEAPI
KsiInitializeUserApc(
    IN PKTRAP_FRAME TrapFrame,
    IN PKNORMAL_ROUTINE NormalRoutine,
    IN PVOID NormalContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
    ULONG_PTR UserEsp;
    ULONG_PTR NewEsp;
    ULONG FrameSize;
    PKTRAP_FRAME UserTrapFrame;

    if (!TrapFrame) return;

    // Ambil stack pointer User Mode saat ini (align 4-byte)
    UserEsp = TrapFrame->HardwareEsp & ~3;

    // Hitung total alokasi di User Stack:
    // 4 Argumen + 1 KTRAP_FRAME utuh
    FrameSize = (4 * sizeof(ULONG_PTR)) + sizeof(KTRAP_FRAME);
    NewEsp = (UserEsp - FrameSize) & ~3;

    _VEA_TRY
    {
        // Probe & validasi batas alamat User Mode (< 0x80000000)
        if (NewEsp >= 0x80000000 || (NewEsp + FrameSize) >= 0x80000000)
        {
            _VEA_LEAVE;
        }

        // Copy KTRAP_FRAME asli ke atas 4 argumen di User Stack
        UserTrapFrame = (PKTRAP_FRAME)(NewEsp + 4 * sizeof(ULONG_PTR));
        RtlCopyMemory(UserTrapFrame, TrapFrame, sizeof(KTRAP_FRAME));

        // Jika NormalContext NULL (misal saat dipanggil untuk LdrpInit),
        // teruskan UserTrapFrame sebagai argumen pertama (Context)
        PVOID EffectiveContext = NormalContext ? NormalContext : (PVOID)UserTrapFrame;

        // Simpan 4 parameter di dasar stack untuk KsUserApcDispatcher
        *(PULONG_PTR)(NewEsp + 0 * sizeof(ULONG_PTR)) = (ULONG_PTR)NormalRoutine;
        *(PULONG_PTR)(NewEsp + 1 * sizeof(ULONG_PTR)) = (ULONG_PTR)EffectiveContext;
        *(PULONG_PTR)(NewEsp + 2 * sizeof(ULONG_PTR)) = (ULONG_PTR)SystemArgument1;
        *(PULONG_PTR)(NewEsp + 3 * sizeof(ULONG_PTR)) = (ULONG_PTR)SystemArgument2;

        // Arahkan EIP TrapFrame ke User APC Dispatcher (di veaso.so) & set ESP baru
        TrapFrame->Eip = (ULONG_PTR)KsUserApcDispatcher;
        TrapFrame->HardwareEsp = NewEsp;

        // Sanitasi Segment Selectors Ring 3 (SEMUA wajib RPL=3 / DPL=3!) & EFlags (IF=1)
        TrapFrame->SegCs = KGDT32_R3_CODE | 3;
        TrapFrame->SegDs = KGDT32_R3_DATA | 3;
        TrapFrame->SegEs = KGDT32_R3_DATA | 3;
        TrapFrame->HardwareSegSs = KGDT32_R3_DATA | 3;
        TrapFrame->SegFs = KGDT32_R3_TEB; 
        TrapFrame->SegGs = 0;
        TrapFrame->ErrCode = 0;
        TrapFrame->EFlags |= 0x200; // Interrupt Flag ON
    }
    _VEA_EXCEPT(1)
    {
        DPRINT("[APC] Failed to initialize User APC on stack: 0x%08X\n", (ULONG)NewEsp);
    }
    _VEA_END;
}
