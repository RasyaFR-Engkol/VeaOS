#pragma once

typedef struct _W32THREAD
{
    struct _ETHREAD *pEThread;
    LONG     RefCount;
    PVOID    pgdiDcattr;
    PVOID    pgdiBrushAttr;
    DWORD    dwEngAcquireCount;
} W32THREAD, *PW32THREAD;

typedef struct _THREADINFO
{
    W32THREAD;                          /* Header wajib di offset 0 */
    // PPROCESSINFO        ppi;
    PUSER_MESSAGE_QUEUE MessageQueue;
    PDESKTOP_OBJECT     rpdesk;         /* Desktop yang terikat ke thread ini */
    HDESK               hdesk;          /* Handle HDESK aktif thread ini */
    ULONG               TIF_flags;
    INT                 iCursorLevel;
    POINT               ptLast;
    LIST_ENTRY          WindowListHead;
    UINT                cWindows;
} THREADINFO, *PTHREADINFO;