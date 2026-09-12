#pragma once

#include "internal/x86.h"
#include "procbind.h"
#include <ldrtypes.h>
#include "hctintrinsic.h"

#define KPCR_SELF_PCR          0x00
#define KPCR_CURRENT_THREAD    0x04
#define KPCR_PROCESSOR_NUMBER  0x08
#define KPCR_IRQL              0x09

VOID
VEAPI
HctInitializeProcessor(PBLOCK_BOOT_2 BlockBoot, ULONG ProcessorNumber);

VOID
VEAPI
HctpSetupProcessorIdentity(ULONG ProcessorNumber);

VOID
VEAPI
AcpiCachingTableToHct(PBLOCK_BOOT_2 BlockBoot);

/* APIC */
#define IA32_APIC_BASE_MSR        0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800

VOID
VEAPI
ApicInitializeSubsystem(VOID);

static inline PKPCR KeGetPcr(VOID)
{
    PKPCR Pcr;
    __asm__ volatile("movl %%fs:0x1C, %0" : "=r"(Pcr));
    return Pcr;
}

static inline PKPRCB KeGetCurrentPrcb(VOID)
{
    PKPRCB Prcb;
    // Baca langsung pointer Prcb dari offset 0x20 di segment FS
    __asm__ volatile("movl %%fs:0x20, %0" : "=r"(Prcb));
    return Prcb;
}

// IRQL LEVEL
#define PASSIVE_LEVEL       0   // Normal user/kernel execution
#define APC_LEVEL           1   // Asynchronous Procedure Calls
#define DISPATCH_LEVEL      2   // Thread Scheduler & DPCs

// DIRQL (Device IRQLs) berada di rentang 3 - 11
#define DIRQL_MIN           3
#define DIRQL_MAX           11

#define PROFILE_LEVEL       12  // Profiling timer
#define CLOCK_LEVEL         13  // System Clock/Timer Interrupt
#define IPI_LEVEL           14  // Inter-Processor Interrupt (SMP)
#define HIGH_LEVEL          15  // Interrupts disabled / Critical

VOID
VEAPI
HctEndSystemInterrupt(KIRQL OldIrql);

KIRQL
VEAPI
HctRaiseIrql(KIRQL NewIrql);

VOID
VEAPI
HctLowerIrql(KIRQL NewIrql);

BOOLEAN
VEAPI
HctInitSystem(ULONG BootPhase);

BOOLEAN
VEAPI
HctBeginSystemInterrupt(
    KIRQL VectorIrql,
    ULONG Vector,
    KIRQL *Irql
);

VOID
VEAPI
HctRequestSoftwareInterrupt(IN KIRQL Irql);