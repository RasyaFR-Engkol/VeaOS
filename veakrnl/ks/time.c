/* VeaOS SPDX License ----------------------------------------------------
    SPDX-License-Identifier: GPL-2.0-only
    LICENSE     : GNU General Public License v2.0
    FILE        : time.c
    CREATOR     : RasyaFR-Engkol
    MAINTAINER  : VeaOS Team
    PURPOSE     : Part of KS used to handling system time and another related
                  to time system
----- Effective since 07-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
    * DATE          : 26-07-2026
    * AUTHOR        : Rasya Fadhillah R
    * REVISION      : Creating file veakrnl/ks/time.c
* --------------------------------------------------------------------- */

// EXTERN
VOID
VEAPI
KsiCheckTimerExpiration(
    IN ULONGLONG CurrentSystemTime
);

// Variable used to save System Time Data
ULONGLONG KsTickCount = 0;
ULONGLONG KsSystemTime = 0;
LIST_ENTRY KsTimerListHead;
KDPC KiTimerExpireDpc;
KSPIN_LOCK KsTimerListLock;

ULONGLONG
VEAPI
KsiAdvanceSystemTime(
    IN ULONG Increment
)
{
    // Tambahkan durasi tick ke waktu sistem absolut
    KsSystemTime += Increment;

    return KsSystemTime;
}

ULONGLONG
VEAPI
KsQuerySystemTime(VOID)
{
    // Kembalikan system time
    return KsSystemTime;
}

VOID
VEAFAST
KsUpdateSystemTime(
    IN PKREGISTER_FRAME RegisterFrame,
    IN ULONG Increment,
    IN KIRQL Irql
)
{
    PKPCR CurrentPcr = KeGetPcr();
    ULONGLONG NewSystemTime;
    
    // Change system time now
    KsTickCount += Increment;

    // Update System Time
    NewSystemTime = KsiAdvanceSystemTime(Increment);

    // Run routine for timer expiration
    KsiCheckTimerExpiration(NewSystemTime);
}

VOID
VEAPI
KsiInitializeTimerList(VOID)
{
    // Initialize ListHead
    InitializeListHead(&KsTimerListHead);
    KsInitializeSpinlock(&KsTimerListLock);
}

VOID
VEAPI
KsInitializeTimer(
    OUT PKTIMER Timer
)
{
    // Zero-init supaya semua field, termasuk Active, dalam state bersih
    RtlZeroMemory(Timer, sizeof(KTIMER));
    Timer->Active = FALSE;
}

/*
 * Insert Timer ke KsTimerListHead, posisi diurutkan ascending
 * berdasarkan DueTime. Dipanggil di dalam KsSetTimer dan
 * KsiCheckTimerExpiration (untuk re-arm periodic timer).
 * Caller wajib sudah di DISPATCH_LEVEL.
 */
static
VOID
VEAPI
KsiInsertTimerSorted(
    IN PKTIMER Timer
)
{
    PLIST_ENTRY Entry;
    PKTIMER ExistingTimer;

    // Cari posisi pertama yang DueTime-nya lebih besar dari Timer
    for (Entry = KsTimerListHead.Flink;
         Entry != &KsTimerListHead;
         Entry = Entry->Flink)
    {
        ExistingTimer = CONTAINING_RECORD(Entry, KTIMER, TimerListEntry);
        if (ExistingTimer->DueTime.QuadPart > Timer->DueTime.QuadPart)
        {
            break;
        }
    }

    // Sisipkan Timer sebelum Entry (atau di akhir kalau Entry == head)
    Timer->TimerListEntry.Flink = Entry;
    Timer->TimerListEntry.Blink = Entry->Blink;
    Entry->Blink->Flink = &Timer->TimerListEntry;
    Entry->Blink = &Timer->TimerListEntry;
}

BOOLEAN
VEAPI
KsSetTimer(
    IN OUT PKTIMER Timer,
    IN LARGE_INTEGER DueTime,
    IN LONG Period OPTIONAL,
    IN PKDPC Dpc OPTIONAL
)
{
    KIRQL OldIrql;
    BOOLEAN WasActive;
    ULONGLONG CurrentSystemTime;

    OldIrql = KeRaiseIrql(DISPATCH_LEVEL);

    // Kalau timer sudah armed, cabut dulu dari list sebelum re-arm
    WasActive = Timer->Active;
    if (WasActive)
    {
        RemoveEntryList(&Timer->TimerListEntry);
    }

    // Konversi relative DueTime ke absolute
    CurrentSystemTime = KsQuerySystemTime();
    if (DueTime.QuadPart < 0)
    {
        Timer->DueTime.QuadPart = CurrentSystemTime - DueTime.QuadPart;
    }
    else
    {
        Timer->DueTime.QuadPart = DueTime.QuadPart;
    }

    Timer->Period = Period;
    Timer->Dpc = Dpc;
    Timer->Active = TRUE;

    KsiInsertTimerSorted(Timer);

    KeLowerIrql(OldIrql);

    return WasActive;
}

BOOLEAN
VEAPI
KsCancelTimer(
    IN OUT PKTIMER Timer
)
{
    KIRQL OldIrql;
    BOOLEAN WasActive;

    OldIrql = KeRaiseIrql(DISPATCH_LEVEL);

    WasActive = Timer->Active;
    if (WasActive)
    {
        RemoveEntryList(&Timer->TimerListEntry);
        Timer->Active = FALSE;
    }

    KeLowerIrql(OldIrql);

    return WasActive;
}

VOID
VEAPI
KsiCheckTimerExpiration(
    IN ULONGLONG CurrentSystemTime
)
{
    PLIST_ENTRY Entry;
    PKTIMER Timer;

    while (!IsListEmpty(&KsTimerListHead))
    {
        Entry = KsTimerListHead.Flink;
        Timer = CONTAINING_RECORD(Entry, KTIMER, TimerListEntry);

        // Timer pertama belum due, berarti sisanya juga belum (sorted)
        if (Timer->DueTime.QuadPart > CurrentSystemTime)
        {
            break;
        }

        RemoveEntryList(&Timer->TimerListEntry);
        Timer->Active = FALSE;

        // Queue DPC-nya kalau ada, biar diproses di luar interrupt context
        if (Timer->Dpc != NULL)
        {
            /* TODO: Insert Queue DPC. Uncommment this code 
               if DPC part is done. */
            KsInsertQueueDpc(Timer->Dpc, NULL, NULL);
        }

        // Re-arm otomatis kalau periodic timer
        if (Timer->Period > 0)
        {
            Timer->DueTime.QuadPart = CurrentSystemTime + ((ULONGLONG)Timer->Period * 10000ULL);
            Timer->Active = TRUE;
            KsiInsertTimerSorted(Timer);
        }
    }
}

VOID
VEAPI
KsiTimerExpirationDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
    PLIST_ENTRY Entry;
    PKTIMER Timer;
    KIRQL OldIrql;
    ULONGLONG CurrentSystemTime = KsQuerySystemTime();

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(DeferredContext);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    KsAcquireSpinLockAtDpcLevel(&KsTimerListLock);

    while (!IsListEmpty(&KsTimerListHead))
    {
        Entry = KsTimerListHead.Flink;
        Timer = CONTAINING_RECORD(Entry, KTIMER, TimerListEntry);

        if (Timer->DueTime.QuadPart > CurrentSystemTime)
        {
            break;
        }

        RemoveEntryList(&Timer->TimerListEntry);
        Timer->Active = FALSE;

        if (Timer->Dpc != NULL)
        {
            KsInsertQueueDpc(Timer->Dpc, SystemArgument1, SystemArgument2);
        }

        if (Timer->Period > 0)
        {
            Timer->DueTime.QuadPart = CurrentSystemTime + ((ULONGLONG)Timer->Period * 10000ULL);
            Timer->Active = TRUE;
            KsiInsertTimerSorted(Timer);
        }
    }

    KsReleaseSpinLockFromDpcLevel(&KsTimerListLock);
}