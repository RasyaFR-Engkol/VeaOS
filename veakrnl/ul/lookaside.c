/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : lookaside.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : lookaside list
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 17-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file lookaside.c
 * --------------------------------------------------------------------- */

/* Keep the name Ex */
#define NUMBER_POOL_LOOKASIDE_LISTS 32
LIST_ENTRY ExpNonPagedLookasideListHead;
KSPIN_LOCK ExpNonPagedLookasideListLock;
LIST_ENTRY ExpPagedLookasideListHead;
KSPIN_LOCK ExpPagedLookasideListLock;
LIST_ENTRY ExSystemLookasideListHead;
LIST_ENTRY ExPoolLookasideListHead;
GENERAL_LOOKASIDE ExpSmallNPagedPoolLookasideLists[NUMBER_POOL_LOOKASIDE_LISTS];
GENERAL_LOOKASIDE ExpSmallPagedPoolLookasideLists[NUMBER_POOL_LOOKASIDE_LISTS];

VOID
VEAPI
UlInitializeSystemLookasideList(IN PGENERAL_LOOKASIDE List,
                                IN POOL_TYPE Type,
                                IN ULONG Size,
                                IN ULONG Tag,
                                IN USHORT MaximumDepth,
                                IN PLIST_ENTRY ListHead)
{
    /* Initialize the list */
    List->Tag = Tag;
    List->Type = Type;
    List->Size = Size;
    InsertHeadList(ListHead, &List->ListEntry);
    List->MaximumDepth = MaximumDepth;
    List->Depth = 2;
    List->Allocate = UlAllocatePoolWithTag;
    List->Free = UlFreePool;
    InitializeSListHead(&List->ListHead);
    List->TotalAllocates = 0;
    List->AllocateHits = 0;
    List->TotalFrees = 0;
    List->FreeHits = 0;
    List->LastTotalAllocates = 0;
    List->LastAllocateHits = 0;
}

VOID
VEAPI
UlInitPoolLookasidePointers(VOID)
{
    ULONG i;
    PKPRCB Prcb = KeGetCurrentPrcb();
    PGENERAL_LOOKASIDE Entry;

    for (i = 0; i < NUMBER_POOL_LOOKASIDE_LISTS; i++)
    {
        Entry = &ExpSmallNPagedPoolLookasideLists[i];
        InitializeSListHead(&Entry->ListHead);

        Prcb->PPNPagedLookasideList[i].P = Entry;
        Prcb->PPNPagedLookasideList[i].L = Entry;

        Entry = &ExpSmallPagedPoolLookasideLists[i];
        InitializeSListHead(&Entry->ListHead);

        Prcb->PPPagedLookasideList[i].P = Entry;
        Prcb->PPPagedLookasideList[i].L = Entry;
    }
}

VOID
VEAPI
UlpInitLookasideList(VOID)
{
    ULONG i;

    InitializeListHead(&ExpNonPagedLookasideListHead);
    InitializeListHead(&ExpPagedLookasideListHead);
    InitializeListHead(&ExSystemLookasideListHead);
    InitializeListHead(&ExPoolLookasideListHead);
    KsInitializeSpinlock(&ExpNonPagedLookasideListLock);
    KsInitializeSpinlock(&ExpPagedLookasideListLock);

    for (i = 0; i < NUMBER_POOL_LOOKASIDE_LISTS; i++)
    {
        UlInitializeSystemLookasideList(
            &ExpSmallNPagedPoolLookasideLists[i],
            NonPagedPool,
            (i + 1) * 8,
            'looP',
            256,
            &ExPoolLookasideListHead
        );

        UlInitializeSystemLookasideList(
            &ExpSmallPagedPoolLookasideLists[i],
            PagedPool,
            (i + 1) * 8,
            'looP',
            256,
            &ExPoolLookasideListHead
        );
    }
}

VOID
VEAPI
UlInitializeNPagedLookasideList(
    IN PNPAGED_LOOKASIDE_LIST Lookaside,
    IN PALLOCATE_FUNCTION Allocate OPTIONAL,
    IN PFREE_FUNCTION Free OPTIONAL,
    IN ULONG Flags,
    IN SIZE_T Size,
    IN ULONG Tag,
    IN USHORT Depth
)
{
    UlInitializeSListHead(&Lookaside->L.ListHead);
    Lookaside->L.TotalAllocates = 0;
    Lookaside->L.AllocateMisses = 0;
    Lookaside->L.TotalFrees = 0;
    Lookaside->L.FreeMisses = 0;
    Lookaside->L.Type = NonPagedPool | Flags;
    Lookaside->L.Tag = Tag;
    Lookaside->L.Size = (ULONG)Size;
    Lookaside->L.Depth = 4;
    Lookaside->L.MaximumDepth = 256;
    Lookaside->L.LastTotalAllocates = 0;
    Lookaside->L.LastAllocateMisses = 0;

    if (Allocate)
    {
        Lookaside->L.Allocate = Allocate;
    }
    else
    {
        Lookaside->L.Allocate = UlAllocatePoolWithTag;
    }

    if (Free)
    {
        Lookaside->L.Free = Free;
    }
    else
    {
        Lookaside->L.Free = UlFreePool;
    }

    UlInterlockedInsertTailList(&ExpNonPagedLookasideListHead, &Lookaside->L.ListEntry, &ExpNonPagedLookasideListLock);
}

VOID
VEAPI
UlInitializePagedLookasideList(
    IN PPAGED_LOOKASIDE_LIST Lookaside,
    IN PALLOCATE_FUNCTION Allocate OPTIONAL,
    IN PFREE_FUNCTION Free OPTIONAL,
    IN ULONG Flags,
    IN SIZE_T Size,
    IN ULONG Tag,
    IN ULONG Depth
)
{
    UlInitializeSListHead(&Lookaside->L.ListHead);
    Lookaside->L.TotalAllocates = 0;
    Lookaside->L.AllocateMisses = 0;
    Lookaside->L.TotalFrees = 0;
    Lookaside->L.FreeMisses = 0;
    Lookaside->L.Type = PagedPool | Flags;
    Lookaside->L.Tag = Tag;
    Lookaside->L.Size = (ULONG)Size;
    Lookaside->L.Depth = 4;
    Lookaside->L.MaximumDepth = 256;
    Lookaside->L.LastTotalAllocates = 0;
    Lookaside->L.LastAllocateMisses = 0;

    if (Allocate)
    {
        Lookaside->L.Allocate = Allocate;
    }
    else
    {
        Lookaside->L.Allocate = UlAllocatePoolWithTag;
    }

    if (Free)
    {
        Lookaside->L.Free = Free;
    }
    else
    {
        Lookaside->L.Free = UlFreePool;
    }

    UlInterlockedInsertTailList(&ExpPagedLookasideListHead,
                                &Lookaside->L.ListEntry,
                                &ExpPagedLookasideListLock);
}