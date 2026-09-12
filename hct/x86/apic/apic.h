#pragma once

#include <procbind.h>
#include "hct.h"

//
// Register Offset Local APIC Timer
//
#define APIC_REG_LVT_TIMER          0x320
#define APIC_REG_TIMER_INIT_COUNT   0x380
#define APIC_REG_TIMER_CURR_COUNT   0x390
#define APIC_REG_TIMER_DIV_CONFIG   0x3E0

#define IrqlToApicVector(Irql)      ((ULONG)((Irql) << 4))
#define ApicVectorToIrql(Vector)    ((KIRQL)((Vector) >> 4))

//
// Vector IDT khusus untuk APIC Timer Interrupt (misal Vector 32 / 0x20)
//
#define APIC_TIMER_INTERRUPT_VECTOR IrqlToApicVector(CLOCK_LEVEL)

//
// APIC Timer Divide Configurations (Offset 0x3E0)
//
#define APIC_TIMER_DIV_1            0x0B
#define APIC_TIMER_DIV_2            0x00
#define APIC_TIMER_DIV_4            0x01
#define APIC_TIMER_DIV_8            0x02
#define APIC_TIMER_DIV_16           0x03
#define APIC_TIMER_DIV_32           0x08
#define APIC_TIMER_DIV_64           0x09
#define APIC_TIMER_DIV_128          0x0A

//
// APIC Timer Modes
//
#define APIC_TIMER_MODE_ONE_SHOT    0x0
#define APIC_TIMER_MODE_PERIODIC    0x1
#define APIC_TIMER_MODE_TSC_DEADLINE 0x2

//
// APIC Timer Register
//
typedef union _APIC_LVT_TIMER_REGISTER {
    ULONG Value;
    struct {
        ULONG Vector          : 8;  // Bit 0-7  : IDT Vector
        ULONG Reserved1       : 4;  // Bit 8-11 : Reserved
        ULONG DeliveryStatus  : 1;  // Bit 12   : 0 = Idle, 1 = Send Pending
        ULONG Reserved2       : 3;  // Bit 13-15: Reserved
        ULONG Mask            : 1;  // Bit 16   : 1 = Masked (Disabled), 0 = Unmasked
        ULONG TimerMode       : 2;  // Bit 17-18: 00 = One-shot, 01 = Periodic
        ULONG Reserved3       : 13; // Bit 19-31: Reserved
    };
} APIC_LVT_TIMER_REGISTER, *PAPIC_LVT_TIMER_REGISTER;

//
// APIC another registers
//
#define APIC_EOI                    0xB0
#define APIC_REG_TPR                0x080
#define APIC_BASE_ADDRESS           0xFEE00000
#define IOAPIC_BASE_ADDRESS         0xFEC00000
#define IOAPIC_REGSEL               0x00
#define IOAPIC_IOWIN                0x10
#define APIC_REG_ICR_LOW            0x300
#define APIC_REG_ICR_HIGH           0x310
#define APIC_REG_EOI                0x0B0
#define APIC_ICR_SHORTHAND_SELF     0x00040000
#define APIC_ICR_LEVEL_ASSERT       0x00004000

// 
// APIC Configuration
//
#define APIC_TIMER_HZ 250

//
// APIC Function
//
VOID
VEAPI
ApicEndSystemInterrupt(VOID);

VOID
VEAPI
ApicInitializeTimerInterrupt(VOID);

VOID
VEAPI
ApicStartTimer(ULONG PeriodicInterrupt);

VOID
VEAPI
ApicInitLazyIrql(VOID);

#define ApicRead(Register) \
    (*(volatile ULONG*)(APIC_BASE_ADDRESS + (Register)))

#define ApicWrite(Register, Value) \
    ((*(volatile ULONG*)(APIC_BASE_ADDRESS + (Register))) = (ULONG)(Value))

FORCEINLINE
ULONG
VEAPI
IoapicRead(ULONG Register)
{
    *(volatile ULONG*)(IOAPIC_BASE_ADDRESS + IOAPIC_REGSEL) = Register;
    return *(volatile ULONG*)(IOAPIC_BASE_ADDRESS + IOAPIC_IOWIN);
}

FORCEINLINE
VOID
VEAPI
IoapicWrite(ULONG Register, 
            ULONG Value)
{
    *(volatile ULONG*)(IOAPIC_BASE_ADDRESS + IOAPIC_REGSEL) = Register;
    *(volatile ULONG*)(IOAPIC_BASE_ADDRESS + IOAPIC_IOWIN) = Value;
}


FORCEINLINE
VOID
VEAPI
ApicWriteIrql(KIRQL NewIrql)
{
    //
    // Konversi IRQL (0-15) ke Priority Class APIC (0x00, 0x10, ..., 0xF0)
    // lalu tulis ke register TPR (0x080)
    //
    ApicWrite(APIC_REG_TPR, IrqlToApicVector(NewIrql));
}

FORCEINLINE
KIRQL
VEAPI
ApicReadIrql(VOID)
{
    //
    // Baca register TPR (0x080) lalu geser 4 bit ke kanan untuk mendapatkan IRQL (0-15)
    //
    ULONG TprValue = ApicRead(APIC_REG_TPR);
    return ApicVectorToIrql(TprValue);
}

VOID
VEAPI
ApicSendSelfIpi(UCHAR Vector);

VOID
VEAPI
ApicRequestSoftwareInterrupt(IN UCHAR Vector);