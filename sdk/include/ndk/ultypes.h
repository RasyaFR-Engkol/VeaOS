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

#define TIMER_QUERY_STATE                   0x0001
#define TIMER_MODIFY_STATE                  0x0002
#define TIMER_ALL_ACCESS                    (VEA_STANDARD_RIGHTS_REQUIRED | \
                                             VEA_SYNCHRONIZE | \
                                             TIMER_QUERY_STATE | \
                                             TIMER_MODIFY_STATE)

typedef struct _ETIMER
{
    KTIMER KeTimer;
    KDPC TimerDpc;
    KSPIN_LOCK Lock;
    LIST_ENTRY ActiveTimerListEntry;
    BOOLEAN ApcAssociated;
} ETIMER, *PETIMER;

typedef enum _TIMER_TYPE
{
    NotificationTimer,
    SynchronizationTimer
} TIMER_TYPE;

typedef VOID
(VEAPI *PTIMER_APC_ROUTINE)
(
    IN PVOID TimerContext,
    IN ULONG TimerLowValue,
    IN LONG TimerHighValue
);

typedef struct _TIMER_BASIC_INFORMATION
{
    LARGE_INTEGER TimeRemaining;
    BOOLEAN SignalState;
} TIMER_BASIC_INFORMATION, *PTIMER_BASIC_INFORMATION;

typedef enum _TIMER_INFORMATION_CLASS
{
    TimerBasicInformation
} TIMER_INFORMATION_CLASS;

/* EOF */