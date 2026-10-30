#pragma once

#include "mmtype.h"
#include "procbind.h"
#include "psdk/veaseh.h"
#include "rtl.h"
#include "sftypes.h"
#include "veastatus.h"

// #define SPINLOCK_DBG 1

typedef ULONG_PTR CAPABILITY("mutex") KSPIN_LOCK, *PKSPIN_LOCK;

/* Simplified Dispatcher Header */
typedef struct _DISPATCHER_HEADER
{
    UCHAR Type;
    UCHAR Abandoned;
    BOOLEAN Signaling;
    BOOLEAN Inserted;
    UCHAR Size;
    UCHAR Hand;
    BOOLEAN DpcActive;
    volatile KSPIN_LOCK Lock;
    LONG SignalState;
    LIST_ENTRY WaitListHead;
} DISPATCHER_HEADER, *PDISPATCHER_HEADER;

VOID
VEAPI
KsBugCheckEx(
    ULONG BugCheckCode,
    ULONG_PTR BugCheckParameter1,
    ULONG_PTR BugCheckParameter2,
    ULONG_PTR BugCheckParameter3,
    ULONG_PTR BugCheckParameter4
);

VOID
VEAPI
KsBugCheck(
    ULONG BugCheckCode
);

typedef struct _KEVENT
{
    DISPATCHER_HEADER Header;
} KEVENT, *PKEVENT;

typedef struct _EVENT_COUNTER {
    ULONG RefCount;
    KEVENT Event;
} EVENT_COUNTER, *PEVENT_COUNTER;

typedef struct _KAPC_STATE
{
    LIST_ENTRY ApcListHead[2];
    struct _KPROCESS *Process;
    BOOLEAN KernelApcInProgress;
    BOOLEAN KernelApcPending;
    BOOLEAN UserApcPending;
} KAPC_STATE, *PKAPC_STATE;

typedef enum _KTHREAD_STATE {
    Initialized,
    Ready,
    Running,
    Standby,
    Terminated,
    Waiting
} KTHREAD_STATE;

typedef enum _KWAIT_TYPE {
    WaitAny,
    WaitAll
} KWAIT_TYPE;

typedef struct _KWAIT_BLOCK
{
    LIST_ENTRY WaitListEntry;
    struct _KTHREAD *Thread;
    PVOID Object;
    PVOID WaitResult;
    USHORT WaitKey;
    UCHAR WaitType;
} KWAIT_BLOCK, *PKWAIT_BLOCK;
#define THREAD_WAIT_OBJECTS 3

typedef struct _KQUEUE KQUEUE, *PKQUEUE;

typedef struct _KPROCESS {
    ULONG_PTR DirectoryTableBase;  // PENTING: Alamat fisik Page Directory (CR3) buat Context Switch
    LIST_ENTRY ThreadListHead;     // Daftar semua thread yang ada di dalam proses ini
    UCHAR State;                   // Status proses
    UCHAR BasePriority;            // Prioritas dasar untuk scheduling
    KSPIN_LOCK ProcessLock;        // Lock biar aman kalau diakses dari multicore
    PVEA_TOKEN Token;
    ULONG StackCount;
} KPROCESS, *PKPROCESS;

//
// KDPC Routine
//
struct _KDPC;

typedef VOID
(VEAPI KDEFERRED_ROUTINE)(
  _In_ struct _KDPC *Dpc,
  _In_opt_ PVOID DeferredContext,
  _In_opt_ PVOID SystemArgument1,
  _In_opt_ PVOID SystemArgument2);
typedef KDEFERRED_ROUTINE *PKDEFERRED_ROUTINE;

//
// KDPC
//

typedef struct _KDPC {
  UCHAR Type;
  UCHAR Importance;
  volatile USHORT Number;
  LIST_ENTRY DpcListEntry;
  PKDEFERRED_ROUTINE DeferredRoutine;
  PVOID DeferredContext;
  PVOID SystemArgument1;
  PVOID SystemArgument2;
  volatile PVOID DpcData;
} KDPC, *PKDPC;

//
// Timer
//
typedef struct _KTIMER
{
    DISPATCHER_HEADER Header;
    LIST_ENTRY TimerListEntry;   // Entry di dalam sorted timer list global
    LARGE_INTEGER DueTime;       // Absolute due time, satuan 100-ns sejak boot
    LONG Period;                 // Interval periodik dalam ms, 0 = one-shot
    PKDPC Dpc;                   // DPC yang di-queue kalau timer expired (OPTIONAL)
    BOOLEAN Active;              // TRUE kalau timer lagi armed/ada di list
} KTIMER, *PKTIMER;

struct _KAPC;

typedef VOID
(VEAPI *PKNORMAL_ROUTINE)(
  IN PVOID NormalContext OPTIONAL,
  IN PVOID SystemArgument1 OPTIONAL,
  IN PVOID SystemArgument2 OPTIONAL);

typedef VOID
(VEAPI *PKRUNDOWN_ROUTINE)(
  IN struct _KAPC *Apc);

typedef VOID (VEAPI *PKKERNEL_ROUTINE)(
    IN struct _KAPC *Apc,
    IN OUT PKNORMAL_ROUTINE *NormalRoutine OPTIONAL,
    IN OUT PVOID *NormalContext OPTIONAL,
    IN OUT PVOID *SystemArgument1 OPTIONAL,
    IN OUT PVOID *SystemArgument2 OPTIONAL
);

typedef struct _KAPC
{
    UCHAR Type;
    UCHAR Size;
    struct _KTHREAD *Thread;
    LIST_ENTRY ApcListEntry;
    PKKERNEL_ROUTINE KernelRoutine;
    PKRUNDOWN_ROUTINE RundownRoutine;
    PKNORMAL_ROUTINE NormalRoutine;
    PVOID NormalContext;
    PVOID SystemArgument1;
    PVOID SystemArgument2;
    CHAR ApcStateIndex;
    KPROCESSOR_MODE ApcMode;
    BOOLEAN Inserted;
} KAPC, *PKAPC;

typedef struct _KTHREAD {
    PVOID KernelStack;
    PVOID InitialStack;             // Batas atas stack
    PVOID StackLimit;               // Batas bawah stack (buat deteksi Stack Overflow)
    ULONGLONG VRuntime;
    ULONGLONG Weight;
    KTHREAD_STATE State;            // Ready? Running? Waiting?
    RTL_BALANCED_NODE CfsNode;
    KWAIT_BLOCK WaitBlock[THREAD_WAIT_OBJECTS];
    PKWAIT_BLOCK WaitBlockList;
    PKQUEUE Queue;
    LIST_ENTRY WaitListEntry;       // Assume this needed
    LIST_ENTRY MutantListHead;
    KTIMER Timer;
    VEASTATUS WaitStatus;
    UCHAR WaitType;
    BOOLEAN WaitNext;
    KIRQL WaitIrql;
    KIRQL Irql;
    PKTRAP_FRAME TrapFrame;         // Nyimpen state register (EAX, EBX, EIP, dll) saat interrupt terjadi
    KAPC_STATE ApcState;
    PVOID Peb;
    CHAR Priority;                  // Prioritas thread ini
    ULONG ContextSwitches;          // Udah berapa kali thread ini dieksekusi? (Buat statistik)
    SHORT KernelApcDisable;         
    SHORT SpecialApcDisable;        
    KAPC_STATE SavedApcState;
    PKAPC_STATE ApcStatePointer[2];
    UCHAR ApcStateIndex;
    KSPIN_LOCK ApcQueueLock;
    BOOLEAN ApcQueueable;
    BOOLEAN Alertable;
    KAPC SuspendApc;
    KPROCESSOR_MODE WaitMode;
    PVOID ServiceTable;
    KPROCESSOR_MODE PreviousMode;
} KTHREAD, *PKTHREAD;

typedef enum _KDPC_IMPORTANCE {
  LowImportance,
  MediumImportance,
  HighImportance,
  MediumHighImportance
} KDPC_IMPORTANCE;

static inline UCHAR KeGetCurrentProcessorNumber(VOID)
{
    UCHAR ProcNum;
    __asm__ volatile("movb %%fs:0x08, %0" : "=q"(ProcNum));
    return ProcNum;
}


VOID VEAPI HctSetIrql(KIRQL Irql);
static inline VOID KeSetCurrentIrql(UCHAR NewIrql)
{
    HctSetIrql(NewIrql);
}

static inline UCHAR KeGetCurrentIrql(VOID)
{
    UCHAR Irql;
    __asm__ volatile("movb %%fs:0x24, %0" : "=q"(Irql));
    return Irql;
}

VOID
VEAPI
KsRegisterSystemInterruptHandler(
    PVOID Handler,
    ULONG Vector
);

// #define KIRQL_DEBUG
#if defined (KIRQL_DEBUG)

KIRQL
VEAPI
KeRaiseIrqlInternal(
    KIRQL NewIrql,
    PCHAR Function,
    INT Line,
    PCHAR File
);

VOID
VEAPI
KeLowerIrqlInternal(
    KIRQL NewIrql,
    PCHAR Function,
    INT Line,
    PCHAR File
);

#define KeRaiseIrql(Irql) \
    KeRaiseIrqlInternal(Irql, __FUNCTION__, __LINE__, __FILE__)

#define KeLowerIrql(Irql) \
    KeLowerIrqlInternal(Irql, __FUNCTION__, __LINE__, __FILE__)

#else

KIRQL
VEAPI
KeRaiseIrql(KIRQL NewIrql);

VOID
VEAPI
KeLowerIrql(KIRQL NewIrql);

#endif

typedef struct _KREGISTER_FRAME KREGISTER_FRAME, *PKREGISTER_FRAME;
VOID
VEAFAST
KsUpdateSystemTime(
    IN PKTRAP_FRAME RegisterFrame,
    IN ULONG Increment,
    IN KIRQL Irql
);

VOID
VEAPI
KsiInitializeTimerList(VOID);

BOOLEAN
VEAPI
KsCancelTimer(
    IN OUT PKTIMER Timer
);

BOOLEAN
VEAPI
KsSetTimer(
    IN OUT PKTIMER Timer,
    IN LARGE_INTEGER DueTime,
    IN LONG Period OPTIONAL,
    IN PKDPC Dpc OPTIONAL
);

VOID
VEAPI
KsInitializeTimer(
    OUT PKTIMER Timer
);

ULONGLONG
VEAPI
KsQuerySystemTime(VOID);

ULONGLONG
VEAPI
KsQuerySystemTimeNs(VOID);

VOID
VEAPI
KsStallExecutionProcessor(
    IN ULONG Microseconds
);

ULONGLONG
VEAPI
KsiAdvanceSystemTime(
    IN ULONG Increment
);

VOID
VEAFAST
KsiDispatchInterrupt(VOID);

typedef struct _BLOCK_BOOT_2 BLOCK_BOOT_2, *PBLOCK_BOOT_2;

extern PBLOCK_BOOT_2 KsLoaderBlock;

/* BV */
VOID
VEAPI
BvInitializeDriver(IN PBLOCK_BOOT_2 BlockBoot2);

//
// From ReactOS. Object Type, Event Type
//

typedef enum _EVENT_TYPE {
  NotificationEvent,
  SynchronizationEvent
} EVENT_TYPE;

typedef enum _KOBJECTS
{
    EventNotificationObject = 0,
    EventSynchronizationObject = 1,
    MutantObject = 2,
    ProcessObject = 3,
    QueueObject = 4,
    SemaphoreObject = 5,
    ThreadObject = 6,
    GateObject = 7,
    TimerNotificationObject = 8,
    TimerSynchronizationObject = 9,
    Spare2Object = 10,
    Spare3Object = 11,
    Spare4Object = 12,
    Spare5Object = 13,
    Spare6Object = 14,
    Spare7Object = 15,
    Spare8Object = 16,
    Spare9Object = 17,
    ApcObject = 18,
    DpcObject = 19,
    DeviceQueueObject = 20,
    EventPairObject = 21,
    InterruptObject = 22,
    ProfileObject = 23,
    ThreadedDpcObject = 24,
    MaximumKernelObject = 25
} KOBJECTS;

VOID
VEAPI
KsInitializeDpc(
    IN PKDPC Dpc,
    IN PKDEFERRED_ROUTINE DeferredRoutine,
    IN PVOID DeferredContext
);

extern KDPC KiTimerExpireDpc;

VOID
VEAPI
KsInitializeSpinlock(IN PKSPIN_LOCK Lock);

#if defined(SPINLOCK_DBG)

VOID
VEAPI
KsAcquireSpinLockInternal(IN PKSPIN_LOCK Lock, OUT PKIRQL OldIrql, const PCHAR Function, const PCHAR File, INT Line);
#define KsAcquireSpinLock(Lock, Irql) \
    KsAcquireSpinLockInternal(Lock, Irql, __FUNCTION__, __FILE__, __LINE__)

VOID
VEAPI
KsReleaseSpinLockInternal(IN PKSPIN_LOCK Lock, IN KIRQL OldIrql, const PCHAR Function, const PCHAR File, INT Line);
#define KsReleaseSpinLock(Lock, Irql) \
    KsReleaseSpinLockInternal(Lock, Irql, __FUNCTION__, __FILE__, __LINE__)

VOID
VEAPI
KsAcquireSpinLockAtDpcLevelInternal(IN OUT PKSPIN_LOCK Lock, const PCHAR Function, const PCHAR File, INT Line);
#define KsAcquireSpinLockAtDpcLevel(Lock) \
    KsAcquireSpinLockAtDpcLevelInternal(Lock, __FUNCTION__, __FILE__, __LINE__)

VOID
VEAPI
KsReleaseSpinLockFromDpcLevelInternal(IN OUT PKSPIN_LOCK Lock, const PCHAR Function, const PCHAR File, INT Line);
#define KsReleaseSpinLockFromDpcLevel(Lock) \
    KsReleaseSpinLockFromDpcLevelInternal(Lock, __FUNCTION__, __FILE__, __LINE__)

#else

VOID
VEAPI
KsAcquireSpinLock(
    IN PKSPIN_LOCK Lock, 
    OUT PKIRQL OldIrql
) ACQUIRE(Lock);

VOID
VEAPI
KsReleaseSpinLock(
    IN PKSPIN_LOCK Lock,
    IN KIRQL OldIrql
) RELEASE(Lock);

VOID
VEAPI
KsAcquireSpinLockAtDpcLevel(
    IN OUT PKSPIN_LOCK Lock
) ACQUIRE(Lock);

VOID 
VEAPI
KsReleaseSpinLockFromDpcLevel(
    IN OUT PKSPIN_LOCK Lock
) RELEASE(Lock);

#endif /* SPINLOCK_DBG */

BOOLEAN
VEAPI
KsTryToAcquireSpinLockAtDpcLevel(IN OUT PKSPIN_LOCK Lock);

BOOLEAN
VEAPI
KsTryToAcquireSpinLock(IN OUT PKSPIN_LOCK Lock, OUT PKIRQL OldIrql);

typedef struct _KPRCB *PKPRCB;

VOID
VEAPI
KsiRetireDpcList(IN PKPRCB Prcb);

BOOLEAN
VEAPI
KsInsertQueueDpc(
    IN PKDPC Dpc,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);

VOID
VEAPI
KsiTimerExpirationDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);

extern KDPC KiTimerExpireDpc;

struct _KQUEUE
{
    DISPATCHER_HEADER Header;
    LIST_ENTRY EntryListHead;
    volatile ULONG CurrentCount;
    ULONG MaximumCount;
    LIST_ENTRY ThreadListHead;
};

VEASTATUS
VEAPI
KsWaitForSingleObject(
    IN PVOID Object
);

VEASTATUS
VEAPI
KsWaitForMultipleObjects(
    IN ULONG ObjectCount,
    IN PVOID *Objects,
    IN KWAIT_TYPE WaitType
);

VOID
VEAPI
KsWaitTest(
    IN PDISPATCHER_HEADER SignaledObject
);

VOID
VEAPI
KsInitializeQueue(
    IN PKQUEUE Queue,
    IN ULONG Count OPTIONAL
);

PLIST_ENTRY
VEAPI
KsInsertQueue(
    IN PKQUEUE Queue,
    IN PLIST_ENTRY ListEntry
);

PLIST_ENTRY
VEAPI
KsRemoveQueue(
    IN PKQUEUE Queue,
    IN KPROCESSOR_MODE WaitMode,
    IN PLARGE_INTEGER Timeout OPTIONAL
);

KIRQL
VEAPI
KsAcquireDispatcherLock(VOID);

VOID
VEAPI
KsReleaseDispatcherLock(KIRQL OldIrql);

LONG
VEAPI
KsSetEvent(
    IN PKEVENT Event,
    IN BOOLEAN Wait
);

LONG
VEAPI
KsResetEvent(
    IN PKEVENT Event
);

VOID
VEAPI
KsInitializeEvent(
    IN OUT PKEVENT Event,
    IN EVENT_TYPE Type,
    IN BOOLEAN State
);

VOID
VEAPI
KsClearEvent(PKEVENT Event);

//
// Kernel Mutant & Mutex
//

typedef struct _KMUTANT
{
    DISPATCHER_HEADER Header;
    LIST_ENTRY MutantListEntry;
    PKTHREAD OwnerThread;
    BOOLEAN Abandoned;
    UCHAR ApcDisable;
} KMUTANT, *PKMUTANT, KMUTEX, *PKMUTEX;

VOID
VEAPI
KsInitializeMutant(
    IN PKMUTANT Mutant,
    IN BOOLEAN InitialOwner
);

VOID
VEAPI
KsInitializeMutex(
    IN PKMUTEX Mutex,
    IN ULONG Level
);

LONG
VEAPI
KeReadStateMutant(
    IN PKMUTANT Mutant
);

LONG
VEAPI
KsReleaseMutant(
    IN PKMUTANT Mutant,
    IN BOOLEAN Wait
);

LONG
VEAPI
KsReleaseMutex(
    IN PKMUTEX Mutex,
    IN BOOLEAN Wait
);

//
// Kernel Semaphore
//

typedef struct _KSEMAPHORE {
    DISPATCHER_HEADER Header;
    LONG Limit;
} KSEMAPHORE, *PKSEMAPHORE;

VOID
VEAPI
KsInitializeSemaphore(
    IN PKSEMAPHORE Semaphore,
    IN LONG Count,
    IN LONG Limit
);

LONG
VEAPI
KsReadStateSemaphore(
    IN PKSEMAPHORE Semaphore
);

LONG
VEAPI
KsReleaseSemaphore(
    IN PKSEMAPHORE Semaphore,
    IN LONG Adjustment,
    IN BOOLEAN Wait
);

#define FAST_FAIL_STACK_COOKIE_CHECK_FAILED 0x02

VOID
VEAPI
KsMemoryBarrierWithoutFence(VOID);

typedef struct _LOOKASIDE_LIST_EX *PLOOKASIDE_LIST_EX;

typedef PVOID
(VEAPI *PALLOCATE_FUNCTION_EX)(
    _In_ POOL_TYPE PoolType,
    _In_ SIZE_T NumberOfBytes,
    _In_ ULONG Tag,
    _Inout_ PLOOKASIDE_LIST_EX Lookaside);

typedef PVOID
(VEAPI *PALLOCATE_FUNCTION)(
    _In_ POOL_TYPE PoolType,
    _In_ SIZE_T NumberOfBytes,
    _In_ ULONG Tag);

typedef VOID
(VEAPI *PFREE_FUNCTION_EX)(
    _In_ PVOID Buffer,
    _Inout_ PLOOKASIDE_LIST_EX Lookaside);

typedef VOID
(VEAPI *PFREE_FUNCTION)(
    _In_ PVOID Buffer);

#define GENERAL_LOOKASIDE_LAYOUT                \
    _ANONYMOUS_UNION union {                    \
        SLIST_HEADER ListHead;                  \
        SINGLE_LIST_ENTRY SingleListHead;       \
    } DUMMYUNIONNAME;                           \
    USHORT Depth;                               \
    USHORT MaximumDepth;                        \
    ULONG TotalAllocates;                       \
    _ANONYMOUS_UNION union {                    \
        ULONG AllocateMisses;                   \
        ULONG AllocateHits;                     \
    } DUMMYUNIONNAME2;                          \
    ULONG TotalFrees;                           \
    _ANONYMOUS_UNION union {                    \
        ULONG FreeMisses;                       \
        ULONG FreeHits;                         \
    } DUMMYUNIONNAME3;                          \
    POOL_TYPE Type;                             \
    ULONG Tag;                                  \
    ULONG Size;                                 \
    _ANONYMOUS_UNION union {                    \
        PALLOCATE_FUNCTION_EX AllocateEx;       \
        PALLOCATE_FUNCTION Allocate;            \
    } DUMMYUNIONNAME4;                          \
    _ANONYMOUS_UNION union {                    \
        PFREE_FUNCTION_EX FreeEx;               \
        PFREE_FUNCTION Free;                    \
    } DUMMYUNIONNAME5;                          \
    LIST_ENTRY ListEntry;                       \
    ULONG LastTotalAllocates;                   \
    _ANONYMOUS_UNION union {                    \
        ULONG LastAllocateMisses;               \
        ULONG LastAllocateHits;                 \
    } DUMMYUNIONNAME6;                          \
    ULONG Future[2];


typedef struct _GENERAL_LOOKASIDE {
    GENERAL_LOOKASIDE_LAYOUT
} GENERAL_LOOKASIDE, *PGENERAL_LOOKASIDE;

typedef struct _GENERAL_LOOKASIDE_POOL {
    GENERAL_LOOKASIDE_LAYOUT
} GENERAL_LOOKASIDE_POOL, *PGENERAL_LOOKASIDE_POOL;

typedef struct _PP_LOOKASIDE_LIST
{
    struct _GENERAL_LOOKASIDE *P;
    struct _GENERAL_LOOKASIDE *L;
} PP_LOOKASIDE_LIST, *PPP_LOOKASIDE_LIST;

typedef struct _LOOKASIDE_LIST_EX {
    GENERAL_LOOKASIDE_POOL L;
} LOOKASIDE_LIST_EX;

typedef struct _NPAGED_LOOKASIDE_LIST {
    GENERAL_LOOKASIDE L;
#if !defined(_AMD64_) && !defined(_IA64_)
    KSPIN_LOCK Lock__ObsoleteButDoNotDelete;
#endif
} NPAGED_LOOKASIDE_LIST, *PNPAGED_LOOKASIDE_LIST;

typedef struct _PAGED_LOOKASIDE_LIST {
    GENERAL_LOOKASIDE L;
#if !defined(_AMD64_) && !defined(_IA64_)
    // TODO: FAST_MUTEX Lock__ObsoleteButDoNotDelete;
#endif
} PAGED_LOOKASIDE_LIST, *PPAGED_LOOKASIDE_LIST;

#define EFLAGS_IF_BIT (1UL << 9)

FORCEINLINE
BOOLEAN
VEAPI
KsDisableInterrupts(VOID)
{
    ULONG_PTR Flags;

#if defined(__x86_64__)
    /* x86_64: Gunakan pushfq untuk simpan RFLAGS 64-bit */
    __asm__ __volatile__ (
        "pushfq\n\t"
        "pop %0\n\t"
        "cli"
        : "=r" (Flags)
        :
        : "memory"
    );
#else
    /* x86 32-bit: Gunakan pushfl untuk simpan EFLAGS 32-bit */
    __asm__ __volatile__ (
        "pushfl\n\t"
        "pop %0\n\t"
        "cli"
        : "=r" (Flags)
        :
        : "memory"
    );
#endif

    /* Cek apakah Bit 9 (IF) bernilai 1 */
    return ((Flags & EFLAGS_IF_BIT) != 0);
}

FORCEINLINE
VOID
VEAPI
KsRestoreInterrupts(IN BOOLEAN Enable)
{
    if (Enable)
    {
        __asm__ __volatile__ ("sti" ::: "memory");
    }
}

typedef enum _KAPC_ENVIRONMENT
{
    OriginalApcEnvironment,
    AttachedApcEnvironment,
    CurrentApcEnvironment,
    InsertApcEnvironment
} KAPC_ENVIRONMENT;

BOOLEAN
VEAPI
KsDpcRoutineRunning(VOID);

static inline LONG VEAPI InterlockedIncrement(PLONG Addend) {
    return __atomic_add_fetch(Addend, 1, __ATOMIC_SEQ_CST);
}

static inline LONG VEAPI InterlockedDecrement(PLONG Addend) {
    return __atomic_sub_fetch(Addend, 1, __ATOMIC_SEQ_CST);
}

VOID
VEAPI
KsDeliverApc(
    IN KPROCESSOR_MODE DeliveryMode,
    IN PKEXCEPTION_FRAME ExceptionFrame OPTIONAL,
    IN PKTRAP_FRAME TrapFrame OPTIONAL
);

VEASTATUS
VEAPI
KsDelayExecutionThread(
    IN KPROCESSOR_MODE WaitMode,
    IN BOOLEAN Alertable,
    IN PLARGE_INTEGER Interval
);

BOOLEAN
VEAPI
KsiDispatchSehException(
    IN PKTRAP_FRAME TrapFrame,
    IN VEASTATUS ExceptionCode
);

typedef struct _KSERVICE_TABLE_DESCRIPTOR
{
    PULONG_PTR Base;
    PULONG Count;
    ULONG Limit;
    PUCHAR Number;
} KSERVICE_TABLE_DESCRIPTOR, *PKSERVICE_TABLE_DESCRIPTOR;

// 1 for Thread, 1 for GUI thread?
#define SSDT_MAX_ENTRIES                2

extern KSERVICE_TABLE_DESCRIPTOR KsServiceTableDescriptor[SSDT_MAX_ENTRIES];
extern KSERVICE_TABLE_DESCRIPTOR KsServiceTableDescriptorShadow[SSDT_MAX_ENTRIES];

VOID
VEAPI
RmInitSyscall(VOID);

extern ULONG KsiServiceLimit;

/* EOF */