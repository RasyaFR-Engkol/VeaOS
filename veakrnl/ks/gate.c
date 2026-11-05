/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : gate.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Gate
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 31-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file gate.c
 * --------------------------------------------------------------------- */

VOID
VEAFAST
KsInitializeGate(IN PKGATE Gate)
{
    Gate->Header.Type = GateObject;
    Gate->Header.Signalling = FALSE;
    Gate->Header.Size = sizeof(KGATE) / sizeof(ULONG);
    Gate->Header.SignalState = 0;
    InitializeListHead(&(Gate->Header.WaitListHead));
}

VEASTATUS
VEAFAST
KsWaitForGate(
    IN PKGATE Gate,
    IN KWAIT_REASON WaitReason,
    IN KPROCESSOR_MODE WaitMode
)
{
    PKTHREAD CurrentThread = PtGetCurrentThread();
    ULONG_PTR Flags;

    if (!Gate || !CurrentThread)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Amankan status interrupt
    Flags = __ReadEFlags();
    __Stop(); // cli

    /* 1. Fast Path: Jika Gate sedang Terbuka (SignalState != 0) */
    if (Gate->Header.SignalState != 0)
    {
        Gate->Header.SignalState = 0; // Auto-reset gate jadi tertutup
        __WriteEFlags(Flags);
        return STATUS_SUCCESS;
    }

    /* 2. Slow Path: Gate Tertutup, ubah State ke Waiting & Masukkan ke Antrean Gate */
    CurrentThread->WaitReason = WaitReason;
    CurrentThread->WaitMode = WaitMode;
    CurrentThread->WaitStatus = STATUS_PENDING;
    CurrentThread->State = Waiting;

    InsertTailList(&(Gate->Header.WaitListHead), &(CurrentThread->WaitListEntry));

    /* 3. Panggil PtYieldThread() untuk lepas CPU */
    // Karena State == Waiting, PtYieldThread TIDAK me-requeue thread ini ke CFS RunQueue
    PtYieldThread();

    // Restore interrupt setelah thread dipanggil/dibangunkan kembali
    __WriteEFlags(Flags);

    return CurrentThread->WaitStatus;
}

VOID
VEAFAST
KsSignalGate(IN PKGATE Gate)
{
    ULONG_PTR Flags;
    PLIST_ENTRY Entry;
    PKTHREAD WaitingThread;

    if (!Gate) return;

    Flags = __ReadEFlags();
    __Stop();

    if (!IsListEmpty(&(Gate->Header.WaitListHead)))
    {
        /* Ambil thread terdepan dari antrean Gate */
        Entry = RemoveHeadList(&(Gate->Header.WaitListHead));
        WaitingThread = CONTAINING_RECORD(Entry, KTHREAD, WaitListEntry);

        /* Bangunkan thread & kembalikan ke CFS RunQueue via PtWakeThreadEx */
        PtWakeThreadEx(WaitingThread, STATUS_SUCCESS);
    }
    else
    {
        /* Tidak ada yang ngantre, biarkan Gate terbuka */
        Gate->Header.SignalState = 1;
    }

    __WriteEFlags(Flags);
}

/* EOF */