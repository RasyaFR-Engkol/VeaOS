#include "internal/util.h"
#include "ldrtypes.h"
#include <veakrnl.h>

ULONG ks_cpu_count = 0;

VOID ks_system_startup_boot_stack(PBLOCK_BOOT_2 block_boot);
PBLOCK_BOOT_2 KsLoaderBlock = NULL;
KPRCB KsProcessorBlock[MAX_CPU];

VOID
VEAPI
KiInitSystem(VOID)
{
    PKPRCB CurrentPrcb = KeGetCurrentPrcb();
    /* Initialize Timer List */
    KsiInitializeTimerList();

    /* Initialize it's DPC Timer and set it as first DPC to ever run */
    KsInitializeDpc(&KiTimerExpireDpc, KsiTimerExpirationDpcRoutine, NULL);

    /* DPC Stack is done allocated by our loader. Init LOCK and linked lsit */
    KsInitializeSpinlock(&CurrentPrcb->DpcLock);
    InitializeListHead(&CurrentPrcb->DpcListHead);
}

VOID
VEAPI
KsiInitializeKernel(
    IN PKPROCESS InitProcess,
    IN PKTHREAD InitThread,
    IN PVOID IdleStack,
    IN PKPRCB Prcb,
    IN PBLOCK_BOOT_2 BlockBoot
)
{
    PVOID TrueStackBase = (PVOID)((ULONG_PTR)IdleStack - 8192);

    // Init hand built thread
    PtInitializeHandBuildThread(InitThread, InitProcess, TrueStackBase, 8192, NULL);

    // Daftarkan ke PRCB dan RunQueue
    Prcb->CurrentThread = (PKTHREAD)InitThread;
    PtRunQueue[Prcb->Number].CurrentThread = InitThread;

    // Init Upper Layer
    ul_initialize_layer(BlockBoot);
}

VOID ks_system_startup(PBLOCK_BOOT_2 block_boot)
{
    ULONG cpu;
    ULONG_PTR InitialStack = block_boot->KernelStack;
    ULONG_PTR StackTop;

    asm volatile("cli");

    /* naikan jumlah cpu */
    cpu = ks_cpu_count++;

    /* Save LoaderBlock */
    KsLoaderBlock = block_boot;

    /* kalo cpu 0, aktifkan serial aja */
    if(!cpu)
    {
        // Set our FS again to KGDT32_R0_PCR
        KeReloadFsPcr();

        kdp_activate_serial();
        kdp_print("Hello from ks_system_startup.\n\r");

        KiInitSystem();

        ks_initialize_exception();
        kdp_print("Initialize exception done.\n\r");
    }
    
    HctInitializeProcessor(block_boot, ks_cpu_count - 1);

    /* jump to kernel stack */
    StackTop = (ULONG_PTR)InitialStack;
    ksi_switch_stack(StackTop, (PVOID)ks_system_startup_boot_stack, (PVOID)block_boot);
}

VOID ks_system_startup_boot_stack(PBLOCK_BOOT_2 block_boot)
{
    PKPRCB Prcb = KeGetCurrentPrcb();
    PKTHREAD Thread = (PKTHREAD)block_boot->Thread;
    PKPROCESS Process = (PKPROCESS)Thread->ApcState.Process;
    PVOID KernelStack = (PVOID)block_boot->KernelStack;

    if(Prcb->Number == 0)
    {
        KsiInitializeKernel(
            Process, 
            Thread, 
            KernelStack, 
            Prcb, 
            block_boot
        );
    }
    else
    {
        PtInitializeHandBuildThread(Thread, Process, KernelStack, 8192, NULL);
    }
    
    asm volatile("sti");
    KeLowerIrql(PASSIVE_LEVEL);

    PtIdleLoop();
}