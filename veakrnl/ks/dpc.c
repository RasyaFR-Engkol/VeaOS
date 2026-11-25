/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : dpc.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Implementation of DPC system
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 02-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file dpc.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
KsInitializeDpc(
    IN PKDPC Dpc,
    IN PKDEFERRED_ROUTINE DeferredRoutine,
    IN PVOID DeferredContext
)
{
    Dpc->Type = DpcObject;
    Dpc->Number = 0;
    Dpc->Importance = MediumImportance;
    Dpc->DeferredContext = DeferredContext;
    Dpc->DeferredRoutine = DeferredRoutine;
    Dpc->DpcData = NULL;
}

BOOLEAN
VEAPI
KsInsertQueueDpc(
    IN PKDPC Dpc,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
    KIRQL OldIrql;
    PKPRCB TargetPrcb;
    BOOLEAN Inserted = FALSE;

    /* IRQL to High Level */
    OldIrql = KeRaiseIrql(HIGH_LEVEL);

#ifndef VEAOS_SMP
    TargetPrcb = KeGetCurrentPrcb();
#else
    TargetPrcb = KeGetPrcbByNumber(Dpc->Number);
#endif

    /* Acquire our spinlock */
    KsAcquireSpinLockAtDpcLevel(&TargetPrcb->DpcLock);

    /* Lets insert DPC data*/
    if(Dpc->DpcData == NULL)
    {
        /* Saving argument */
        Dpc->SystemArgument1 = SystemArgument1;
        Dpc->SystemArgument2 = SystemArgument2;

        Dpc->DpcData = TargetPrcb;

        if (Dpc->Importance == HighImportance) {
            InsertHeadList(&TargetPrcb->DpcListHead, &Dpc->DpcListEntry);
        } else {
            InsertTailList(&TargetPrcb->DpcListHead, &Dpc->DpcListEntry);
        }

        Inserted = TRUE;

        if (!TargetPrcb->DpcRoutineActive && !TargetPrcb->DpcInterruptRequested) {
            TargetPrcb->DpcInterruptRequested = TRUE;

            PKPRCB CurrentPrcb = KeGetCurrentPrcb();

            if (TargetPrcb == CurrentPrcb) {
                /*
                 * Target adalah CPU LOKAL:
                 * Cukup minta Software Interrupt / Pending IRQL untuk DISPATCH_LEVEL.
                 * Saat ISR selesai dan IRQL mau turun, KiRetireDpcList akan kepanggil.
                 */
                HctRequestSoftwareInterrupt(DISPATCH_LEVEL);
            } else {
#ifdef VEAOS_SMP
                /*
                 * Target adalah CPU LAIN (SMP):
                 * Kirim Inter-Processor Interrupt (IPI) ke LAPIC CPU target 
                 * agar CPU tersebut bangun dan mengeksekusi KiRetireDpcList!
                 */
                HalSendIpi(TargetPrcb->SetMember, IPI_DPC_REQUEST);
#endif
            }
        }
    }

    KsReleaseSpinLockFromDpcLevel(&TargetPrcb->DpcLock);
    KeLowerIrql(OldIrql);

    return Inserted;
}

BOOLEAN
VEAPI
KsRemoveQueueDpc(IN OUT PKDPC Dpc)
{
    KIRQL OldIrql;
    PKPRCB TargetPrcb;
    BOOLEAN Removed = FALSE;

    OldIrql = KeRaiseIrql(HIGH_LEVEL);

    // Ambil PRCB tempat DPC ini terdaftar
    TargetPrcb = (PKPRCB)Dpc->DpcData;

    if (TargetPrcb != NULL) {
        KsAcquireSpinLockAtDpcLevel(&TargetPrcb->DpcLock);

        // Pastikan DPC masih ada di antrean saat lock didapatkan
        if (Dpc->DpcData == TargetPrcb) {
            RemoveEntryList(&Dpc->DpcListEntry);
            Dpc->DpcData = NULL;
            Removed = TRUE;
        }

        KsReleaseSpinLockFromDpcLevel(&TargetPrcb->DpcLock);
    }

    KeLowerIrql(OldIrql);
    return Removed;
}

VOID
VEAPI
KsiRetireDpcList(IN PKPRCB Prcb)
{
    ULONG_PTR Flags;

    __asm__ __volatile__("pushf; pop %0" : "=r"(Flags) :: "memory");
    __Stop();

    /* Tell PRCB we are working on DPC now */
    Prcb->DpcRoutineActive = TRUE;

    KsAcquireSpinLockAtDpcLevel(&Prcb->DpcLock);

    /* While our list is not empty at all */
    while (!IsListEmpty(&Prcb->DpcListHead))
    {
        /* Pop Head Entry */
        PLIST_ENTRY Entry = RemoveHeadList(&Prcb->DpcListHead);

        /* Get our PKDPC Pointer */
        PKDPC Dpc = CONTAINING_RECORD(Entry, KDPC, DpcListEntry);

        /* Tell them we are done */
        Dpc->DpcData = NULL;

        /* Release Spinlock & buka interrupt sebelum eksekusi DeferredRoutine */
        KsReleaseSpinLockFromDpcLevel(&Prcb->DpcLock);
        __Start();

        /* Execute DPC */
        Dpc->DeferredRoutine(
            Dpc,
            Dpc->DeferredContext,
            Dpc->SystemArgument1,
            Dpc->SystemArgument2
        );

        /* Matikan interrupt lagi sebelum mengunci DpcLock di iterasi berikutnya */
        __Stop();
        KsAcquireSpinLockAtDpcLevel(&Prcb->DpcLock);
    }

    Prcb->DpcInterruptRequested = FALSE;
    Prcb->DpcRoutineActive = FALSE;

    KsReleaseSpinLockFromDpcLevel(&Prcb->DpcLock);

    /* Kembalikan IF hanya jika sebelumnya memang aktif saat masuk fungsi */
    if (Flags & 0x200)
    {
        __Start();
    }
}

BOOLEAN
VEAPI
KsDpcRoutineRunning(VOID)
{
    return KeGetCurrentPrcb()->DpcRoutineActive;
}
