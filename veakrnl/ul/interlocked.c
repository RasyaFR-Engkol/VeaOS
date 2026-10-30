/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : interlocked.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Interlocked EntryList
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 17-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file interlocked.c
 * --------------------------------------------------------------------- */

FORCEINLINE
BOOLEAN
_UliDisableInterruptAndAcquireSpinlock(
    IN OUT PKSPIN_LOCK Lock
)
{
    BOOLEAN Enabled;

    Enabled = KsDisableInterrupts();
    
    while (__atomic_exchange_n(Lock, 1, __ATOMIC_ACQUIRE)) {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#elif defined(__aarch64__)
        __asm__ volatile("yield" ::: "memory");
#endif
    }

    return Enabled;
}

FORCEINLINE
VOID
_UliReleaseSpinLockAndRestoreInterrupts(
    IN OUT PKSPIN_LOCK Lock,
    BOOLEAN Enable
)
{
    __atomic_store_n(Lock, 0, __ATOMIC_RELEASE);

    KsRestoreInterrupts(Enable);
}

PLIST_ENTRY
VEAFAST
UlfInterlockedInsertTailList(
    IN OUT PLIST_ENTRY ListHead,
    IN PLIST_ENTRY ListEntry,
    IN OUT PKSPIN_LOCK Lock
)
{
    BOOLEAN Enable;
    PLIST_ENTRY LastEntry;

    Enable = _UliDisableInterruptAndAcquireSpinlock(Lock);

    LastEntry = ListHead->Blink;

    InsertTailList(ListHead, ListEntry);

    _UliReleaseSpinLockAndRestoreInterrupts(Lock, Enable);

    return (LastEntry == ListHead) ? NULL : LastEntry;
}