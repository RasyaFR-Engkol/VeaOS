#include <osi386.h>

VEA_MEMORY_DESCRIPTOR LmMemoryMap[64];
int LmMemoryMapCount = 0;
VEA_MEMORY_DESCRIPTOR_EX LmExtendedMap[128];
int LmExtendedMapCount = 0;
PAGE_ENTRY *g_KernelPageDirectory = NULL;

void load_cr3_and_enable_paging(PAGE_ENTRY *pd);

VOID
VEAPI
LmBuildMemoryMap(VOID)
{
    UINT EBX = 0;
    ULONG Status;

    USHORT RMSegment = 0x0800;
    USHORT RMOffset = 0x0000;

    VEA_MEMORY_DESCRIPTOR *BounceBuffer = (VEA_MEMORY_DESCRIPTOR*)0x8000;

    LmMemoryMapCount = 0;

    do
    {
        Status = _INT15_E820_CALL(&EBX, RMSegment, RMOffset);

        if (Status == 1)
        {
            if (BounceBuffer->Length > 0)
            {
                cmemcpy(&LmMemoryMap[LmMemoryMapCount], BounceBuffer, sizeof(VEA_MEMORY_DESCRIPTOR));
                LmMemoryMapCount++;
            }
        }
    } while(EBX != 0 && Status == 1 && LmMemoryMapCount < 64);
}

VOID
VEAPI
LmInitMemoryManager(VOID)
{
    LmExtendedMapCount = 0;

    for(ULONG i = 0; i < LmMemoryMapCount; i++)
    {
        LmExtendedMap[i].Descriptor = LmMemoryMap[i];

        switch (LmMemoryMap[i].Type)
        {
            case 1: 
                LmExtendedMap[i].ExtendedType = LoaderGood; 
                break;
            case 3: 
                LmExtendedMap[i].ExtendedType = LoaderAcpiReclaimableMemory; 
                break;
            case 4: 
                LmExtendedMap[i].ExtendedType = LoaderAcpiNVS; 
                break;
            default: 
                LmExtendedMap[i].ExtendedType = LoaderOldMemory; // Reserved/Bad
                break;
        }
        LmExtendedMapCount++;
    }
}

PAGE_ENTRY*
VEAPI
LmInitializePaging(VOID)
{
    PAGE_ENTRY *PageDirectory = (PAGE_ENTRY*)LmAllocatePagesPreVirt(1);
    PAGE_ENTRY *PageTableIdt  = (PAGE_ENTRY*)LmAllocatePagesPreVirt(1);
    PAGE_ENTRY *PageTableHhk  = (PAGE_ENTRY*)LmAllocatePagesPreVirt(1);

    for (ULONG i = 0; i < 1024; i++) {
        // Identity Map
        PageTableIdt[i].bits.present = 1;
        PageTableIdt[i].bits.rw = 1;
        PageTableIdt[i].bits.user = 0;
        PageTableIdt[i].bits.frame = i;

        // Higher Half Map (Isi sama persis)
        PageTableHhk[i].bits.present = 1;
        PageTableHhk[i].bits.rw = 1;
        PageTableHhk[i].bits.user = 0;
        PageTableHhk[i].bits.frame = i; 
    }

    PageDirectory[0].bits.present = 1;
    PageDirectory[0].bits.rw = 1;
    PageDirectory[0].bits.user = 0;
    PageDirectory[0].bits.frame = ((ULONG)PageTableIdt) >> 12;

    PageDirectory[768].bits.present = 1;
    PageDirectory[768].bits.rw = 1;
    PageDirectory[768].bits.user = 0;
    PageDirectory[768].bits.frame = ((ULONG)PageTableHhk) >> 12;

    PageDirectory[1023].bits.present = 1;
    PageDirectory[1023].bits.rw = 1;
    PageDirectory[1023].bits.user = 0;
    PageDirectory[1023].bits.frame = ((ULONG)PageDirectory) >> 12;

    return PageDirectory;
}

PAGE_ENTRY*
VEAPI
LmInitSystem(VOID)
{
    LmBuildMemoryMap();
    LmInitMemoryManager();

    PAGE_ENTRY *PageDirectory = LmInitializePaging();
    if(!PageDirectory)
    {
        while(1) asm("hlt");
    }

    load_cr3_and_enable_paging(PageDirectory);

    g_KernelPageDirectory = PageDirectory;
    return PageDirectory;
}