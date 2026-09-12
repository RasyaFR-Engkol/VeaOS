#include <veakrnl.h>
#include "apic.h"

// HANDLER
VOID HctApicTimerTick(VOID);
VOID HctpDispatch(VOID);

// Variable used to save HCT System Data Time
ULARGE_INTEGER KsiTick = {.QuadPart = 0};
ULONGLONG      KsiTotalSeconds = 0;

VOID
VEAPI
ApicInitializeTimerInterrupt(VOID)
{
    APIC_LVT_TIMER_REGISTER LvtTimer;

    // Bus divider 16
    ApicWrite(APIC_REG_TIMER_DIV_CONFIG, APIC_TIMER_DIV_16);

    // Value = 0, Vector dikirim ke APIC_TIMER_INTERRUPT_VECTOR, TimerMode Periodic,
    // dan masked
    LvtTimer.Value = 0;
    LvtTimer.Vector = APIC_TIMER_INTERRUPT_VECTOR;
    LvtTimer.TimerMode = APIC_TIMER_MODE_PERIODIC;
    LvtTimer.Mask = 1;

    // Tulis value configurasi ke APIC_REG_LVT_TIMER
    ApicWrite(APIC_REG_LVT_TIMER, LvtTimer.Value);
    
    // Reset initial count nya
    ApicWrite(APIC_REG_TIMER_INIT_COUNT, 0);

    // Daftarkan handler punya Hct
    KsRegisterSystemInterruptHandler((PVOID)HctApicTimerTick, IrqlToApicVector(CLOCK_LEVEL));
    KsRegisterSystemInterruptHandler((PVOID)HctpDispatch, IrqlToApicVector(DISPATCH_LEVEL));
}

VOID
VEAPI
ApicStartTimer(IN ULONG PeriodicInterrupt)
{
    APIC_LVT_TIMER_REGISTER LvtTimer;
    ULONG TicksElapsed;
    ULONG TicksPerSecond;
    ULONG TargetInitialCount;

    // Cegag PeriodicInterrupt 0
    if (PeriodicInterrupt == 0)
    {
        return;
    }

    // Set PIT Channel ke mode 2
    IoWritePortByte(0x43, 0xB0);

    // Durasi 10ms = 0.01 detik
    // Frekuensi PIT = 1,193,182 Hz -> (1193182 * 0.01) = 11932 ticks
    IoWritePortByte(0x42, 11932 & 0xFF);
    IoWritePortByte(0x42, (11932 >> 8) & 0xFF);

    // Set Gate 2 (Port 0x61: Bit 0 = Gate 2, Bit 1 = Disable Speaker)
    UCHAR PitControl = IoReadPortByte(0x61);
    PitControl = (PitControl & ~0x02) | 0x01;
    IoWritePortByte(0x61, PitControl);

    // Persiapkan APIC Timer: Divider 16
    ApicWrite(APIC_REG_TIMER_DIV_CONFIG, APIC_TIMER_DIV_16);

    // Tulis Initial Count bernilai MAX (0xFFFFFFFF) untuk mulai hitung mundur
    ApicWrite(APIC_REG_TIMER_INIT_COUNT, 0xFFFFFFFF);

    while ((IoReadPortByte(0x61) & 0x20) == 0)
    {
        // Polling tunggu 10 ms...
    }

    // Hitung berapa tick APIC yang berkurang selama sampel 10 ms
    TicksElapsed = 0xFFFFFFFF - ApicRead(APIC_REG_TIMER_CURR_COUNT);

    // Extrapolasi jumlah tick APIC untuk 1 Detik penuh (10 ms * 100 = 1000 ms = 1 s)
    TicksPerSecond = TicksElapsed * 100;

    // Hitung Initial Count target sesuai dengan Hz yang diminta (misal 250 Hz)
    TargetInitialCount = TicksPerSecond / PeriodicInterrupt;

    LvtTimer.Value = ApicRead(APIC_REG_LVT_TIMER);
    LvtTimer.TimerMode = APIC_TIMER_MODE_PERIODIC;
    LvtTimer.Mask = 0;

    ApicWrite(APIC_REG_LVT_TIMER, LvtTimer.Value);
    ApicWrite(APIC_REG_TIMER_INIT_COUNT, TargetInitialCount);
}

VOID
VEAPI
HctDumpTimeRun(IN ULONGLONG InputSecond)
{
    //
    // Cast ke ULONG agar aman dari issue division 64-bit (__udivdi3) di 32-bit kernel.
    // (ULONG detik cukup buat menampung runtime OS sampai 136 tahun!)
    //
    ULONG TotalSeconds = (ULONG)InputSecond;

    ULONG Hour   = TotalSeconds / 3600;            // 1 Jam = 3600 detik
    ULONG Minute = (TotalSeconds / 60) % 60;       // Ambil sisa menit
    ULONG Second = TotalSeconds % 60;              // Ambil sisa detik

    //
    // Format %02u artinya:
    // Unsigned integer, minimal 2 digit, pad dengan angka 0 di depan jika < 10 (contoh: 01:05:09)
    //
    KdPrintf("Kernel RunTime: %02u:%02u:%02u\n\r", Hour, Minute, Second);
}

VOID
VEAPI
HctApicTimerTickC(IN PKREGISTER_FRAME Frame)
{
    KIRQL OldIrql;
    
    /* Begin system interrupt (also check if it's spurious interrupt or not) */
    if (!HctBeginSystemInterrupt(CLOCK_LEVEL, APIC_TIMER_INTERRUPT_VECTOR, &OldIrql))
    {
        /* Directly return to ASM stub */
        return;
    }
    
    /* Increase Time */
    KsiTick.QuadPart++;

    /* Update System Time here */
    KsUpdateSystemTime(Frame, TIMER_INCREMENT, OldIrql);

    /* Insert timer checking DPC */
    KsInsertQueueDpc(&KiTimerExpireDpc, (PVOID)(ULONG_PTR)KsiTick.LowPart, NULL);

    /* After CLOCK_LEVEL, we should DISPATCH then */
    HctRequestSoftwareInterrupt(DISPATCH_LEVEL);

    /* End system interrupt */
    HctEndSystemInterrupt(OldIrql);
}