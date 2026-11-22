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
    IN PHANDLE_TABLE Table,
    IN PHANDLE_TABLE_ENTRY HandleTableEntry
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
#if defined(_VEA64B_)
    if (((ULONG_PTR)SListHead & 0xf) != 0) {
        ExRaiseStatus(STATUS_DATATYPE_MISALIGNMENT);
    }
#if defined(_IA64_)
    SListHead->Region = (ULONG_PTR)SListHead & VRN_MASK;
#else
    SListHead->Region = 0;
#endif /* _IA64_ */
#endif /* _VEA64B_ */
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

BOOLEAN
VEAPI
UlInitializeEventImplementation(VOID);

VOID
VEAPI
ProbeForRead(
    IN const PVOID Address,
    IN SIZE_T Length,
    IN ULONG Alignment
);

VOID
VEAPI
ProbeForWrite(IN PVOID Address,
              IN SIZE_T Length,
              IN ULONG Alignment);

PHANDLE_TABLE
VEAPI
UlDupHandleTable(
    IN PEPROCESS TargetProcess,
    IN PHANDLE_TABLE SourceTable
);

VEASTATUS
VEAPI
VeaQuerySystemInformation(
    IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
    OUT PVOID SystemInformation,
    IN ULONG SystemInformationLength,
    OUT PULONG ReturnedLength
);

static const UNICODE_STRING __emptyUnicodeString = {0, 0, NULL};
static const LARGE_INTEGER __emptyLargeInteger = {{0, 0}};
static const ULARGE_INTEGER __emptyULargeInteger = {{0, 0}};
static const OIP_INFORMATION __emptyIoStatusBlock = {{0}, 0};

#define ProbeForReadGenericType(Ptr, Type, Default)                            \
    (((ULONG_PTR)(Ptr) + sizeof(Type) - 1 < (ULONG_PTR)(Ptr) ||                \
     (ULONG_PTR)(Ptr) + sizeof(Type) - 1 >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS) ?   \
         UlRaiseStatus(STATUS_ACCESS_VIOLATION), Default :                     \
         *(const volatile Type *)(Ptr))

#define ProbeForReadBoolean(Ptr) ProbeForReadGenericType(Ptr, BOOLEAN, FALSE)
#define ProbeForReadUchar(Ptr) ProbeForReadGenericType(Ptr, UCHAR, 0)
#define ProbeForReadChar(Ptr) ProbeForReadGenericType(Ptr, CHAR, 0)
#define ProbeForReadUshort(Ptr) ProbeForReadGenericType(Ptr, USHORT, 0)
#define ProbeForReadShort(Ptr) ProbeForReadGenericType(Ptr, SHORT, 0)
#define ProbeForReadUlong(Ptr) ProbeForReadGenericType(Ptr, ULONG, 0)
#define ProbeForReadLong(Ptr) ProbeForReadGenericType(Ptr, LONG, 0)
#define ProbeForReadUint(Ptr) ProbeForReadGenericType(Ptr, UINT, 0)
#define ProbeForReadInt(Ptr) ProbeForReadGenericType(Ptr, INT, 0)
#define ProbeForReadUlonglong(Ptr) ProbeForReadGenericType(Ptr, ULONGLONG, 0)
#define ProbeForReadLonglong(Ptr) ProbeForReadGenericType(Ptr, LONGLONG, 0)
#define ProbeForReadPointer(Ptr) ProbeForReadGenericType(Ptr, PVOID, NULL)
#define ProbeForReadHandle(Ptr) ProbeForReadGenericType(Ptr, HANDLE, NULL)
#define ProbeForReadLangId(Ptr) ProbeForReadGenericType(Ptr, LANGID, 0)
#define ProbeForReadSize_t(Ptr) ProbeForReadGenericType(Ptr, SIZE_T, 0)
#define ProbeForReadLargeInteger(Ptr) ProbeForReadGenericType((const LARGE_INTEGER *)(Ptr), LARGE_INTEGER, __emptyLargeInteger)
#define ProbeForReadUlargeInteger(Ptr) ProbeForReadGenericType((const ULARGE_INTEGER *)(Ptr), ULARGE_INTEGER, __emptyULargeInteger)
#define ProbeForReadUnicodeString(Ptr) ProbeForReadGenericType((const UNICODE_STRING *)(Ptr), UNICODE_STRING, __emptyUnicodeString)

VEASTATUS
VEAPI
VeaSetSystemInformation(
    IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
    IN PVOID SystemInformation,
    IN ULONG SystemInformationLength
);

BOOLEAN
VEAPI
UlVeapiInit(VOID);

extern POBJECT_TYPE UlWindowStationObjectType;
extern POBJECT_TYPE UlDesktopObjectType;

VEASTATUS
VEAPI
VeaSetEvent(
    IN HANDLE EventHandle,
    OUT PLONG PreviousState OPTIONAL
);

VEASTATUS
VEAPI
VeaCreateEvent(
    OUT PHANDLE EventHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN EVENT_TYPE EventType,
    IN BOOLEAN InitialState
);


/* EOF */