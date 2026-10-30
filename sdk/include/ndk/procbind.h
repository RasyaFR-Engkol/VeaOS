#pragma once

#include <stddef.h>

/* =====================================================================
 * SAL (Source Code Annotation Language) Support
 * =====================================================================
 * Menangani anotasi SAL bawaan MS (_In_, _Out_, dsb.) untuk Clang/MSVC.
 * Note: IN, OUT, dan OPTIONAL tidak diotak-atik di sini.
 */

#if defined(_MSC_VER) || defined(__clang__)
    /* Cek apakah file header sal.h dari PSDK tersedia */
    #if defined(__has_include)
        #if __has_include(<sal.h>)
            #include <sal.h>
        #endif
    #endif
#endif

#if defined (__clang__)
    #define GCC_COMPILER 1
    #define COMPILER_MODE "GCC"
#elif defined (__GNUC__) || defined(__GNUG__)
    #define GCC_COMPILER 1
    #define COMPILER_MODE "GCC"
#elif defined (_MSC_VER)
    #define MSCVER_COMPILER 1
    #define COMPILER_MODE "MSVC"
#else
    #error "procbind.h: ERROR: Unknown compiler. Please use verified compiler (GCC, Clang, MSVC)
#endif

/* ---------------------------------------------------------------------
 * Fallback Macro
 * Jika sal.h tidak ditemukan atau anotasi belum didefinisikan oleh SDK,
 * di-define kosong agar kompiler (Clang/GCC) tidak melempar Syntax Error.
 * --------------------------------------------------------------------- */

#ifndef _In_
    #define _In_
#endif

#ifndef _Out_
    #define _Out_
#endif

#ifndef _Inout_
    #define _Inout_
#endif

#ifndef _In_opt_
    #define _In_opt_
#endif

#ifndef _Out_opt_
    #define _Out_opt_
#endif

#ifndef _Inout_opt_
    #define _Inout_opt_
#endif

#ifndef _Outptr_
    #define _Outptr_
#endif

#ifndef _Check_return_
    #define _Check_return_
#endif

#ifndef _Success_
    #define _Success_(expr)
#endif

// we are in 32 bit os
#define _X86_

/* 32 bit type data */
typedef char CHAR, *PCHAR, *PSTR;
typedef const CHAR *CSTR, *PCSTR;
typedef short SHORT, *PSHORT;
typedef SHORT const CSHORT;
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

typedef unsigned __int64 ULONG64, *PULONG64;
typedef unsigned __int32 ULONG32, *PULONG32;
typedef __int32 LONG32, *PLONG32;
typedef __int64 LONG64, *PLONG64;

#define IN
#define OUT
#define OPTIONAL

/* character */
typedef unsigned short WCHAR, *PWCHAR, *PWSTR, WSTR;
typedef const WCHAR *PCWTSR;

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

#undef MAX_CPU
#define MAX_CPU 32

#elif defined (_X64_)



#endif

#if defined(_MSC_VER)
    // Kompiler MSVC (Microsoft Visual Studio)
    #define FORCEINLINE __forceinline

#elif defined(__GNUC__) || defined(__clang__)
    // Kompiler GCC / Clang (GCC butuh kata 'inline' + attribute)
    #define FORCEINLINE inline __attribute__((always_inline))

#else
    // Fallback untuk kompiler C standar
    #define FORCEINLINE inline
#endif

typedef UCHAR KIRQL, *PKIRQL;

#define UNREFERENCED_PARAMETER(x) (void)(x)

typedef struct _GUID {
    ULONG  Data1;
    USHORT Data2;
    USHORT Data3;
    UCHAR  Data4[8];
} GUID, *PGUID;

#if defined(__GNUC__) || defined(__clang__)
    #if defined(BUILDING_VEAKRNL)
        // Pas ngompal Kernel: Biar simbol tetep kelihatan di ELF symbol table
        // meskipun pke flag compiler -fvisibility=hidden
        #define VEA_EXPORT __attribute__((visibility("default")))
    #else
        // Pas ngompal Driver: Murni penanda fungsi luar (extern)
        #define VEA_EXPORT extern
    #endif
#elif defined(_MSC_VER)
    // Kalau suatu saat pake MSVC / PE Format
    #if defined(BUILDING_VEAKRNL)
        #define VEA_EXPORT __declspec(dllexport)
    #else
        #define VEA_EXPORT __declspec(dllimport)
    #endif
    #define VEAPI __cdecl
#else
    #define VEA_EXPORT extern
    #define VEAPI
#endif

// special
typedef enum _KPROCESSOR_MODE {
    KernelMode,
    UserMode
} KPROCESSOR_MODE;

/* i386 doesn't have exception frame*/
typedef struct _KEXCEPTION_FRAME *PKEXCEPTION_FRAME;

#define NORET __attribute((noreturn))

#define FIELD_OFFSET(Type, Field) ((LONG)__builtin_offsetof(Type, Field))

#if defined(__clang__)
  #define CAPABILITY(x)            __attribute__((capability(x)))
  #define ACQUIRE(...)             __attribute__((acquire_capability(__VA_ARGS__)))
  #define RELEASE(...)             __attribute__((release_capability(__VA_ARGS__)))
  #define REQUIRES(...)            __attribute__((requires_capability(__VA_ARGS__)))
#else
  #define CAPABILITY(x)
  #define ACQUIRE(...)
  #define RELEASE(...)
  #define REQUIRES(...)
#endif

#if defined(__clang__)
  #define NO_THREAD_SAFETY_ANALYSIS __attribute__((no_thread_safety_analysis))
#else
  #define NO_THREAD_SAFETY_ANALYSIS
#endif

#ifndef FlagOn
#define FlagOn(_F,_SF)        ((_F) & (_SF))
#endif

#ifndef BooleanFlagOn
#define BooleanFlagOn(F,SF)   ((BOOLEAN)(((F) & (SF)) != 0))
#endif

#ifndef SetFlag
#define SetFlag(_F,_SF)       ((_F) |= (_SF))
#endif

#ifndef ClearFlag
#define ClearFlag(_F,_SF)     ((_F) &= ~(_SF))
#endif

#ifndef DUMMYUNIONNAME
 #if defined(NONAMELESSUNION)// || !defined(_MSC_EXTENSIONS)
  #define _ANONYMOUS_UNION
  #define _UNION_NAME(x) x
  #define DUMMYUNIONNAME  u
  #define DUMMYUNIONNAME1 u1
  #define DUMMYUNIONNAME2 u2
  #define DUMMYUNIONNAME3 u3
  #define DUMMYUNIONNAME4 u4
  #define DUMMYUNIONNAME5 u5
  #define DUMMYUNIONNAME6 u6
  #define DUMMYUNIONNAME7 u7
  #define DUMMYUNIONNAME8 u8
  #define DUMMYUNIONNAME9  u9
 #else
  #define _ANONYMOUS_UNION
  #define _UNION_NAME(x)
  #define DUMMYUNIONNAME
  #define DUMMYUNIONNAME1
  #define DUMMYUNIONNAME2
  #define DUMMYUNIONNAME3
  #define DUMMYUNIONNAME4
  #define DUMMYUNIONNAME5
  #define DUMMYUNIONNAME6
  #define DUMMYUNIONNAME7
  #define DUMMYUNIONNAME8
  #define DUMMYUNIONNAME9
 #endif /* NONAMELESSUNION */
#endif /* !DUMMYUNIONNAME */

#ifndef DUMMYSTRUCTNAME
 #if defined(NONAMELESSUNION)// || !defined(_MSC_EXTENSIONS)
  #define _ANONYMOUS_STRUCT
  #define _STRUCT_NAME(x) x
  #define DUMMYSTRUCTNAME s
  #define DUMMYSTRUCTNAME1 s1
  #define DUMMYSTRUCTNAME2 s2
  #define DUMMYSTRUCTNAME3 s3
  #define DUMMYSTRUCTNAME4 s4
  #define DUMMYSTRUCTNAME5 s5
 #else
  #define _ANONYMOUS_STRUCT
  #define _STRUCT_NAME(x)
  #define DUMMYSTRUCTNAME
  #define DUMMYSTRUCTNAME1
  #define DUMMYSTRUCTNAME2
  #define DUMMYSTRUCTNAME3
  #define DUMMYSTRUCTNAME4
  #define DUMMYSTRUCTNAME5
 #endif /* NONAMELESSUNION */
#endif /* DUMMYSTRUCTNAME */

#define SLIST_ENTRY SINGLE_LIST_ENTRY
#define _SLIST_ENTRY _SINGLE_LIST_ENTRY
#define PSLIST_ENTRY PSINGLE_LIST_ENTRY

typedef union _SLIST_HEADER {
  ULONGLONG Alignment;
  _ANONYMOUS_STRUCT struct {
    SLIST_ENTRY Next;
    USHORT Depth;
    USHORT Sequence;
  } DUMMYSTRUCTNAME;
} SLIST_HEADER, *PSLIST_HEADER;

#define ANYSIZE_ARRAY 1

#ifndef NOMINMAX
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#endif

#define MSR_IA32_SYSENTER_CS  0x174
#define MSR_IA32_SYSENTER_ESP 0x175
#define MSR_IA32_SYSENTER_EIP 0x176