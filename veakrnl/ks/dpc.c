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
    /* Tell PRCB we are workin on DPC now */
    Prcb->DpcRoutineActive = TRUE;

    /* While our list is not empty at all*/
    while(!IsListEmpty(&Prcb->DpcListHead))
    {
        /* Acquire spinlock as safety */
        KsAcquireSpinLockAtDpcLevel(&Prcb->DpcLock);

        /* Recheck. This could be done by another CPU maybe */
        if (IsListEmpty(&Prcb->DpcListHead)) {
            KsReleaseSpinLockFromDpcLevel(&Prcb->DpcLock);
            break;
        }

        /* Pop Head Entry */
        PLIST_ENTRY Entry = RemoveHeadList(&Prcb->DpcListHead);

        /* Get our PKDPC Pointer */
        PKDPC Dpc = CONTAINING_RECORD(Entry, KDPC, DpcListEntry);

        /* Tell them we done */
        Dpc->DpcData = NULL;

        /* Release Spinlock cause we are safe */
        KsReleaseSpinLockFromDpcLevel(&Prcb->DpcLock);

        /* Execute DPC */
        Dpc->DeferredRoutine(
            Dpc,
            Dpc->DeferredContext,
            Dpc->SystemArgument1,
            Dpc->SystemArgument2
        );
    }

    /* Yey we are done */
    Prcb->DpcRoutineActive = FALSE;
}