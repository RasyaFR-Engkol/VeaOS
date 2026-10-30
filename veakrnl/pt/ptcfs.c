#include <veakrnl.h>

#define PT_CFS_BASE_WEIGHT 1024UL
BOOLEAN
VEAPI
PtSetThreadPriority(
    IN PKTHREAD Thread,
    IN CHAR Priority
)
{
    ULONG WeightShift;

    if (!Thread)
        return FALSE;

    if (Priority < 0)
        Priority = 0;

    WeightShift = (ULONG)Priority / 8;
    if (WeightShift > 5)
        WeightShift = 5;

    Thread->Priority = Priority;
    Thread->Weight = (ULONGLONG)PT_CFS_BASE_WEIGHT << WeightShift;
    return TRUE;
}

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

    ULONG_PTR Flags = PtpSaveAndDisableInterrupts();

    // Bersihkan pointer node sebelum dimasukkan ke tree
    Thread->CfsNode.Left = NULL;
    Thread->CfsNode.Right = NULL;
    Thread->CfsNode.Parent = NULL;

    // Ubah status thread menjadi Ready
    Thread->State = Ready;

    // Masukkan ke dalam Red-Black Tree CFS
    RtlInsertElementRb(&RunQueue->CfsTree, &Thread->CfsNode);

    // Update akumulasi statistik RunQueue
    RunQueue->RunningTasks++;
    RunQueue->TotalWeight += Thread->Weight;

    PtpRestoreInterrupts(Flags);
}

VOID
VEAPI
PtDequeueThreadCfs(
    PKRUNQUEUE RunQueue, 
    PKTHREAD Thread
)
{
    if (!RunQueue || !Thread) return;

    ULONG_PTR Flags = PtpSaveAndDisableInterrupts();

    RtlDeleteElementRb(&RunQueue->CfsTree, &Thread->CfsNode);

    if (RunQueue->RunningTasks > 0) {
        RunQueue->RunningTasks--;
    }
    
    if (RunQueue->TotalWeight >= Thread->Weight) {
        RunQueue->TotalWeight -= Thread->Weight;
    } else {
        RunQueue->TotalWeight = 0;
    }

    PtpRestoreInterrupts(Flags);
}

PKTHREAD
VEAPI
PtPickNextThreadCfs(PKRUNQUEUE RunQueue)
{
    PKTHREAD NextThread = NULL;

    if (!RunQueue) return NULL;

    ULONG_PTR Flags = PtpSaveAndDisableInterrupts();

    PRTL_BALANCED_NODE LeftmostNode = RtlGetLeftmostNodeRb(&RunQueue->CfsTree);

    if (!LeftmostNode) {
        RunQueue->CurrentThread = NULL;
        PtpRestoreInterrupts(Flags);
        return NULL;
    }

    NextThread = CONTAINING_RECORD(LeftmostNode, KTHREAD, CfsNode);

    RtlDeleteElementRb(&RunQueue->CfsTree, &NextThread->CfsNode);

    NextThread->State = Running;
    RunQueue->CurrentThread = NextThread;

    PtpRestoreInterrupts(Flags);

    return NextThread;
}