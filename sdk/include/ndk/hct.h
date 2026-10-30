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

FORCEINLINE PKPCR KeGetPcr(VOID)
{
    PKPCR Pcr;
    __asm__ volatile("movl %%fs:0x1C, %0" : "=r"(Pcr));
    return Pcr;
}

FORCEINLINE PKPRCB KeGetCurrentPrcb(VOID)
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
#define SYNCH_LEVEL         DISPATCH_LEVEL

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

VOID
VEAPI
HctSetIrql(KIRQL Irql);

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

VOID
VEAPI
HctMemoryBarrierWithoutFence(VOID);

VOID
VEAPI
HctStallExecutionProcessor(
    IN ULONG Microseconds
);

UCHAR
VEAPI
READ_PORT_UCHAR(
    IN volatile UCHAR *Port
);

USHORT
VEAPI
READ_PORT_USHORT(
    IN volatile USHORT *Port
);

ULONG
VEAPI
READ_PORT_ULONG(
    IN volatile ULONG *Port
);

VOID
VEAPI
WRITE_PORT_UCHAR(
    IN volatile UCHAR *Port,
    IN UCHAR Value
);

VOID
VEAPI
WRITE_PORT_USHORT(
    IN volatile USHORT *Port,
    IN USHORT Value
);

VOID
VEAPI
WRITE_PORT_ULONG(
    IN volatile ULONG *Port,
    IN ULONG Value
);

ULONG
VEAPI
READ_REGISTER_ULONG(
    IN volatile ULONG *Register
);

USHORT
VEAPI
READ_REGISTER_USHORT(
    IN volatile USHORT *Register
);

UCHAR
VEAPI
READ_REGISTER_UCHAR(
    IN volatile UCHAR *Register
);

VOID
VEAPI
WRITE_REGISTER_ULONG(
    IN volatile ULONG *Register,
    IN ULONG Value
);

VOID
VEAPI
WRITE_REGISTER_USHORT(
    IN volatile USHORT *Register,
    IN USHORT Value
);

VOID
VEAPI
WRITE_REGISTER_UCHAR(
    IN volatile UCHAR *Register,
    IN UCHAR Value
);

#define HctIrqVectorToIsrVector(Irq) ((Irq) + 0x30)
#define HctIsrVectorToIrqVector(Isr) ((Isr) - 0x30)
#define HctIsIsrRegistered(Vector) \
    ((HctIsrBitmap[(Vector) >> 3] & (1 << ((Vector) & 7))) != 0)
#define HctSetIsrBitmap(Vector) \
    (HctIsrBitmap[(Vector) >> 3] |= (1 << ((Vector) & 7)))
#define HctClearIsrBitmap(Vector) \
    (HctIsrBitmap[(Vector) >> 3] &= ~(1 << ((Vector) & 7)))

typedef VOID (VEAPI *PHCT_INTERNAL_HANDLER)(PKTRAP_FRAME TrapFrame, IN ULONG Vector);

/* Struct Internal Interrupt (ISR 0x00 - 0x19 / CPU Exceptions) */
typedef struct _HCT_INTERRUPT_INTERNAL {
    ULONG Vector;
    ULONG Used;
    PHCT_INTERNAL_HANDLER HandlerRoutine;
} HCT_INTERRUPT_INTERNAL, *PHCT_INTERRUPT_INTERNAL;

/* Struct External Interrupt (ISR 0x20 - 0xFF / IRQs) */
typedef struct _HCT_INTERRUPT_EXTERNAL {
    PKINTERRUPT Interrupt;          // Dipakai oleh Driver Device
    PHCT_INTERNAL_HANDLER Handler;  // Dipakai oleh Core System (Timer, IPI, Dispatch)
    ULONG Vector;
    BOOLEAN IsSystemInterrupt;      // Flag pembeda
    ULONG Used;
    KIRQL Irql;
} HCT_INTERRUPT_EXTERNAL, *PHCT_INTERRUPT_EXTERNAL;

VEASTATUS
VEAPI
HctRegisterSystemIsr(
    IN ULONG IsrVector,
    IN PHCT_INTERNAL_HANDLER HandlerRoutine
);

VEASTATUS
VEAPI
HctInitializeIsr(VOID);

VEASTATUS
VEAPI
HctRegisterInterruptToIsr(
    IN ULONG Vector,
    IN PHCT_INTERNAL_HANDLER HandlerRoutine
);

VEASTATUS
VEAPI
HctConnectInterruptToIsr(
    IN PKINTERRUPT Interrupt,
    IN ULONG IrqVector
);

VEASTATUS
VEAPI
HctDisconnectInterruptFromIsr(
    IN PKINTERRUPT Interrupt
);

VEASTATUS
VEAPI
HctConnectMessageInterrupt(
    IN PKINTERRUPT Interrupt,
    IN KIRQL Irql,
    OUT PKINTERRUPT_MESSAGE Message
);
