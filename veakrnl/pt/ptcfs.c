#include <veakrnl.h>

LONG
VEAPI
PtCfsCompareRoutine(
    PRTL_BALANCED_NODE NodeA, 
    PRTL_BALANCED_NODE NodeB
)
{
    PKTHREAD ThreadA = CONTAINING_RECORD(NodeA, KTHREAD, CfsNode);
    PKTHREAD ThreadB = CONTAINING_RECORD(NodeB, KTHREAD, CfsNode);

    if (ThreadA->VRuntime < ThreadB->VRuntime) {
        return -1; // NodeA lebih kecil, taruh di kiri
    } else if (ThreadA->VRuntime > ThreadB->VRuntime) {
        return 1;  // NodeA lebih besar, taruh di kanan
    }

    return 0;
}

VOID
VEAPI
PtEnqueueThreadCfs(
    PKRUNQUEUE RunQueue, 
    PKTHREAD Thread
)
{
    if (!RunQueue || !Thread) return;

    // Bersihkan pointer node sebelum dimasukkan ke tree
    Thread->CfsNode.Left = NULL;
    Thread->CfsNode.Right = NULL;
    Thread->CfsNode.Parent = NULL;

    // Ubah status thread menjadi Ready (menunggu jatah CPU)
    Thread->State = Ready;

    // Masukkan ke dalam Red-Black Tree CFS
    RtlInsertElementRb(&RunQueue->CfsTree, &Thread->CfsNode);

    // Update akumulasi statistik RunQueue
    RunQueue->RunningTasks++;
    RunQueue->TotalWeight += Thread->Weight;
}

VOID
VEAPI
PtDequeueThreadCfs(
    PKRUNQUEUE RunQueue, 
    PKTHREAD Thread
)
{
    if (!RunQueue || !Thread) return;

    // Hapus node dari RBT
    RtlDeleteElementRb(&RunQueue->CfsTree, &Thread->CfsNode);

    // Potong statistik RunQueue
    if (RunQueue->RunningTasks > 0) {
        RunQueue->RunningTasks--;
    }
    
    if (RunQueue->TotalWeight >= Thread->Weight) {
        RunQueue->TotalWeight -= Thread->Weight;
    } else {
        RunQueue->TotalWeight = 0; // Proteksi underflow
    }
}

PKTHREAD
VEAPI
PtPickNextThreadCfs(PKRUNQUEUE RunQueue)
{
    if (!RunQueue) return NULL;

    // Ambil node paling kiri (Leftmost) dalam O(1)
    PRTL_BALANCED_NODE LeftmostNode = RtlGetLeftmostNodeRb(&RunQueue->CfsTree);

    // Jika RBT kosong, tidak ada task yang "Ready" (CPU Idle)
    if (!LeftmostNode) {
        RunQueue->CurrentThread = NULL;
        return NULL;
    }

    // Rekonstruksi pointer KTHREAD dari RTL_BALANCED_NODE
    PKTHREAD NextThread = CONTAINING_RECORD(LeftmostNode, KTHREAD, CfsNode);

    // CABUT dari RBT agar VRuntime-nya bisa di-update bebas tanpa merusak tree
    RtlDeleteElementRb(&RunQueue->CfsTree, &NextThread->CfsNode);

    // Ubah status ke Running dan set sebagai CurrentThread
    NextThread->State = Running;
    RunQueue->CurrentThread = NextThread;

    return NextThread;
}