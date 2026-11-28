#pragma once

#ifndef WM_QUIT
#define WM_QUIT         0x0012
#endif

#ifndef CW_USEDEFAULT
#define CW_USEDEFAULT   ((INT)0x80000000)
#endif

typedef struct tagPOINT
{
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;

typedef VOID (CALLBACK *TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);