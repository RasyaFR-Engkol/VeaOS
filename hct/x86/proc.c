#include <veakrnl.h>

#define MAX_PROCESSORS 32
#define GDT_ENTRIES 8

KPCR HctpProcessorRegisters[MAX_PROCESSORS];
KGDTENTRY32 HctpGdt[MAX_PROCESSORS][GDT_ENTRIES];

VOID 
VEAPI 
HctpSetGdtEntry(
    PKGDTENTRY32 Gdt, 
    ULONG Index, 
    ULONG Base, 
    ULONG Limit, 
    UCHAR Access, 
    UCHAR Flags
) 
{
    // Cek batas index biar gak buffer overflow
    if (Index >= GDT_ENTRIES) return; 

    // Base Address (32-bit) dipecah
    Gdt[Index].BaseLow = (USHORT)(Base & 0xFFFF);
    Gdt[Index].HighWord.Bytes.BaseMid = (UCHAR)((Base >> 16) & 0xFF);
    Gdt[Index].HighWord.Bytes.BaseHi  = (UCHAR)((Base >> 24) & 0xFF);

    // Limit (20-bit) dipecah
    Gdt[Index].LimitLow = (USHORT)(Limit & 0xFFFF);
    
    // Flags2 gabungan dari Limit (4 bit atas) + Flags (4 bit atas)
    Gdt[Index].HighWord.Bytes.Flags2 = (UCHAR)((Limit >> 16) & 0x0F) | (Flags & 0xF0);
    
    // Access Rights utuh 8-bit
    Gdt[Index].HighWord.Bytes.Flags1 = Access;
}

VOID
VEAPI
HctpSetupProcessorIdentity(ULONG ProcessorNumber)
{
    PKPCR Pcr = &HctpProcessorRegisters[ProcessorNumber];
    PKGDTENTRY32 CpuGdt = HctpGdt[ProcessorNumber];

    Pcr->SelfPcr = Pcr;
    Pcr->ProcessorNumber = (UCHAR)ProcessorNumber;
    Pcr->Irql = 0; // PASSIVE_LEVEL default
    Pcr->CurrentThread = NULL;

    HctpSetGdtEntry(CpuGdt, 1, 0x00000000, 0xFFFFF, 0x9A, 0xC0);
    HctpSetGdtEntry(CpuGdt, 2, 0x00000000, 0xFFFFF, 0x92, 0xC0);
    HctpSetGdtEntry(CpuGdt, 6, (ULONG)Pcr, sizeof(KPCR) - 1, 0x92, 0xC0);

    KDESCRIPTOR NewGdtDesc;
    NewGdtDesc.Limit = (sizeof(KGDTENTRY32) * GDT_ENTRIES) - 1;
    NewGdtDesc.Base = (ULONG)CpuGdt;
    __asm__ volatile("lgdt %0" : : "m"(NewGdtDesc));

    KDESCRIPTOR IdtDesc;
    __asm__ volatile("sidt %0" : "=m"(IdtDesc));
    Pcr->GDT = (PVOID)CpuGdt;
    Pcr->IDT = (PVOID)IdtDesc.Base;

    __asm__ volatile(
        "mov $0x10, %%ax \n"
        "mov %%ax, %%ds \n"
        "mov %%ax, %%es \n"
        "mov %%ax, %%ss \n"
        "mov $0x30, %%ax \n"
        "mov %%ax, %%fs \n"
        // Far jump ke CS (0x08) buat nge-flush pipeline instruction cache
        "ljmp $0x08, $1f \n" 
        "1: \n"
        : : : "eax"
    );
}