#pragma once

#include "ks.h"
#include "procbind.h"

typedef struct _HANDLE_TABLE_ENTRY {
    PVOID Object;          // Pointer ke Header/Body Objek (NULL jika entry ini kosong)
    ULONG GrantedAccess;   // Hak akses untuk handle ini
    ULONG NextFreeIndex;   // Index ke entry kosong berikutnya (hanya valid jika Object == NULL)
    PVOID OwnerThread;     // Thread yang menerbitkan dan memiliki handle ini
} HANDLE_TABLE_ENTRY, *PHANDLE_TABLE_ENTRY;

typedef struct _HANDLE_TABLE {
    PHANDLE_TABLE_ENTRY Entries; // Flat array dari entries
    ULONG MaxEntries;            // Kapasitas maksimal array saat ini
    ULONG HandleCount;           // Jumlah handle yang sedang aktif
    ULONG FirstFreeIndex;        // Head dari linked list entry kosong
    KSPIN_LOCK Lock;          // Nanti diaktifkan kalau sistem thread udah siap
} HANDLE_TABLE, *PHANDLE_TABLE;

#define HANDLE_TO_INDEX(h) ((ULONG)(h) >> 2)
#define INDEX_TO_HANDLE(i) ((HANDLE)((i) << 2))

typedef PVOID HANDLE, *PHANDLE;

#define TEMPORARY_MAX_HANDLE 100

//
// This is for Kernel Worker
//

typedef enum _WORK_QUEUE_TYPE
{
    CriticalWorkQueue,
    DelayedWorkQueue,
    HyperCriticalWorkQueue,
    MaximumWorkQueue
} WORK_QUEUE_TYPE, *PWORK_QUEUE_TYPE;

//
// Ulp Worker Structure Information
//

typedef struct _UL_QUEUE_WORK_TYPE
{
    ULONG QueueDisabled:1;
    ULONG MakeThreadsAsNecessary:1;
    ULONG WaitMode:1;
    ULONG WorkerCount:29;
} UL_QUEUE_WORK_INFO, *PUL_QUEUE_WORK_INFO;

typedef struct UL_WORK_QUEUE
{
    KQUEUE WorkerQueue;
    LONG DynamicThreadCount;
    ULONG WorkItemsProcessed;
    ULONG WorkItemsProcessedLastPass;
    ULONG QueueDepthLastPass;
    UL_QUEUE_WORK_INFO Info;
} UL_WORK_QUEUE, *PUL_WORK_QUEUE;

typedef VOID 
(VEAPI WORKER_THREAD_ROUTINE)(
    IN PVOID Parameter);
typedef WORKER_THREAD_ROUTINE *PWORKER_THREAD_ROUTINE;

typedef struct _UL_WORK_QUEUE_ITEM {
    LIST_ENTRY List;
    PWORKER_THREAD_ROUTINE WorkerRoutine;
    volatile PVOID Parameter;
} UL_WORK_QUEUE_ITEM, *PUL_WORK_QUEUE_ITEM;

#define EVENT_QUERY_STATE (0x0001)
#define EVENT_MODIFY_STATE (0x0002)
#define EVENT_ALL_ACCESS (VEA_STANDARD_RIGHTS_REQUIRED | VEA_SYNCHRONIZE | 0x3)

extern UCHAR UlpInitializationPhase;
