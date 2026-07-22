#pragma once

#include "procbind.h"
#include "ks.h"
#include "veastatus.h"

typedef struct _ETHREAD {
    KTHREAD Tcb;                   // HARUS PALING ATAS!
    ULONG UniqueThreadId;          // TID (Thread ID)
    LIST_ENTRY ThreadListEntry;    // Nyambungin thread ini ke ThreadListHead milik KPROCESS
    
    PVOID StartAddress;            // Alamat fungsi awal saat thread ini dibikin
    VEASTATUS ExitStatus;           // Kalau thread mati, dia ninggalin kode error di sini
} ETHREAD, *PETHREAD;

typedef struct _EPROCESS {
    KPROCESS Pcb;                  // HARUS PALING ATAS! (Biar pointer EPROCESS bisa di-cast jadi KPROCESS)
    ULONG UniqueProcessId;         // PID (Process ID)
    LIST_ENTRY ActiveProcessLinks; // Rantai yang nyambungin semua proses di VeaOS
    CHAR ImageFileName[16];        // Nama file, misal: "veakrnl.exe" atau "explorer.exe"
    PVOID ObjectTable;             // Nanti dipakai buat nyimpen Handle (File, PnP Device, dll)
    struct _EPROCESS* ParentProcess; 
} EPROCESS, *PEPROCESS;