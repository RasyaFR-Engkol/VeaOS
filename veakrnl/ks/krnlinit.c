#include "internal/util.h"
#include "ldrtypes.h"
#include <veakrnl.h>

ULONG ks_cpu_count = 0;
#define MAX_KERNEL_SIZE
ULONG ks_kernel_stack[0x2000];

VOID ks_system_startup_boot_stack(PBLOCK_BOOT_1 block_boot);

VOID ks_system_startup(PBLOCK_BOOT_1 block_boot)
{
    ULONG cpu;
    ULONG_PTR stack_top;

    /* naikan jumlah cpu */
    cpu = ks_cpu_count++;

    /* kalo cpu 0, aktifkan serial aja */
    if(!cpu)
    {
        kdp_activate_serial();

        kdp_print("Hello from ks_system_startup.\n\r");

        ks_initialize_exception();
        kdp_print("Initialize exception done.\n\r");
    }

    /* jump to kernel stack */
    stack_top = (ULONG_PTR)ks_kernel_stack + sizeof(ks_kernel_stack);
    ksi_switch_stack(stack_top, (PVOID)ks_system_startup_boot_stack, (PVOID)block_boot);
}

VOID 
VEAPI 
UlDumpPoolTracker(VOID);

VOID ks_system_startup_boot_stack(PBLOCK_BOOT_1 block_boot)
{

    ul_initialize_layer(block_boot);

    HctInitializeProcessor(block_boot, ks_cpu_count - 1);

    ANSI_STRING StringThis;
    RtlInitAnsiString(&StringThis, "/SomeFunckingNugger");

    OBJECT_ATTRIBUTES ObjAttr;
    InitializeObjectAttributes(&ObjAttr, &StringThis, OBJ_CASE_INSENSITIVE, NULL, NULL);
    VeaCreateDirectoryNamespace(&ObjAttr);

    ObDumpObjectTree();

    // todo: buat task baru untuk muter muter
    while(1) asm("hlt");
}