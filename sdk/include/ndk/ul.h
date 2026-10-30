#pragma once

#include <ldrtypes.h>
#include "veastatus.h"
#include "ultypes.h"
#include <mm.h>

void ul_initialize_layer(PBLOCK_BOOT_2 block_boot);

VEASTATUS
VEAPI
UlDestroyHandle(
    PHANDLE_TABLE Table,
    HANDLE Handle
);

HANDLE
VEAPI
UlCreateHandle(
    PHANDLE_TABLE Table,
    PVOID Object,
    ULONG GrantedAccess
);

PHANDLE_TABLE
VEAPI
UlCreateHandleTable(
    ULONG InitialCapacity
);

VOID
VEAPI
UlPhase1(VOID);

PVOID 
VEAPI 
UlAllocatePoolWithTag(POOL_TYPE PoolType, ULONG NumberOfBytes, ULONG Tag);

VOID 
VEAPI 
UlFreePoolWithTag(PVOID P, ULONG Tag);

VOID
VEAPI
UlpInitializeWorkerThreads(VOID);

#define UlInitializeWorkItem(Item, Routine, Context) \
{ \
    (Item)->WorkerRoutine = Routine; \
    (Item)->Parameter = Context; \
    (Item)->List.Flink = NULL; \
}

VOID
VEAPI
UlQueueWorkItem(
    IN PUL_WORK_QUEUE_ITEM WorkItem,
    IN WORK_QUEUE_TYPE WorkType
);

FORCEINLINE
VOID
InitializeSListHead(
  _Out_ PSLIST_HEADER SListHead)
{
#if defined(_WIN64)
    if (((ULONG_PTR)SListHead & 0xf) != 0) {
        ExRaiseStatus(STATUS_DATATYPE_MISALIGNMENT);
    }
#if defined(_IA64_)
    SListHead->Region = (ULONG_PTR)SListHead & VRN_MASK;
#else
    SListHead->Region = 0;
#endif /* _IA64_ */
#endif /* _WIN64 */
    SListHead->Alignment = 0;
}

#define UlInitializeSListHead InitializeSListHead

PLIST_ENTRY
VEAFAST
UlfInterlockedInsertTailList(
    IN OUT PLIST_ENTRY ListHead,
    IN PLIST_ENTRY ListEntry,
    IN OUT PKSPIN_LOCK Lock
);

#define UlInterlockedInsertTailList UlfInterlockedInsertTailList

VOID
VEAPI
UlpInitLookasideList(VOID);

VOID
VEAPI
UlInitPoolLookasidePointers(VOID);

PSINGLE_LIST_ENTRY
VEAFAST
InterlockedPopEntrySList(
    IN PSLIST_HEADER ListHead
);

PSINGLE_LIST_ENTRY
VEAFAST
ExInterlockedPopEntrySList(
    IN PSLIST_HEADER ListHead,
    IN PKSPIN_LOCK Lock
);

PSINGLE_LIST_ENTRY
VEAFAST
InterlockedPushEntrySList(
    IN PSLIST_HEADER ListHead,
    IN PSINGLE_LIST_ENTRY ListEntry
);

PSINGLE_LIST_ENTRY
VEAFAST
ExInterlockedPushEntrySList(
    IN PSLIST_HEADER ListHead,
    IN PSINGLE_LIST_ENTRY ListEntry,
    IN PKSPIN_LOCK Lock
);

#define UlQueryDepthSList(SListHead) (SListHead)->Depth

static __inline
PVOID
UlAllocateFromPagedLookasideList(
    IN OUT PPAGED_LOOKASIDE_LIST Lookaside
)
{
    PVOID Entry;

    Lookaside->L.TotalAllocates++;

    Entry = InterlockedPopEntrySList(&Lookaside->L.ListHead);
    if (Entry == NULL)
    {
        Lookaside->L.AllocateMisses++;
        Entry = (Lookaside->L.Allocate)(Lookaside->L.Type,
                                    Lookaside->L.Size,
                                    Lookaside->L.Tag);
    }

    return Entry;
}

static __inline
VOID
UlFreeToPagedLookasideList(
    IN OUT PPAGED_LOOKASIDE_LIST Lookaside,
    IN PVOID Entry
)
{
    Lookaside->L.TotalFrees++;
    if (UlQueryDepthSList(&Lookaside->L.ListHead) >= Lookaside->L.Depth)
    {
        Lookaside->L.FreeMisses++;
        (Lookaside->L.Free)(Entry);
    }
    else
    {
        InterlockedPushEntrySList(&Lookaside->L.ListHead, (PSLIST_ENTRY)Entry);
    }
}

static __inline
PVOID
UlAllocateFromNPagedLookasideList(
    IN OUT PNPAGED_LOOKASIDE_LIST Lookaside
)
{
    PVOID Entry;

    Lookaside->L.TotalAllocates++;
    Entry = InterlockedPopEntrySList(&Lookaside->L.ListHead);
    if (Entry == NULL)
    {
        Lookaside->L.AllocateMisses++;
        Entry = (Lookaside->L.Allocate)(Lookaside->L.Type,
                                        Lookaside->L.Size,
                                        Lookaside->L.Tag);
    }

    return Entry;
}

static __inline
VOID
UlFreeToNPagedLookasideList(
    IN OUT PNPAGED_LOOKASIDE_LIST Lookaside,
    IN PVOID Entry
)
{
    Lookaside->L.TotalFrees++;

    if (UlQueryDepthSList(&Lookaside->L.ListHead) >= Lookaside->L.Depth) {
        Lookaside->L.FreeMisses++;
        (Lookaside->L.Free)(Entry);
    } else {
        InterlockedPushEntrySList(&Lookaside->L.ListHead, (PSLIST_ENTRY)Entry);
    }
}

VOID
VEAPI
UlRaiseStatus(
    IN VEASTATUS Status
);


/* EOF */