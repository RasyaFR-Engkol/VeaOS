#pragma once

#pragma once

#ifndef WINAPI
    #define WINAPI __stdcall
#endif

typedef ULONG COLORREF, *LPCOLORREF;

#define RGB(r, g, b) \
    ((COLORREF)(((UCHAR)(r) | ((USHORT)((UCHAR)(g)) << 8)) | (((ULONG)(UCHAR)(b)) << 16)))

typedef const RECT *LPCRECT;

typedef struct tagPAINTSTRUCT {
    HDC     hdc;
    INT     fErase;
    RECT    rcPaint;
    INT     fRestore;
    INT     fIncUpdate;
    UCHAR   rgbReserved[32];
} PAINTSTRUCT, *PPAINTSTRUCT, *NPPAINTSTRUCT, *LPPAINTSTRUCT;
