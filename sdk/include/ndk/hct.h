#pragma once

#include "procbind.h"
#include <ldrtypes.h>
#include "hctintrinsic.h"

/* KPCR */
typedef struct _KPCR {
    // NT_TIB (Thread Information Block) - Biasanya wajib ada di awal buat kompatibilitas user-mode
    struct _KPCR *SelfPcr;     // Offset 0x00: Pointer ke dirinya sendiri (WAJIB)
    PVOID CurrentThread;       // Offset 0x04: Thread yang lagi jalan di Core ini
    
    // Identitas CPU
    UCHAR ProcessorNumber;     // Offset 0x08: Core 0, Core 1, dst.
    UCHAR Irql;                // Offset 0x09: IRQL saat ini (buat Lazy IRQL)
    USHORT Padding;            // Offset 0x0A: Biar rata 4 byte
    
    // Tabel Hardware
    PVOID IDT;                 // Offset 0x0C: Interrupt Descriptor Table CPU ini
    PVOID GDT;                 // Offset 0x10: Global Descriptor Table CPU ini
    PVOID TSS;                 // Offset 0x14: Task State Segment CPU ini
    
    // Ekstra buat HCT / HAL
    ULONG HalReserved[16];     // Ruang buat naruh data spesifik hardware (kayak alamat LAPIC)
} KPCR, *PKPCR;

VOID
VEAPI
HctInitializeProcessor(PBLOCK_BOOT_1 BlockBoot, ULONG ProcessorNumber);

VOID
VEAPI
HctpSetupProcessorIdentity(ULONG ProcessorNumber);

/* APIC */
#define IA32_APIC_BASE_MSR        0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800

VOID
VEAPI
ApicInitializeSubsystem(VOID);