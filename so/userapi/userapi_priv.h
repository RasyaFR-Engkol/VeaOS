#pragma once

#include <ndk/pttypes.h>
#include <procbind.h>
#include <veastatus.h>
#include <psdk/msg.h>
#include <psdk/virtkey.h>
#include <psdk/winuser.h>
#include <psdk/resource.h>
#include <userapi.h>
#include <rtl.h>

/* Deklarasi WndProc kontrol bawaan (diimplementasikan di controls.c) */
LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ButtonWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

/* Dipanggil saat startup userapi.so sebelum WinMain berjalan */
VOID UserapiRegisterSystemControls(VOID);
VOID
VEAPI
UserapiInitDefaultCursor(VOID);

#define USER_CURSOR_MAGIC   0x52554356
#define MAX_USER_CURSORS    64
#define MAX_CURSOR_DIM      64

typedef struct _USER_CURSOR_OBJECT {
    ULONG   Magic;
    BOOLEAN Used;
    BOOLEAN SystemCursor;
    ULONG   Width;
    ULONG   Height;
    LONG    HotX;
    LONG    HotY;
    ULONG   Pixels[MAX_CURSOR_DIM * MAX_CURSOR_DIM];
} USER_CURSOR_OBJECT, *PUSER_CURSOR_OBJECT;

#define CTRL_ALLOC(_Size) RtlAllocateHeap(NtCurrentPeb()->ProcessHeap, 0, (_Size))
#define CTRL_FREE(_Ptr)   RtlFreeHeap(NtCurrentPeb()->ProcessHeap, 0, (_Ptr))

#define MAX_CTRL_STATES 64
#define MAX_CTRL_TEXT   128

typedef struct _CTRL_STATE {
    LIST_ENTRY ListEntry;
    HWND       hWnd;
    HWND       hParent;
    USHORT     CtrlId;
    ULONG      Style;
    BOOLEAN    IsPressed;
    BOOLEAN    IsFocused;
    ULONG      TextLen;
    ULONG      TextCapacity;
    PSTR       Text;
} CTRL_STATE, *PCTRL_STATE;

CONST RESOURCE_DATA_ENTRY *
VEAPI
UserFindResourceEx(
    IN HMODULE     hModule,
    IN CONST CHAR *lpType,
    IN CONST CHAR *lpName,
    IN USHORT      wLanguage
);

CONST VOID *
VEAPI
UserLockResource(
    IN HMODULE                    hModule,
    IN CONST RESOURCE_DATA_ENTRY *pResData
);

ULONG
VEAPI
UserSizeofResource(
    IN HMODULE                    hModule,
    IN CONST RESOURCE_DATA_ENTRY *pResData
);

extern CONST UCHAR VeaResourceStart[];
extern HMODULE gUserApiModule;

PVOID
VEAPI
LdrGetProcedureAddress(
    IN PVOID BaseAddress,
    IN PCSTR ProcedureName
);

#define IDB_DEFAULT_CURSOR  101
#define IDB_16X16_CURSOR    102
#define IDB_32X32_CURSOR    103

/* EOF */