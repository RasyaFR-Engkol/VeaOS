#pragma once

#include <procbind.h>
#include <veastatus.h>

typedef struct _RTL_CRITICAL_SECTION_DEBUG
{
    USHORT     Type;
    USHORT     CreatorBackTraceIndex;
    struct _RTL_CRITICAL_SECTION *CriticalSection;
    LIST_ENTRY ProcessLocksList;
    ULONG      EntryCount;
    ULONG      ContentionCount;
    ULONG      Spare[2];
} RTL_CRITICAL_SECTION_DEBUG, *PRTL_CRITICAL_SECTION_DEBUG;

typedef struct _RTL_CRITICAL_SECTION
{
    PRTL_CRITICAL_SECTION_DEBUG DebugInfo;
    LONG      LockCount;            /* -1 = bebas */
    LONG      RecursionCount;
    HANDLE    OwningThread;         /* thread id pemilik */
    HANDLE    LockSemaphore;        /* event/semaphore untuk menunggu, dibuat lazy */
    ULONG_PTR SpinCount;
} RTL_CRITICAL_SECTION, *PRTL_CRITICAL_SECTION;

#define RTL_CS_SPIN_MASK            0x00FFFFFFUL
#define RTL_CS_PREALLOCATE_EVENT    0x80000000UL

VEASTATUS VEAPI RtlInitializeCriticalSection(IN OUT PRTL_CRITICAL_SECTION CriticalSection);
VEASTATUS VEAPI RtlInitializeCriticalSectionAndSpinCount(IN OUT PRTL_CRITICAL_SECTION CriticalSection, IN ULONG SpinCount);
VEASTATUS VEAPI RtlDeleteCriticalSection(IN OUT PRTL_CRITICAL_SECTION CriticalSection);
VEASTATUS VEAPI RtlEnterCriticalSection(IN OUT PRTL_CRITICAL_SECTION CriticalSection);
BOOLEAN   VEAPI RtlTryEnterCriticalSection(IN OUT PRTL_CRITICAL_SECTION CriticalSection);
VEASTATUS VEAPI RtlLeaveCriticalSection(IN OUT PRTL_CRITICAL_SECTION CriticalSection);

VOID
VEAPI
RtlRaiseHardError(VEASTATUS Status);