#pragma once

#include "procbind.h"

VOID
VEAPI
KsBugCheckEx(
    ULONG BugCheckCode,
    ULONG_PTR BugCheckParameter1,
    ULONG_PTR BugCheckParameter2,
    ULONG_PTR BugCheckParameter3,
    ULONG_PTR BugCheckParameter4
);

VOID
VEAPI
KsBugCheck(
    ULONG BugCheckCode,
    ULONG_PTR BugCheckParameter1,
    ULONG_PTR BugCheckParameter2,
    ULONG_PTR BugCheckParameter3,
    ULONG_PTR BugCheckParameter4
);

typedef struct _KEVENT
{
    BOOLEAN Signaled;
} KEVENT, *PKEVENT;

typedef ULONG_PTR KSPIN_LOCK, *PKSPIN_LOCK;

typedef enum _KTHREAD_STATE {
    Initialized,
    Ready,
    Running,
    Standby,
    Terminated,
    Waiting
} KTHREAD_STATE;

typedef struct _KPROCESS {
    ULONG_PTR DirectoryTableBase;  // PENTING: Alamat fisik Page Directory (CR3) buat Context Switch
    LIST_ENTRY ThreadListHead;     // Daftar semua thread yang ada di dalam proses ini
    UCHAR State;                   // Status proses
    UCHAR BasePriority;            // Prioritas dasar untuk scheduling
    KSPIN_LOCK ProcessLock;        // Lock biar aman kalau diakses dari multicore
} KPROCESS, *PKPROCESS;

typedef struct _KTHREAD {
    PVOID KernelStack;             // PENTING: Pointer stack (RSP/ESP) saat thread ini di-pause
    PVOID InitialStack;            // Batas atas stack
    PVOID StackLimit;              // Batas bawah stack (buat deteksi Stack Overflow)
    
    KTHREAD_STATE State;           // Ready? Running? Waiting?
    LIST_ENTRY WaitListEntry;      // PENTING BUAT PNP: Kalau thread lagi nunggu disk/hardware, dia masuk antrean ini
    
    PKTRAP_FRAME TrapFrame;        // Nyimpen state register (EAX, EBX, EIP, dll) saat interrupt terjadi
    struct _KPROCESS *Process;     // Thread ini milik KPROCESS yang mana?
    
    CHAR Priority;                // Prioritas thread ini
    ULONG ContextSwitches;         // Udah berapa kali thread ini dieksekusi? (Buat statistik)
} KTHREAD, *PKTHREAD;