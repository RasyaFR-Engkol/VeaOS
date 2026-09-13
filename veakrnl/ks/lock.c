/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : lock.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Spinlock
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 02-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file lock.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
KsInitializeSpinlock(IN PKSPIN_LOCK Lock)
{
    __atomic_store_n(Lock, 0, __ATOMIC_RELEASE);
}

VOID
VEAPI
KsAcquireSpinLockInternal(IN PKSPIN_LOCK Lock, OUT PKIRQL OldIrql, const PCHAR Function, const PCHAR File, INT Line)
{

#if defined(SPINLOCK_DBG)
    KdPrintf("[Ks] Function %s from %s and %d line try to aquire spinlock.\n", Function, File, Line);
#endif

    /*  Raise IRQL terlebih dahulu untuk mencegah preemption di CPU lokal */
    if (KeGetCurrentIrql() < DISPATCH_LEVEL)
        *OldIrql = KeRaiseIrql(DISPATCH_LEVEL);
    else
        *OldIrql = KeGetCurrentIrql();

    /*  Acquire lock (Test-and-Set) */
    while (__atomic_exchange_n(Lock, 1, __ATOMIC_ACQUIRE)) {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#elif defined(__aarch64__)
        __asm__ volatile("yield" ::: "memory");
#endif
    }
}

VOID
VEAPI
KsReleaseSpinLockInternal(IN PKSPIN_LOCK Lock, IN KIRQL OldIrql, const PCHAR Function, const PCHAR File, INT Line)
{
#if defined(SPINLOCK_DBG)
    KdPrintf("[Ks] Function %s from %s and %d line try to aquire spinlock.\n", Function, File, Line);
#endif
    /* Lepas lock TERLEBIH DAHULU */
    __atomic_store_n(Lock, 0, __ATOMIC_RELEASE);

    /* Baru turunkan IRQL ke level semula */
    if (OldIrql < DISPATCH_LEVEL)
        KeLowerIrql(OldIrql);
}

VOID
VEAPI
KsAcquireSpinLockAtDpcLevelInternal(IN OUT PKSPIN_LOCK Lock, const PCHAR Function, const PCHAR File, INT Line)
{
#if defined(SPINLOCK_DBG)
    KdPrintf("[Ks] Function %s from %s and %d line try to aquire spinlock.\n", Function, File, Line);
#endif
    // Safety check khusus Debug build (sangat direkomendasikan!)
    // VE_ASSERT(KeGetCurrentIrql() >= DISPATCH_LEVEL);

    /* Langsung kunci tanpa utak-atik IRQL */
    while (__atomic_exchange_n(Lock, 1, __ATOMIC_ACQUIRE)) {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#elif defined(__aarch64__)
        __asm__ volatile("yield" ::: "memory");
#endif
    }
}

VOID
VEAPI
KsReleaseSpinLockFromDpcLevelInternal(IN OUT PKSPIN_LOCK Lock, const PCHAR Function, const PCHAR File, INT Line)
{
#if defined(SPINLOCK_DBG)
    KdPrintf("[Ks] Function %s from %s and %d line try to aquire spinlock.\n", Function, File, Line);
#endif
    // VE_ASSERT(KeGetCurrentIrql() >= DISPATCH_LEVEL);

    /* Langsung lepas lock tanpa utak-atik IRQL */
    __atomic_store_n(Lock, 0, __ATOMIC_RELEASE);
}

BOOLEAN
VEAPI
KsTryToAcquireSpinLockAtDpcLevel(IN OUT PKSPIN_LOCK Lock)
{
    // VE_ASSERT(KeGetCurrentIrql() >= DISPATCH_LEVEL);

    /* 
     * __atomic_exchange_n mengembalikan nilai LAMA dari Lock.
     * Jika nilai lama == 0, artinya lock sebelumnya BEBAS dan sekarang BERHASIL dikunci (diisi 1).
     * Jika nilai lama == 1, artinya lock sedang DIPAKAI, dan tidak ada perubahan state.
     */
    return (__atomic_exchange_n(Lock, 1, __ATOMIC_ACQUIRE) == 0);
}

BOOLEAN
VEAPI
KsTryToAcquireSpinLock(IN OUT PKSPIN_LOCK Lock, OUT PKIRQL OldIrql)
{
    KIRQL CurrentIrql = KeGetCurrentIrql();

    /* 1. Naikkan IRQL terlebih dahulu ke DISPATCH_LEVEL */
    if (CurrentIrql < DISPATCH_LEVEL)
        *OldIrql = KeRaiseIrql(DISPATCH_LEVEL);
    else
        *OldIrql = CurrentIrql;

    /* 2. Coba ambil lock (1x test-and-set) */
    if (__atomic_exchange_n(Lock, 1, __ATOMIC_ACQUIRE) == 0) {
        // BERHASIL! Lock didapat, IRQL tetap di DISPATCH_LEVEL, return TRUE.
        return TRUE;
    }

    /* 
     * 3. GAGAL! Lock sedang dipegang CPU lain.
     * Kunci Utama: Turunkan kembali IRQL ke level asal sebelum keluar dari fungsi!
     */
    if (*OldIrql < DISPATCH_LEVEL)
        KeLowerIrql(*OldIrql);

    return FALSE;
}