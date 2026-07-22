#include <veakrnl.h>

KIDTDESCRIPTOR32 KiIdt[256];
KIDTR32 KsIdt32;

VOID
VEAPI
ks_setup_idt_trap(ULONG Vector, PVOID Routine, ULONG CS, ULONG Flags)
{
    if (Vector >= 256) return;

    ULONG Address = (ULONG)Routine;
    PKIDTDESCRIPTOR32 Entry = &KiIdt[Vector];

    Entry->OffsetLow  = (USHORT)(Address & 0xFFFF);
    Entry->Selector   = (USHORT)CS;
    Entry->Reserved   = 0;
    Entry->Attributes.Flags = (UCHAR)Flags;
    Entry->OffsetHigh = (USHORT)((Address >> 16) & 0xFFFF);
}

VOID
VEAPI
ks_initialize_exception(VOID)
{
    ks_setup_idt_trap(0,  (PVOID)isr_0_entry,  KGDT32_R0_CODE, IDT_FLAGS_INTR_RING0);
    ks_setup_idt_trap(1,  (PVOID)isr_1_entry,  KGDT32_R0_CODE, IDT_FLAGS_INTR_RING0);
    ks_setup_idt_trap(2,  (PVOID)isr_2_entry,  KGDT32_R0_CODE, IDT_FLAGS_INTR_RING0);
    ks_setup_idt_trap(6,  (PVOID)isr_6_entry,  KGDT32_R0_CODE, IDT_FLAGS_INTR_RING0);
    ks_setup_idt_trap(8,  (PVOID)isr_8_entry,  KGDT32_R0_CODE, IDT_FLAGS_INTR_RING0);
    // 2. Setup GDT & Memory Fault yang krusial untuk kestabilan kernel
    ks_setup_idt_trap(13, (PVOID)isr_13_entry, KGDT32_R0_CODE, IDT_FLAGS_INTR_RING0);
    ks_setup_idt_trap(14, (PVOID)isr_14_entry, KGDT32_R0_CODE, IDT_FLAGS_INTR_RING0);

    KsIdt32.Limit = (sizeof(KIDTDESCRIPTOR32) * 256) - 1;
    KsIdt32.Base  = (ULONG)&KiIdt;

    // lidt
    ks_load_idt(&KsIdt32);
}

VOID
VEAPI
ksi_dispatch_exception(PKREGISTER_FRAME register_frame)
{
    if(register_frame->Vector == KS_EXCEPTION_PAGE_FAULT)
    {
        // handle page fault
        BOOLEAN Success = FALSE;
        ULONG Cr2;
        __asm__ __volatile__(
            "mov %%cr2, %0"
            : "=r" (Cr2)  // Output operand: simpan hasil dari %0 ke variabel Cr2
            :             // Input operand (kosong)
            :             // Clobbered registers (kosong)
        );
        // Success = MmHandlePageFault(Cr2);

        if(!Success)
        {
            KsBugCheck(0x50, (ULONG_PTR)Cr2, register_frame->Vector, register_frame->ErrorCode, 0);
        }
    }
    else if(register_frame->Vector == KS_EXCEPTION_GENERAL_PROTECTION && (register_frame->Cs & RPL_MASK) == MODE_USER)
    {
        // handle GP caused by user
    }

    kdp_print("ERROR. Something Happened.\n\r");
    while(1) asm("hlt");
}