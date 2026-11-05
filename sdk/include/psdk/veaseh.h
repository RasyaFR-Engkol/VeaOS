/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   FILE        : veaseh.h
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VeaSEH v2 - SEH engine like PSEH3 (GCC, x86-32 kernel)
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#pragma once

#include <procbind.h>
#include <stddef.h>
#include <veastatus.h>

#define VEA_EXCEPTION_EXECUTE_HANDLER      1
#define VEA_EXCEPTION_CONTINUE_SEARCH      0
#define VEA_EXCEPTION_CONTINUE_EXECUTION  -1

/* Nilai penanda "__finally dimasuki lewat jalur abnormal" */
#define VEA_SEH_ABNORMAL  ((PVOID)1)

/* Ruang scratch yang disisakan saat filter/finally dijalankan di atas
   stack fault. Wajib: dengan -maccumulate-outgoing-args compiler menulis
   argumen call ke [esp+N], dan tanpa scratch itu akan menimpa frame dispatcher. */
#define VEA_SEH_FILTER_SCRATCH  1024

/* ---------------------------------------------------------------------- */
/* Struktur                                                                */
/* ---------------------------------------------------------------------- */

typedef struct _VEA_EXCEPTION_POINTERS {
    VEASTATUS        ExceptionCode;
    PKTRAP_FRAME     TrapFrame;        /* NULL jika software raise */
} VEA_EXCEPTION_POINTERS, *PVEA_EXCEPTION_POINTERS;

/* Tabel statis per blok try (disimpan di .rdata, bukan di stack) */
typedef struct _VEA_SEH_SCOPE {
    PVOID Filter;   /* label filter (__except) atau body (__finally) */
    PVOID Target;   /* label handler __except; NULL = ini __finally  */
} VEA_SEH_SCOPE;

/* Dua field pertama sama dengan EXCEPTION_REGISTRATION_RECORD. JANGAN DIGESER.
   Offset dipakai oleh asm di seheng3.c (lihat static assert di bawah). */
typedef struct _VEA_SEH_FRAME {
    struct _VEA_SEH_FRAME *Next;        /*  0 */
    PVOID                  Handler;     /*  4 reserved, kompat layout NT */
    const VEA_SEH_SCOPE   *Scope;       /*  8 */
    ULONG                  Ebp;         /* 12 */
    ULONG                  Esp;         /* 16 esp setelah return dari Register */
    ULONG                  Ebx;         /* 20 */
    ULONG                  Esi;         /* 24 */
    ULONG                  Edi;         /* 28 */
    ULONG                  ReturnAddr;  /* 32 titik "return kedua" */
    ULONG                  FilterEsp;   /* 36 esp untuk kembali ke dispatcher */
    ULONG                  ExceptionCode;     /* 40 */
    PVOID                  ExceptionPointers; /* 44 except: ptr, finally: penanda abnormal */
    PKTRAP_FRAME           TrapFrame;
} VEA_SEH_FRAME, *PVEA_SEH_FRAME;

_Static_assert(offsetof(VEA_SEH_FRAME, Ebp)        == 12, "asm offset");
_Static_assert(offsetof(VEA_SEH_FRAME, Esp)        == 16, "asm offset");
_Static_assert(offsetof(VEA_SEH_FRAME, Ebx)        == 20, "asm offset");
_Static_assert(offsetof(VEA_SEH_FRAME, Esi)        == 24, "asm offset");
_Static_assert(offsetof(VEA_SEH_FRAME, Edi)        == 28, "asm offset");
_Static_assert(offsetof(VEA_SEH_FRAME, ReturnAddr) == 32, "asm offset");
_Static_assert(offsetof(VEA_SEH_FRAME, FilterEsp)  == 36, "asm offset");

/* ---------------------------------------------------------------------- */
/* Primitif (diimplementasi di seheng3.c)                                  */
/* ---------------------------------------------------------------------- */

/* Seperti setjmp: return 0 saat registrasi, lalu "return lagi" dengan
     1 = jalankan filter (pass 1, stack fault masih utuh)
     2 = jalankan handler (pass 2, setelah unwind, semua register dipulihkan)
   returns_twice membuat GCC memperlakukannya seperti setjmp. */
int __attribute__((regparm(1), returns_twice))
VeaSehRegister(volatile VEA_SEH_FRAME *Frame);

/* mode 1: panggil filter/finally, kembali ke pemanggil dengan eax = hasil.
   mode 2: lompat ke handler, tidak kembali. */
int __attribute__((regparm(2)))
VeaSehResume(volatile VEA_SEH_FRAME *Frame, int Mode);

BOOLEAN VEAPI KsiDispatchSehException(IN PKTRAP_FRAME TrapFrame,
                                      IN VEASTATUS ExceptionCode);

/* ---------------------------------------------------------------------- */
/* FS:[0]                                                                  */
/* ---------------------------------------------------------------------- */

static __inline PVEA_SEH_FRAME VeaSehGetFrameHead(VOID) {
    PVEA_SEH_FRAME Head;
    asm volatile ("movl %%fs:0, %0" : "=r"(Head));
    return Head;
}

static __inline VOID VeaSehSetFrameHead(PVEA_SEH_FRAME Head) {
    asm volatile ("movl %0, %%fs:0" : : "r"(Head) : "memory");
}

/* Hanya unlink jika frame ini memang head (aman dipanggil dua kali) */
static __inline VOID VeaSehAutoCleanup(volatile VEA_SEH_FRAME *Frame) {
    if (VeaSehGetFrameHead() == (PVEA_SEH_FRAME)Frame)
        VeaSehSetFrameHead(Frame->Next);
}

#define _VEA_AUTO_CLEANUP __attribute__((cleanup(VeaSehAutoCleanup)))

/* ---------------------------------------------------------------------- */
/* Helper compiler                                                         */
/* ---------------------------------------------------------------------- */

/* Paksa GCC memakai frame pointer di fungsi ini (alloca => frame_pointer_needed).
   Wajib: filter berjalan dengan ESP "dalam", jadi semua variabel lokal harus
   diakses lewat EBP. alloca(0) agar tidak menumbuhkan stack di dalam loop. */
#define _VEA_ENFORCE_FP() \
    asm volatile ("#\n" : : "m"(*(char*)__builtin_alloca(0)) : "memory")

/* Akhiri filter/finally: pulihkan ESP ke yang disimpan VeaSehResume, pop
   register dispatcher, lalu ret ke dispatcher dengan eax = hasil. */
#define _VEA_FILTER_RETURN(Result) \
    do { \
        asm volatile ( \
            "movl %[fe], %%esp\n\t" \
            "popl %%edi\n\t" \
            "popl %%esi\n\t" \
            "popl %%ebx\n\t" \
            "popl %%ebp\n\t" \
            "ret\n\t" \
            : : "a"(Result), [fe] "m"(_VeaFrame.FilterEsp) : "memory"); \
        __builtin_unreachable(); \
    } while (0)

/* ---------------------------------------------------------------------- */
/* Macro                                                                   */
/*                                                                         */
/*   _VEA_TRY { ... } _VEA_EXCEPT(filter) { ... } _VEA_END                 */
/*   _VEA_TRY { ... } _VEA_FINALLY        { ... } _VEA_END                 */
/* ---------------------------------------------------------------------- */

#define _VEA_TRY \
    if (1) { \
        __label__ _VeaL_BeforeTry, _VeaL_DoTry, _VeaL_AfterTry, _VeaL_EndTry, \
                  _VeaL_Target, _VeaL_Filter, _VeaL_Leave; \
        volatile VEA_SEH_FRAME _VEA_AUTO_CLEANUP _VeaFrame; \
        goto _VeaL_BeforeTry; \
        { \
            _VeaL_Leave: (void)0; \
            goto _VeaL_AfterTry; \
            goto _VeaL_Leave; \
        _VeaL_DoTry: \
            if (1)

#define _VEA_EXCEPT(...) \
        } \
    _VeaL_AfterTry: (void)0; \
        goto _VeaL_EndTry; \
    \
    _VeaL_BeforeTry: (void)0; \
        { \
            static const VEA_SEH_SCOPE _VeaScope = \
                { &&_VeaL_Filter, &&_VeaL_Target }; \
            int _VeaR; \
            _VeaFrame.Handler           = NULL; \
            _VeaFrame.Scope             = &_VeaScope; \
            _VeaFrame.ExceptionCode     = 0; \
            _VeaFrame.ExceptionPointers = NULL; \
            _VeaFrame.FilterEsp         = 0; \
            _VeaR = VeaSehRegister(&_VeaFrame); \
            if (_VeaR) { \
                if (_VeaR == 1) goto _VeaL_Filter; \
                goto _VeaL_Target; \
            } \
            goto _VeaL_DoTry; \
        } \
    \
    _VeaL_Filter: (void)0; \
        _VEA_ENFORCE_FP(); \
        { \
            LONG _VeaAction = (LONG)(__VA_ARGS__); \
            _VEA_FILTER_RETURN(_VeaAction); \
        } \
    \
    _VeaL_Target: (void)0; \
        _VEA_ENFORCE_FP(); \
        if (1) { \
            if (1)

#define _VEA_FINALLY \
        } \
    _VeaL_AfterTry: (void)0; \
        /* jalur normal: tandai normal, lepas frame, jalankan body inline */ \
        _VeaFrame.ExceptionPointers = NULL; \
        VeaSehAutoCleanup(&_VeaFrame); \
        goto _VeaL_Filter; \
    \
    _VeaL_BeforeTry: (void)0; \
        { \
            static const VEA_SEH_SCOPE _VeaScope = { &&_VeaL_Filter, NULL }; \
            int _VeaR; \
            _VeaFrame.Handler           = NULL; \
            _VeaFrame.Scope             = &_VeaScope; \
            _VeaFrame.ExceptionCode     = 0; \
            _VeaFrame.FilterEsp         = 0; \
            _VeaFrame.ExceptionPointers = VEA_SEH_ABNORMAL; \
            _VeaR = VeaSehRegister(&_VeaFrame); \
            if (_VeaR) goto _VeaL_Filter; \
            goto _VeaL_DoTry; \
        } \
    \
    _VeaL_Target: (void)0; \
    _VeaL_Filter: (void)0; \
        _VEA_ENFORCE_FP(); \
        if (1) { \
            if (1)

#define _VEA_END \
        } \
        /* __finally yang dimasuki lewat dispatcher: kembali ke dispatcher */ \
        if (_VeaFrame.ExceptionPointers == VEA_SEH_ABNORMAL) \
            _VEA_FILTER_RETURN(0); \
    _VeaL_EndTry: (void)0; \
    }

#define _VEA_YIELD(Stmt) \
    do { \
        VeaSehAutoCleanup(&_VeaFrame); \
        Stmt; \
    } while (0)

/* Keluar dari blok try tanpa `return`/`goto` manual (wajib untuk __finally) */
#define _VEA_LEAVE  goto _VeaL_Leave

static __inline PVEA_EXCEPTION_POINTERS VeaSehGetExceptionPointers(volatile VEA_SEH_FRAME *Frame) {
    // Gunakan static thread-local / scratch area di Frame jika ingin mempertahankan format EXCEPTION_POINTERS
    static VEA_EXCEPTION_POINTERS Ep;
    Ep.ExceptionCode = Frame->ExceptionCode;
    Ep.TrapFrame     = Frame->TrapFrame;
    return &Ep;
}

/* Intrinsics */
#define GetExceptionCode()         (_VeaFrame.ExceptionCode)
#define GetExceptionInformation()  VeaSehGetExceptionPointers(&_VeaFrame)
#define AbnormalTermination()      (_VeaFrame.ExceptionPointers != NULL)