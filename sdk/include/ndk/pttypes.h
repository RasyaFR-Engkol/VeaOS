#pragma once

#include "mmtype.h"
#include "procbind.h"
#include "ks.h"
#include "veastatus.h"
#include "obtype.h"

typedef struct _CLIENT_ID {
    ULONG UniqueProcess; // PID
    ULONG UniqueThread;  // TID
} CLIENT_ID, *PCLIENT_ID;

typedef struct _ETHREAD {
    KTHREAD Tcb;                   // HARUS PALING ATAS!
    CLIENT_ID Cid;        // TID PID (Thread ID)
    LIST_ENTRY ThreadListEntry;    // Nyambungin thread ini ke ThreadListHead milik KPROCESS
    
    PVOID StartAddress;            // Alamat fungsi awal saat thread ini dibikin
    VEASTATUS ExitStatus;           // Kalau thread mati, dia ninggalin kode error di sini
} ETHREAD, *PETHREAD;

typedef struct _EPROCESS {
    KPROCESS Pcb;                  // HARUS PALING ATAS! (Biar pointer EPROCESS bisa di-cast jadi KPROCESS)
    CLIENT_ID Cid;        // PID (Process ID)
    LIST_ENTRY ActiveProcessLinks; // Rantai yang nyambungin semua proses di VeaOS
    CHAR ImageFileName[16];        // Nama file, misal: "veakrnl.exe" atau "explorer.exe"
    PVOID ObjectTable;             // Nanti dipakai buat nyimpen Handle (File, PnP Device, dll)
    struct _EPROCESS* ParentProcess; 
    WORKING_SET Vm;
    PVOID Peb;
} EPROCESS, *PEPROCESS;

typedef struct _KRUNQUEUE {
    RTL_RB_TREE CfsTree;
    PKTHREAD CurrentThread;
    ULONG TotalWeight;
    ULONG RunningTasks;
    ULONGLONG MinVRuntime;
    ULONG CpuId;
    KSPIN_LOCK SpinLock;
} KRUNQUEUE, *PKRUNQUEUE;

// defaulted size
#define PtThreadStackSize 0x2000

extern POBJECT_TYPE PtEthreadType;
extern POBJECT_TYPE PtEprocessType;
extern LIST_ENTRY PtProcessList;
extern PEPROCESS PtSystemProcess;
extern KTHREAD PtSystemIdleThread[32];
extern KRUNQUEUE PtRunQueue[MAX_CPU];