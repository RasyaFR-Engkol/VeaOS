#include <veakrnl.h>

PHANDLE_TABLE
VEAPI
UlCreateHandleTable(
    ULONG InitialCapacity
)
{
    PHANDLE_TABLE Table = UlAllocatePoolWithTag(PagedPool, sizeof(HANDLE_TABLE), 'TbHn');
    if (!Table) return NULL;

    Table->Entries = UlAllocatePoolWithTag(PagedPool, sizeof(HANDLE_TABLE_ENTRY) * InitialCapacity, 'EnHn');
    if (!Table->Entries) {
        UlFreePoolWithTag(Table, 'TbHn');
        return NULL;
    }

    Table->MaxEntries = InitialCapacity;
    Table->HandleCount = 0;
    Table->Entries[0].Object = NULL;
    Table->Entries[0].NextFreeIndex = 0;

    Table->FirstFreeIndex = 1;
    for (ULONG i = 1; i < InitialCapacity; i++) {
        Table->Entries[i].Object = NULL;
        Table->Entries[i].GrantedAccess = 0;
        // Entry terakhir menunjuk ke 0 (menandakan list habis)
        Table->Entries[i].NextFreeIndex = (i == InitialCapacity - 1) ? 0 : (i + 1);
    }

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
    if (!Table || !Object) return NULL;

    // TODO: Nanti pasang AcquireSpinLock(&Table->Lock) di sini

    // Cek apakah tabel penuh
    if (Table->FirstFreeIndex == 0) {
        // TODO: Fase 2 - Implementasi Resize/Grow array di sini
        // Untuk sekarang (Fase 1), kita tolak kalau penuh
        // ReleaseSpinLock(&Table->Lock);
        return NULL;
    }

    // Ambil entry kosong dari kepala Free List
    ULONG NewIndex = Table->FirstFreeIndex;
    PHANDLE_TABLE_ENTRY Entry = &Table->Entries[NewIndex];

    // Geser kepala Free List ke entry kosong berikutnya
    Table->FirstFreeIndex = Entry->NextFreeIndex;

    // Isi data objek ke entry tersebut
    Entry->Object = Object;
    Entry->GrantedAccess = GrantedAccess;
    Entry->NextFreeIndex = 0; // Bersihkan sisa data

    Table->HandleCount++;

    // TODO: Nanti pasang ReleaseSpinLock(&Table->Lock) di sini

    // Kembalikan Handle (Index digeser 2 bit ke kiri ala NT)
    return INDEX_TO_HANDLE(NewIndex);
}

VEASTATUS
VEAPI
UlDestroyHandle(
    PHANDLE_TABLE Table,
    HANDLE Handle
)
{
    if (!Table || !Handle) {
        return STATUS_INVALID_PARAMETER;
    }

    // Ekstrak Index dari Handle
    ULONG Index = HANDLE_TO_INDEX(Handle);

    // TODO: AcquireSpinLock(&Table->Lock)

    // Validasi Batas Index
    if (Index == 0 || Index >= Table->MaxEntries) {
        // ReleaseSpinLock(&Table->Lock);
        return STATUS_INVALID_HANDLE;
    }

    PHANDLE_TABLE_ENTRY Entry = &Table->Entries[Index];

    // Pastikan slot ini memang sedang dipakai (Object tidak NULL)
    if (Entry->Object == NULL) {
        // ReleaseSpinLock(&Table->Lock);
        return STATUS_INVALID_HANDLE; // Handle sudah pernah ditutup (Double Close!)
    }

    // Amankan pointer objek sebelum entry dihapus
    PVOID ObjectToDereference = Entry->Object;

    // Bersihkan Entry dan kembalikan ke Free List
    Entry->Object = NULL;
    Entry->GrantedAccess = 0;
    
    // Sambungkan NextFreeIndex ke kepala Free List yang lama
    Entry->NextFreeIndex = Table->FirstFreeIndex;
    // Jadikan entry ini sebagai kepala Free List yang baru
    Table->FirstFreeIndex = Index;

    Table->HandleCount--;

    // TODO: ReleaseSpinLock(&Table->Lock)

    // Dereferensi Objek (Sangat penting agar memori objek bisa bersih jika ini handle terakhir)
    ObDereferenceObject(ObjectToDereference);

    return STATUS_SUCCESS;
}