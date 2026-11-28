#pragma once

typedef struct tagPOINT
{
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;

typedef struct _MESSAGE_QUEUE {
    HANDLE     hwnd;
    ULONG     message;
    ULONG_PTR wParam;   /* Virtual Key (VK_*) atau ASCII char */
    LONG_PTR  lParam;   /* Repeat count & ScanCode */
    ULONG     time;
    POINT      pt;
} MESSAGE_QUEUE, *PMESSAGE_QUEUE;