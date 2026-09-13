#include <veakrnl.h>

VOID
VEAPI
PtIdleLoop(VOID)
{   
    // There is nothing for a while now
    for(;;) 
    {
        asm volatile("sti");
        asm volatile("hlt");
    }
}