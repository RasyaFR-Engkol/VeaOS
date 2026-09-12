#include <veakrnl.h>

#define MAX_PROCESSORS MAX_CPU
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
    // Nothing. there is nothing
    return;
}