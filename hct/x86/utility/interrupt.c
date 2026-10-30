/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : interrupt.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Interrupt Function
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>
#include "../apic/apic.h"

/* Revision History ------------------------------------------------------
 * DATE       : 01-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file interrupt.c
 * --------------------------------------------------------------------- */

/* VARIABLE */
KIDTDESCRIPTOR32 KiIdt[256];
KIDTR32 KsIdt32;
UCHAR HctIsrBitmap[32];
static HCT_INTERRUPT_INTERNAL HctInternalTable[0x1A];
static HCT_INTERRUPT_EXTERNAL HctExternalTable[256 - 0x20];
extern ULONG_PTR HctTrapStubs[0x1A];
extern ULONG_PTR HctIrqStubs[256 - 0x20];
static UCHAR DfStackBuffer[4096] __attribute__((aligned(16)));

/* FUNCTION */
VOID KiLoadIdt(VOID);
VOID KiLoadIdtPointer(IN PKIDTR32 Idtr);

/* DEFINE */
#define HCT_MSI_ADDRESS_BASE    0xFEE00000

/* PRIVATE FUNCTION */

static
VOID
HctpSetIdtGate(
    IN UCHAR Vector,
    IN ULONG_PTR Base,
    IN USHORT Selector,
    IN UCHAR Flags)
{
    KiIdt[Vector].OffsetLow = (USHORT)(Base & 0xFFFF);
    KiIdt[Vector].Selector = Selector;
    KiIdt[Vector].Attributes.Flags = Flags; 
    KiIdt[Vector].OffsetHigh = (USHORT)((Base >> 16) & 0xFFFF);
}

VOID
VEAPI
HctpInterruptHandler(IN PKTRAP_FRAME Frame, IN ULONG IsrVector)
{
    if (IsrVector <= 0x19)
    {
        PHCT_INTERRUPT_INTERNAL Internal;
        
        Internal = &HctInternalTable[IsrVector];
        if (Internal->Used && Internal->HandlerRoutine)
        {
            Internal->HandlerRoutine(Frame, IsrVector); // Pass trapframe jika ada
            return;
        }
        
        DPRINT("Unhandled CPU Exception Vector: 0x%X\n", IsrVector);
        return;
    }

    // 2. Handling External / IRQ Interrupt (0x20 - 0xFF)
    if (IsrVector >= 0x20 && IsrVector <= 0xFF)
    {
        PHCT_INTERRUPT_EXTERNAL External;
        ULONG Index;
        KIRQL OldIrql;
        KIRQL InterruptIrql;

        Index = IsrVector - 0x20;
        External = &HctExternalTable[Index];

        if (External->Used)
        {
            // A. Tentukan target IRQL
            if (External->IsSystemInterrupt)
            {
                InterruptIrql = External->Irql;
            }
            else if (External->Interrupt)
            {
                InterruptIrql = External->Interrupt->SynchronizeIrql;
            }
            else
            {
                DPRINT("Corrupted External ISR Vector: 0x%X\n", IsrVector);
                return;
            }

            // B. BEGIN: Naikkan IRQL & Atur APIC TPR
            if (!HctBeginSystemInterrupt(InterruptIrql, IsrVector, &OldIrql))
            {
                // Jika CurrentIrql >= InterruptIrql, HctBeginSystemInterrupt
                // sudah ngirim EOI + Self-IPI lalu return FALSE. 
                // Abort eksekusi ISR sekarang!
                return;
            }

            // C. EKSEKUSI HANDLER
            if (External->IsSystemInterrupt && External->Handler)
            {
                External->Handler(Frame, IsrVector);
            }
            else if (External->Interrupt && External->Interrupt->ServiceRoutine)
            {
                PKINTERRUPT Interrupt = External->Interrupt;
                BOOLEAN ServiceResult;

                ServiceResult = Interrupt->ServiceRoutine(
                    Interrupt,
                    Interrupt->ServiceContext);
            }

            // D. END: Kirim APIC EOI & Turunkan IRQL ke OldIrql
            HctEndSystemInterrupt(OldIrql);
            return;
        }
    }

    DPRINT("Spurious / Unregistered Interrupt: 0x%X\n", IsrVector);
    return;
}

/* PUBLIC FUNCTION */

KIRQL
VEAPI
HctConvertVectorToIrql(IN UCHAR Vector)
{
    return (KIRQL)(Vector >> 4);
}

VEASTATUS
VEAPI
HctInitializeIsr(VOID)
{
    ULONG i;

    RtlZeroMemory(HctIsrBitmap, sizeof(HctIsrBitmap));

    for (i = 0; i <= 0x19; i++)
    {
        HctInternalTable[i].Vector = i;
        HctInternalTable[i].Used = FALSE;
        HctInternalTable[i].HandlerRoutine = NULL;

        // Set IDT entry ke ASM Stub
        HctpSetIdtGate((UCHAR)i, HctTrapStubs[i], KGDT32_R0_CODE, IDT_FLAGS_INTR_RING0);
    }

    for (i = 0; i < (256 - 0x20); i++)
    {
        ULONG Vector = i + 0x20;

        HctExternalTable[i].Vector = Vector;
        HctExternalTable[i].Used = FALSE;
        HctExternalTable[i].Interrupt = NULL;
        HctExternalTable[i].Handler = NULL;
        HctExternalTable[i].IsSystemInterrupt = FALSE;
        HctExternalTable[i].Irql = 0;

        HctpSetIdtGate((UCHAR)Vector, HctIrqStubs[i], KGDT32_R0_CODE, IDT_FLAGS_INTR_RING0);
    }

    KiLoadIdt();

    DPRINT("HCT ISR Subsystem initialized.\n");
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
HctRegisterInterruptToIsr(
    IN ULONG Vector,
    IN PHCT_INTERNAL_HANDLER HandlerRoutine
)
{
    if (Vector > 0x19)
    {
        DPRINT("Vector 0x%X outside of range (0x00 - 0x19)\n", Vector);
        return STATUS_INVALID_PARAMETER;
    }

    if (HctIsIsrRegistered(Vector))
    {
        DPRINT("Internal Vector 0x%X registered!\n", Vector);
        return STATUS_KINTERRUPT_OCCUPIED;
    }

    HctInternalTable[Vector].Vector = Vector;
    HctInternalTable[Vector].HandlerRoutine = HandlerRoutine;
    HctInternalTable[Vector].Used = TRUE;

    HctSetIsrBitmap(Vector);

    DPRINT("Internal ISR Registered for Vector: 0x%X\n", Vector);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
HctConnectInterruptToIsr(
    IN PKINTERRUPT Interrupt,
    IN ULONG IrqVector
)
{
    ULONG IsrVector;
    ULONG ExternalIndex;
    ULONG Gsi;
    USHORT GsiFlags;
    VEASTATUS Status;

    if (!Interrupt)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (Interrupt->ConnectionType != InterruptLineBased)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (IrqVector > (0xBF - 0x30))
    {
        return STATUS_INVALID_PARAMETER;
    }

    IsrVector = HctIrqVectorToIsrVector(IrqVector);

    if (IsrVector > 0xFF)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (HctIsIsrRegistered(IsrVector))
    {
        DPRINT("Vector occupied: %u. Sharing not supported rn\n", IrqVector);
        return STATUS_KINTERRUPT_OCCUPIED;
    }

    Status = HctTranslateIrqToGsi(IrqVector, &Gsi, &GsiFlags);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("No IOAPIC GSI mapping for IRQ %u: 0x%X\n", IrqVector, Status);
        return Status;
    }

    ExternalIndex = IsrVector - 0x20;

    HctExternalTable[ExternalIndex].Interrupt = Interrupt;
    HctExternalTable[ExternalIndex].Handler = NULL;
    HctExternalTable[ExternalIndex].Vector = IsrVector;
    HctExternalTable[ExternalIndex].IsSystemInterrupt = FALSE;
    HctExternalTable[ExternalIndex].Used = TRUE;

    Status = HctSetGsiRedirection(Gsi, IsrVector, GsiFlags, FALSE);
    if (!VEA_SUCCESS(Status))
    {
        HctExternalTable[ExternalIndex].Interrupt = NULL;
        HctExternalTable[ExternalIndex].Handler = NULL;
        HctExternalTable[ExternalIndex].IsSystemInterrupt = FALSE;
        HctExternalTable[ExternalIndex].Irql = 0;
        HctExternalTable[ExternalIndex].Used = FALSE;
        return Status;
    }

    Interrupt->Vector = IsrVector;
    HctSetIsrBitmap(IsrVector);

    DPRINT("Connected IRQ %u (GSI %u, ISR 0x%X) to HCT\n",
           IrqVector, Gsi, IsrVector);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
HctDisconnectInterruptFromIsr(IN PKINTERRUPT Interrupt)
{
    ULONG IsrVector;
    ULONG ExternalIndex;
    ULONG Gsi;
    USHORT GsiFlags;
    VEASTATUS Status;
    PHCT_INTERRUPT_EXTERNAL External;

    if (!Interrupt)
    {
        return STATUS_INVALID_PARAMETER;
    }

    IsrVector = Interrupt->Vector;
    if (IsrVector < 0x20 || IsrVector > 0xFF)
    {
        return STATUS_INVALID_PARAMETER;
    }

    ExternalIndex = IsrVector - 0x20;
    External = &HctExternalTable[ExternalIndex];

    if (!External->Used || External->IsSystemInterrupt ||
        External->Interrupt != Interrupt)
    {
        return STATUS_NOT_FOUND;
    }

    if (Interrupt->ConnectionType == InterruptLineBased)
    {
        if (Interrupt->IrqVector > (0xBF - 0x30))
        {
            return STATUS_INVALID_PARAMETER;
        }

        Status = HctTranslateIrqToGsi(
            Interrupt->IrqVector,
            &Gsi,
            &GsiFlags);
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }

        Status = HctSetGsiRedirection(Gsi, IsrVector, GsiFlags, TRUE);
        if (!VEA_SUCCESS(Status))
        {
            return Status;
        }
    }
    else if (Interrupt->ConnectionType != InterruptMessageBased)
    {
        return STATUS_INVALID_PARAMETER;
    }

    External->Interrupt = NULL;
    External->Handler = NULL;
    External->IsSystemInterrupt = FALSE;
    External->Irql = 0;
    External->Used = FALSE;
    HctClearIsrBitmap(IsrVector);

    DPRINT("Disconnected %s interrupt (ISR 0x%X) from HCT\n",
           Interrupt->ConnectionType == InterruptLineBased ? "line" : "message",
           IsrVector);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
HctRegisterSystemIsr(
    IN ULONG IsrVector,
    IN PHCT_INTERNAL_HANDLER HandlerRoutine
)
{
    ULONG ExternalIndex;

    if (IsrVector < 0x20 || IsrVector > 0xFF)
    {
        DPRINT("Invalid System ISR Vector: 0x%X\n", IsrVector);
        return STATUS_INVALID_PARAMETER;
    }

    if (HctIsIsrRegistered(IsrVector))
    {
        DPRINT("ISR Vector 0x%X already occupied!\n", IsrVector);
        return STATUS_KINTERRUPT_OCCUPIED;
    }

    ExternalIndex = IsrVector - 0x20;

    HctExternalTable[ExternalIndex].Vector = IsrVector;
    HctExternalTable[ExternalIndex].Handler = HandlerRoutine;
    HctExternalTable[ExternalIndex].Interrupt = NULL;
    HctExternalTable[ExternalIndex].IsSystemInterrupt = TRUE;
    HctExternalTable[ExternalIndex].Used = TRUE;
    HctExternalTable[ExternalIndex].Irql =
        HctConvertVectorToIrql((UCHAR)IsrVector);

    HctSetIsrBitmap(IsrVector);

    DPRINT("System ISR Registered for Vector: 0x%X\n", IsrVector);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
HctConnectMessageInterrupt(
    IN PKINTERRUPT Interrupt,
    IN KIRQL Irql,
    OUT PKINTERRUPT_MESSAGE Message
)
{
    ULONG Vector;
    ULONG Low;
    ULONG High;
    ULONG Destination;

    if (!Interrupt || !Message || Irql < 3 || Irql > 0xB)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (Interrupt->ConnectionType != InterruptMessageBased)
    {
        return STATUS_INVALID_PARAMETER;
    }

    /* The vector priority class must match the ISR dispatch IRQL. */
    Low = (Irql << 4);
    High = (Irql << 4) | 0xF;

    for (Vector = Low; Vector <= High; Vector++)
    {
        if (!HctIsIsrRegistered(Vector))
        {
            break;
        }
    }

    if (Vector > High)
    {
        return STATUS_KINTERRUPT_OCCUPIED;
    }

    HctExternalTable[Vector - 0x20].Interrupt = Interrupt;
    HctExternalTable[Vector - 0x20].Handler = NULL;
    HctExternalTable[Vector - 0x20].Vector = Vector;
    HctExternalTable[Vector - 0x20].IsSystemInterrupt = FALSE;
    HctExternalTable[Vector - 0x20].Irql = 0;
    HctExternalTable[Vector - 0x20].Used = TRUE;

    Destination = (ApicRead(APIC_REG_ID) >> 24) & 0xFF;
    Message->AddressLow = HCT_MSI_ADDRESS_BASE | (Destination << 12);
    Message->AddressHigh = 0;
    Message->Data = Vector;
    Message->Vector = Vector;

    Interrupt->Vector = Vector;
    HctSetIsrBitmap(Vector);
    return STATUS_SUCCESS;
}

VOID VEAPI HctDoubleFaultTaskHandler(VOID)
{
    ULONG FaultingAddress;
    asm volatile("mov %%cr2, %0" : "=r"(FaultingAddress));

    PKPCR Kpcr = KeGetPcr();
    PKTSSENTRY32 CrashedTss = (PKTSSENTRY32)Kpcr->TSS;

    ULONG CrashedEip = CrashedTss->Eip;
    ULONG CrashedEsp = CrashedTss->Esp;

    KsBugCheckEx(
        KMODE_EXCEPTION_NOT_HANDLED,
        0x8,
        FaultingAddress,
        CrashedEip,
        CrashedEsp
    );

    for (;;)
    {
        asm volatile("cli; hlt");
    }
}

VOID
VEAPI
HctInitializeDoubleFault(VOID)
{
    PKPCR Kpcr = KeGetPcr();
    PKGDTENTRY32 Gdt = (PKGDTENTRY32)Kpcr->GDT;

    PKGDTENTRY32 DfGdtEntry = &Gdt[KGDT32_DF_TSS >> 3];
    ULONG DfTssBase = DfGdtEntry->BaseLow | 
                     (DfGdtEntry->HighWord.Bits.BaseMid << 16) | 
                     (DfGdtEntry->HighWord.Bits.BaseHi << 24);
    
    PKTSSENTRY32 DfTss = (PKTSSENTRY32)DfTssBase;
    
    DfTss->Eip  = (ULONG_PTR)HctDoubleFaultTaskHandler;
    DfTss->Esp  = (ULONG_PTR)(DfStackBuffer + sizeof(DfStackBuffer));
    DfTss->Esp0 = DfTss->Esp;

    KiIdt[8].OffsetLow = 0;
    KiIdt[8].Selector = KGDT32_DF_TSS;
    KiIdt[8].Reserved = 0;

    KiIdt[8].Attributes.Bits.Type    = 0x5; // 32-bit Task Gate (0x5)
    KiIdt[8].Attributes.Bits.System  = 0;   // System/Gate Descriptor
    KiIdt[8].Attributes.Bits.Dpl     = 0;   // Ring 0
    KiIdt[8].Attributes.Bits.Present = 1;   // Present = 1

    KiIdt[8].OffsetHigh = 0;
}

/* EOF */