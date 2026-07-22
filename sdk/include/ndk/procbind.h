#pragma once

#include <stddef.h>

// we are in 32 bit os
#define _X86_

/* 32 bit type data */
typedef char CHAR, *PCHAR;
typedef short SHORT, *PSHORT;
typedef long LONG, *PLONG;
typedef int INT, *PINT;

/* unsigned */
typedef unsigned char UCHAR, *PUCHAR, BYTE, *PBYTE;
typedef unsigned short USHORT, *PUSHORT, WORD, *PWORD;
typedef unsigned long ULONG, *PULONG, DWORD, *PDWORD;
typedef unsigned int UINT, *PUINT;

typedef void VOID, *PVOID;

typedef unsigned char BOOLEAN, *PBOOLEAN;
#define TRUE  1
#define FALSE 0

typedef unsigned long ULONG_PTR, *PULONG_PTR;
typedef long LONG_PTR, *PLONG_PTR;
typedef unsigned long SIZE_T, *PSIZE_T;

typedef long long LONGLONG, *PLONGLONG;
typedef unsigned long long ULONGLONG, *PULONGLONG, QWORD, *PQWORD;

/* character */
typedef unsigned short WCHAR, *PWCHAR;

typedef struct _STRING {
    USHORT Length;          // Panjang string saat ini dalam hitungan byte (TIDAK termasuk null-terminator)
    USHORT MaximumLength;   // Total kapasitas memori Buffer dalam byte
    PCHAR  Buffer;          // Pointer ke array karakter (1 byte per char)
} STRING, *PSTRING, ANSI_STRING, *PANSI_STRING, OEM_STRING, *POEM_STRING;

typedef struct _UNICODE_STRING {
    USHORT Length;          // Panjang string saat ini dalam hitungan byte (BUKAN jumlah karakter!)
    USHORT MaximumLength;   // Total kapasitas memori Buffer dalam byte
    PWCHAR Buffer;          // Pointer ke array wide character (2 byte per char)
} UNICODE_STRING, *PUNICODE_STRING;

/* linked list */

typedef struct _LIST_ENTRY {
    struct _LIST_ENTRY *Flink; // Forward link (Maju)
    struct _LIST_ENTRY *Blink; // Backward link (Mundur)
} LIST_ENTRY, *PLIST_ENTRY;

typedef struct _SINGLE_LIST_ENTRY {
    struct _SINGLE_LIST_ENTRY *Next;
} SINGLE_LIST_ENTRY, *PSINGLE_LIST_ENTRY;

/* calling convention */
#define VEAPI      __stdcall    // Untuk public API (System calls, Driver interface)
#define VEAFAST    __fastcall   // Untuk internal OS (Paling cepat, via ECX/EDX)
#define VEACDECL   __cdecl      // Hanya untuk kprintf dll

/* large integer and uinteger */
typedef union _LARGE_INTEGER {
  struct {
    DWORD LowPart;
    LONG  HighPart;
  };
  LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef union _ULARGE_INTEGER {
  struct {
    DWORD LowPart;
    DWORD HighPart;
  };
  ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

#ifdef _X86_

typedef struct _KTRAP_FRAME
{
    ULONG DbgEbp;
    ULONG DbgEip;
    ULONG DbgArgMark;
    ULONG DbgArgPointer;
    ULONG TempSegCs;
    ULONG TempEsp;
    ULONG Dr0;
    ULONG Dr1;
    ULONG Dr2;
    ULONG Dr3;
    ULONG Dr6;
    ULONG Dr7;
    ULONG SegGs;
    ULONG SegEs;
    ULONG SegDs;
    ULONG Edx;
    ULONG Ecx;
    ULONG Eax;
    ULONG PreviousPreviousMode;
    struct _EXCEPTION_REGISTRATION_RECORD *ExceptionList;
    ULONG SegFs;
    ULONG Edi;
    ULONG Esi;
    ULONG Ebx;
    ULONG Ebp;
    ULONG ErrCode;
    ULONG Eip;
    ULONG SegCs;
    ULONG EFlags;
    ULONG HardwareEsp;
    ULONG HardwareSegSs;
    ULONG V86Es;
    ULONG V86Ds;
    ULONG V86Fs;
    ULONG V86Gs;
} KTRAP_FRAME, *PKTRAP_FRAME;

#elif defined (_X64_)



#endif