#pragma once

#include "procbind.h"

typedef struct _HANDLE_TABLE_ENTRY {
    PVOID Object;          // Pointer ke Header/Body Objek (NULL jika entry ini kosong)
    ULONG GrantedAccess;   // Hak akses untuk handle ini
    ULONG NextFreeIndex;   // Index ke entry kosong berikutnya (hanya valid jika Object == NULL)
} HANDLE_TABLE_ENTRY, *PHANDLE_TABLE_ENTRY;

typedef struct _HANDLE_TABLE {
    PHANDLE_TABLE_ENTRY Entries; // Flat array dari entries
    ULONG MaxEntries;            // Kapasitas maksimal array saat ini
    ULONG HandleCount;           // Jumlah handle yang sedang aktif
    ULONG FirstFreeIndex;        // Head dari linked list entry kosong
    // KSPIN_LOCK Lock;          // Nanti diaktifkan kalau sistem thread udah siap
} HANDLE_TABLE, *PHANDLE_TABLE;

#define HANDLE_TO_INDEX(h) ((ULONG)(h) >> 2)
#define INDEX_TO_HANDLE(i) ((HANDLE)((i) << 2))

typedef PVOID HANDLE, *PHANDLE;