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
    IN PHANDLE_TABLE Table,
    IN PHANDLE_TABLE_ENTRY HandleTableEntry
)
{
    KIRQL OldIrql;
    PVOID Owner;
    ULONG CountNow;

    if (!Table || !HandleTableEntry) return NULL;

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

    // Salin seluruh isi HandleTableEntry yang dikirim caller (Object & GrantedAccess)
    *Entry = *HandleTableEntry;

    // Set metadata owner dan reset link free list entry aktif
    Entry->OwnerThread = PtGetCurrentThread();
    Owner = Entry->OwnerThread;
    Entry->NextFreeIndex = 0;

    Table->HandleCount++;
    CountNow = Table->HandleCount;

    KsReleaseSpinLock(&Table->Lock, OldIrql);

    /* Buat public handle value dari index (INDEX_TO_HANDLE) */
    HANDLE NewHandle = INDEX_TO_HANDLE(NewIndex);

    /* Laporkan pembaruan siklus hidup objek ke subsystem */
    ObpReportLifecycle(NewHandle, CountNow, TRUE, TRUE, Owner, Entry->Object);

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

PHANDLE_TABLE
VEAPI
UlDupHandleTable(
    IN PEPROCESS TargetProcess,
    IN PHANDLE_TABLE SourceTable
)
{
    PHANDLE_TABLE TargetTable;
    KIRQL OldIrql;
    ULONG MaxEntries;
    ULONG LastFreeIndex = 0;

    UNREFERENCED_PARAMETER(TargetProcess);

    if (!SourceTable) return NULL;

    // Ambil kapasitas maksimal tabel sumber
    KsAcquireSpinLock(&SourceTable->Lock, &OldIrql);
    MaxEntries = SourceTable->MaxEntries;
    KsReleaseSpinLock(&SourceTable->Lock, OldIrql);

    if (MaxEntries < 2) return NULL;

    // 1. Alokasikan struktur HANDLE_TABLE baru
    TargetTable = UlAllocatePoolZero(NonPagedPool, sizeof(HANDLE_TABLE), 'TbHn');
    if (!TargetTable) return NULL;

    TargetTable->Entries = UlAllocatePoolZero(
        NonPagedPool,
        sizeof(HANDLE_TABLE_ENTRY) * MaxEntries,
        'EnHn'
    );
    if (!TargetTable->Entries)
    {
        UlFreePoolWithTag(TargetTable, 'TbHn');
        return NULL;
    }

    TargetTable->MaxEntries = MaxEntries;
    TargetTable->HandleCount = 0;
    TargetTable->FirstFreeIndex = 0;
    KsInitializeSpinlock(&TargetTable->Lock);

    // Lock tabel sumber selama proses pengkopian entry
    KsAcquireSpinLock(&SourceTable->Lock, &OldIrql);

    // 2. Iterasi seluruh entry (Index 1 sampai MaxEntries - 1)
    for (ULONG i = 1; i < MaxEntries; i++)
    {
        PHANDLE_TABLE_ENTRY SrcEntry = &SourceTable->Entries[i];
        PHANDLE_TABLE_ENTRY DstEntry = &TargetTable->Entries[i];

        // Cek apakah slot di Parent terisi DAN memiliki flag OBJ_INHERIT
        if (SrcEntry->Object != NULL && (SrcEntry->GrantedAccess & OBJ_INHERIT))
        {
            // Salin detail entry dan pertahankan Index yang sama
            DstEntry->Object = SrcEntry->Object;
            DstEntry->GrantedAccess = SrcEntry->GrantedAccess;
            DstEntry->OwnerThread = PtGetCurrentThread();
            DstEntry->NextFreeIndex = 0;

            // Naikkan Reference Count objek agar tidak ter-free jika Parent menutup handle
            ObReferenceObject(SrcEntry->Object);

            TargetTable->HandleCount++;

            /* Laporkan pembaruan lifecycle handle untuk child process */
            HANDLE HandleVal = INDEX_TO_HANDLE(i);
            ObpReportLifecycle(
                HandleVal,
                TargetTable->HandleCount,
                TRUE,
                TRUE,
                DstEntry->OwnerThread,
                DstEntry->Object
            );
        }
        else
        {
            // Slot ini kosong atau TIDAK diwariskan -> Sambungkan ke Free List TargetTable
            DstEntry->Object = NULL;
            DstEntry->OwnerThread = NULL;
            DstEntry->GrantedAccess = 0;
            DstEntry->NextFreeIndex = 0;

            if (TargetTable->FirstFreeIndex == 0)
            {
                TargetTable->FirstFreeIndex = i;
            }
            else
            {
                TargetTable->Entries[LastFreeIndex].NextFreeIndex = i;
            }
            LastFreeIndex = i;
        }
    }

    KsReleaseSpinLock(&SourceTable->Lock, OldIrql);

    return TargetTable;
}

/* EOF */