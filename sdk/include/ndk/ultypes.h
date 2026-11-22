#pragma once

#include "ks.h"
#include "procbind.h"
#include "ultypes2.h"

typedef struct _HANDLE_TABLE {
    PHANDLE_TABLE_ENTRY Entries; // Flat array dari entries
    ULONG MaxEntries;            // Kapasitas maksimal array saat ini
    ULONG HandleCount;           // Jumlah handle yang sedang aktif
    ULONG FirstFreeIndex;        // Head dari linked list entry kosong
    KSPIN_LOCK Lock;          // Nanti diaktifkan kalau sistem thread udah siap
    PVOID UniqueProcessId;
} HANDLE_TABLE, *PHANDLE_TABLE;

typedef struct UL_WORK_QUEUE
{
    KQUEUE WorkerQueue;
    LONG DynamicThreadCount;
    ULONG WorkItemsProcessed;
    ULONG WorkItemsProcessedLastPass;
    ULONG QueueDepthLastPass;
    UL_QUEUE_WORK_INFO Info;
} UL_WORK_QUEUE, *PUL_WORK_QUEUE;

/* EOF */