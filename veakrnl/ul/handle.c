#include <veakrnl.h>

PHANDLE_TABLE
VEAPI
UlCreateHandleTable(
    ULONG InitialCapacity
)
{
    PHANDLE_TABLE Table;
    
    if (InitialCapacity < 2) return NULL;

    Table = UlAllocatePoolZero(NonPagedPool, sizeof(HANDLE_TABLE), 'TbHn');
    if (!Table) return NULL;

    Table->Entries = UlAllocatePoolZero(NonPagedPool,
                        sizeof(HANDLE_TABLE_ENTRY) * InitialCapacity, 'EnHn');
    if (!Table->Entries)
    {
        UlFreePoolWithTag(Table, 'TbHn');
        return NULL;
    }

    Table->MaxEntries     = InitialCapacity;
    Table->HandleCount    = 0;
    Table->FirstFreeIndex = 1;

    for (ULONG i = 1; i < InitialCapacity; i++)
        Table->Entries[i].NextFreeIndex = (i == InitialCapacity - 1) ? 0 : (i + 1);

    KsInitializeSpinlock(&Table->Lock);
    return Table;
}

HANDLE
VEAPI
UlCreateHandle(
    PHANDLE_TABLE Table,
    PVOID Object,
    ULONG GrantedAccess
)
{
    KIRQL OldIrql;
    PVOID  Owner;
    ULONG  CountNow;

    if (!Table || !Object) return NULL;

    KsAcquireSpinLock(&Table->Lock, &OldIrql);

    // Cek apakah tabel penuh
    if (Table->FirstFreeIndex == 0) {
        KsReleaseSpinLock(&Table->Lock, OldIrql);
        return NULL;
    }

    // Ambil entry kosong dari kepala Free List
    ULONG NewIndex = Table->FirstFreeIndex;
    PHANDLE_TABLE_ENTRY Entry = &Table->Entries[NewIndex];

    // Geser kepala Free List ke entry kosong berikutnya
    Table->FirstFreeIndex = Entry->NextFreeIndex;

    // Isi data objek ke entry tersebut
    Entry->Object = Object;
    Entry->OwnerThread = PtGetCurrentThread();
    Owner    = Entry->OwnerThread;
    Entry->GrantedAccess = GrantedAccess;
    Entry->NextFreeIndex = 0; 

    Table->HandleCount++;
    CountNow = Table->HandleCount;

    KsReleaseSpinLock(&Table->Lock, OldIrql);

    /* Build the public handle value after the table entry is committed. */
    HANDLE NewHandle = INDEX_TO_HANDLE(NewIndex);

    /* Report that a new handle now owns a reference to the object. */
    ObpReportLifecycle(NewHandle, CountNow, TRUE, TRUE, Owner, Object);

    /* Return the handle with its index shifted two bits like NT handles. */
    return NewHandle;
}

VEASTATUS
VEAPI
UlDestroyHandle(
    PHANDLE_TABLE Table,
    HANDLE Handle
)
{
    KIRQL OldIrql;

    if (!Handle) {
        return STATUS_INVALID_PARAMETER;
    }

    // Ekstrak Index dari Handle
    ULONG Index = HANDLE_TO_INDEX(Handle);

    KsAcquireSpinLock(&Table->Lock, &OldIrql);

    // Validasi Batas Index
    if (Index == 0 || Index >= Table->MaxEntries) {
        KsReleaseSpinLock(&Table->Lock, OldIrql);
        return STATUS_INVALID_HANDLE;
    }

    PHANDLE_TABLE_ENTRY Entry = &Table->Entries[Index];

    // Pastikan slot ini memang sedang dipakai (Object tidak NULL)
    if (Entry->Object == NULL) {
        KsReleaseSpinLock(&Table->Lock, OldIrql);
        return STATUS_INVALID_HANDLE;
    }

    // Amankan pointer objek sebelum entry dihapus
    PVOID ObjectToDereference = Entry->Object;

    // Bersihkan Entry dan kembalikan ke Free List
    Entry->Object = NULL;
    PVOID OwnerThread = Entry->OwnerThread;
    Entry->OwnerThread = NULL;
    Entry->GrantedAccess = 0;
    
    // Sambungkan NextFreeIndex ke kepala Free List yang lama
    Entry->NextFreeIndex = Table->FirstFreeIndex;
    Table->FirstFreeIndex = Index;

    Table->HandleCount--;

    KsReleaseSpinLock(&Table->Lock, OldIrql);

    /* Report that this handle was removed from the table. */
    ObpReportLifecycle(
        Handle,
        Table->HandleCount,
        FALSE,
        TRUE,
        OwnerThread,
        ObjectToDereference
    );

    ObpDeleteNameCheck(ObjectToDereference);

    /* Dereference the object after removing and reporting the handle. */
    ObDereferenceObject(ObjectToDereference);

    return STATUS_SUCCESS;
}
