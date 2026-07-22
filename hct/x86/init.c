#include <veakrnl.h>

VOID
VEAPI
HctInitializeProcessor(PBLOCK_BOOT_1 BlockBoot, ULONG ProcessorNumber)
{
    HctpSetupProcessorIdentity(ProcessorNumber);

    ApicInitializeSubsystem();
}