#pragma once

typedef struct _MESSAGE_QUEUE {
    HANDLE     hwnd;
    ULONG     message;
    ULONG_PTR wParam;   /* Virtual Key (VK_*) atau ASCII char */
    LONG_PTR  lParam;   /* Repeat count & ScanCode */
    ULONG     time;
} MESSAGE_QUEUE, *PMESSAGE_QUEUE;