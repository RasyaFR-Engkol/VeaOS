#pragma once

#include "procbind.h"
#include "rtl.h"
#include "sftypes.h"

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
    BOOLEAN Signaled;
} KEVENT, *PKEVENT;

typedef ULONG_PTR KSPIN_LOCK, *PKSPIN_LOCK;

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

typedef struct _KPROCESS {
    ULONG_PTR DirectoryTableBase;  // PENTING: Alamat fisik Page Directory (CR3) buat Context Switch
    LIST_ENTRY ThreadListHead;     // Daftar semua thread yang ada di dalam proses ini
    UCHAR State;                   // Status proses
    UCHAR BasePriority;            // Prioritas dasar untuk scheduling
    KSPIN_LOCK ProcessLock;        // Lock biar aman kalau diakses dari multicore
    PVEA_TOKEN Token;
} KPROCESS, *PKPROCESS;

typedef struct _KTHREAD {
    PVOID KernelStack;             // PENTING: Pointer stack (RSP/ESP) saat thread ini di-pause
    PVOID InitialStack;            // Batas atas stack
    PVOID StackLimit;              // Batas bawah stack (buat deteksi Stack Overflow)
    ULONGLONG VRuntime;
    ULONGLONG Weight;
    
    KTHREAD_STATE State;           // Ready? Running? Waiting?
    RTL_BALANCED_NODE CfsNode;
    LIST_ENTRY WaitListEntry;      // PENTING BUAT PNP: Kalau thread lagi nunggu disk/hardware, dia masuk antrean ini
    
    PKTRAP_FRAME TrapFrame;        // Nyimpen state register (EAX, EBX, EIP, dll) saat interrupt terjadi
    KAPC_STATE ApcState;
    PVOID Peb;
    CHAR Priority;                // Prioritas thread ini
    ULONG ContextSwitches;         // Udah berapa kali thread ini dieksekusi? (Buat statistik)
} KTHREAD, *PKTHREAD;

//
// DPC Deferred Context
//

typedef struct _KDPC *PKDPC;

typedef VOID
(VEAPI KDEFERRED_ROUTINE)(
  _In_ struct _KDPC *Dpc,
  _In_opt_ PVOID DeferredContext,
  _In_opt_ PVOID SystemArgument1,
  _In_opt_ PVOID SystemArgument2);
typedef KDEFERRED_ROUTINE *PKDEFERRED_ROUTINE;

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

typedef enum _KDPC_IMPORTANCE {
  LowImportance,
  MediumImportance,
  HighImportance,
  MediumHighImportance
} KDPC_IMPORTANCE;

//
// Timer
//
typedef struct _KTIMER
{
    LIST_ENTRY TimerListEntry;   // Entry di dalam sorted timer list global
    LARGE_INTEGER DueTime;       // Absolute due time, satuan 100-ns sejak boot
    LONG Period;                 // Interval periodik dalam ms, 0 = one-shot
    PKDPC Dpc;                   // DPC yang di-queue kalau timer expired (OPTIONAL)
    BOOLEAN Active;              // TRUE kalau timer lagi armed/ada di list
} KTIMER, *PKTIMER;

static inline UCHAR KeGetCurrentProcessorNumber(VOID)
{
    UCHAR ProcNum;
    __asm__ volatile("movb %%fs:0x08, %0" : "=q"(ProcNum));
    return ProcNum;
}

static inline VOID KeSetCurrentIrql(UCHAR NewIrql)
{
    __asm__ volatile("movb %0, %%fs:0x09" : : "q"(NewIrql));
}

static inline UCHAR KeGetCurrentIrql(VOID)
{
    UCHAR Irql;
    __asm__ volatile("movb %%fs:0x09, %0" : "=q"(Irql));
    return Irql;
}

VOID
VEAPI
KsRegisterSystemInterruptHandler(
    PVOID Handler,
    ULONG Vector
);

KIRQL
VEAPI
KeRaiseIrql(KIRQL NewIrql);

VOID
VEAPI
KeLowerIrql(KIRQL NewIrql);

typedef struct _KREGISTER_FRAME KREGISTER_FRAME, *PKREGISTER_FRAME;
VOID
VEAFAST
KsUpdateSystemTime(
    IN PKREGISTER_FRAME RegisterFrame,
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
// From ReactOS. Object Type
//

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
