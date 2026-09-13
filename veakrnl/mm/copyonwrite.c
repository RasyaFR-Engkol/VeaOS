#include <veakrnl.h>

ULONG MmScratchSrcVa = 0;
ULONG MmScratchDstVa = 0;

VOID
VEAPI
MmpInitializeCowScratch(VOID)
{
    // Reserve 2 halaman VA permanen dari VAD tree, khusus dipakai
    // buat "jendela" sementara mapping physical page pas CoW copy
    MmScratchSrcVa = MmAllocateVirtualRange(1);
    MmScratchDstVa = MmAllocateVirtualRange(1);
}
