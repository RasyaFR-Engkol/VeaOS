/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   FILE        : veaseh.h
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Trap EIP Rewiring SEH Engine for VeaOS Kernel (32-bit x86)
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#pragma once
#include <procbind.h>

#define VEA_EXCEPTION_EXECUTE_HANDLER    1
#define VEA_EXCEPTION_CONTINUE_SEARCH     0
#define VEA_EXCEPTION_CONTINUE_EXECUTION -1

typedef struct _VEA_SEH_REGISTRATION_FRAME {
    struct _VEA_SEH_REGISTRATION_FRAME* Next; // Pointer ke FS:[0] sebelumnya
    PVOID TargetEip;                          // Alamat label target (EXCEPT atau FINALLY)
    ULONG FaultEip;
    ULONG ExceptionCode;                      // Status error (misal: STATUS_ACCESS_VIOLATION)
    ULONG SavedEbp;                           // Frame Pointer EBP sebelum masuk Try
    ULONG SavedEsp;                           // Stack Pointer ESP sebelum masuk Try
    BOOLEAN AbnormalTermination;              // TRUE jika masuk via Exception Trap
    BOOLEAN IsFinally;                        // TRUE jika ini blok _VEA_FINALLY
} VEA_SEH_REGISTRATION_FRAME, *PVEA_SEH_REGISTRATION_FRAME;

static __inline PVEA_SEH_REGISTRATION_FRAME VeaSehGetFrameHead(VOID) {
    PVEA_SEH_REGISTRATION_FRAME Head;
    asm volatile ("movl %%fs:0, %0" : "=r"(Head));
    return Head;
}

static __inline VOID VeaSehSetFrameHead(PVEA_SEH_REGISTRATION_FRAME Head) {
    asm volatile ("movl %0, %%fs:0" : : "r"(Head) : "memory");
}

static __inline VOID VeaSehAutoCleanup(volatile VEA_SEH_REGISTRATION_FRAME* Frame) {
    VeaSehSetFrameHead(Frame->Next);
}

#define _VEA_AUTO_CLEANUP __attribute__((cleanup(VeaSehAutoCleanup)))

#define _VEA_SCARE_GCC(TargetLabel) \
    asm volatile ("#" : : "r"(&&TargetLabel) : "memory")

/* Macro SEH Engine dengan Dynamic Filter */
#define _VEA_TRY \
    do { \
        __label__ _VeaTryStart, _VeaTryEnd, _VeaTargetLabel, _VeaExceptBody, _VeaDoneLabel; \
        volatile VEA_SEH_REGISTRATION_FRAME _VEA_AUTO_CLEANUP _VeaFrame; \
        \
        asm goto ("#\n" : : : "memory" : _VeaTargetLabel); \
        \
        _VeaFrame.TargetEip = &&_VeaTargetLabel; \
        _VeaFrame.FaultEip = 0; \
        _VeaFrame.ExceptionCode = 0; \
        _VeaFrame.AbnormalTermination = FALSE; \
        _VeaFrame.IsFinally = FALSE; \
        asm volatile ("movl %%ebp, %0" : "=r"(_VeaFrame.SavedEbp)); \
        asm volatile ("movl %%esp, %0" : "=r"(_VeaFrame.SavedEsp)); \
        \
        _VeaFrame.Next = VeaSehGetFrameHead(); \
        VeaSehSetFrameHead((PVEA_SEH_REGISTRATION_FRAME)&_VeaFrame); \
        \
        goto _VeaTryStart; \
        _VeaTryStart: {

#define _VEA_EXCEPT(FilterExpression) \
        } \
        _VeaTryEnd:; \
        goto _VeaDoneLabel; \
        _VeaTargetLabel: { \
            _VeaFrame.IsFinally = FALSE; \
            ULONG _TmpEbp = _VeaFrame.SavedEbp; \
            ULONG _TmpEsp = _VeaFrame.SavedEsp; \
            asm volatile ( \
                "movl %0, %%esp\n\t" \
                "movl %1, %%ebp\n\t" \
                : \
                : "r"(_TmpEsp), "r"(_TmpEbp) \
                : "memory" \
            ); \
            /* Evaluasi Dynamic Filter Expression */ \
            LONG _FilterAction = (FilterExpression); \
            if (_FilterAction == VEA_EXCEPTION_EXECUTE_HANDLER) { \
                goto _VeaExceptBody; \
            } else if (_FilterAction == VEA_EXCEPTION_CONTINUE_EXECUTION) { \
                /* Re-register frame dan coba ulang instruksi fault */ \
                _VeaFrame.Next = VeaSehGetFrameHead(); \
                VeaSehSetFrameHead((PVEA_SEH_REGISTRATION_FRAME)&_VeaFrame); \
                asm volatile ("jmp *%0" : : "r"(_VeaFrame.FaultEip) : "memory"); \
            } else { \
                /* VEA_EXCEPTION_CONTINUE_SEARCH: Pass exception ke outer frame! */ \
                KsiDispatchSehException(NULL, _VeaFrame.ExceptionCode); \
                goto _VeaDoneLabel; \
            } \
        } \
        _VeaExceptBody: {

#define _VEA_FINALLY \
        } \
        _VeaTryEnd:; \
        _VeaTargetLabel: { \
            _VeaFrame.IsFinally = TRUE; \
            ULONG _TmpEbp = _VeaFrame.SavedEbp; \
            ULONG _TmpEsp = _VeaFrame.SavedEsp; \
            asm volatile ( \
                "movl %0, %%esp\n\t" \
                "movl %1, %%ebp\n\t" \
                : \
                : "r"(_TmpEsp), "r"(_TmpEbp) \
                : "memory" \
            );

#define _VEA_END \
        } \
        _VeaDoneLabel:; \
        if (_VeaFrame.AbnormalTermination && _VeaFrame.IsFinally) { \
            KsiDispatchSehException(NULL, _VeaFrame.ExceptionCode); \
        } \
    } while(0);

/* Intrinsics */
#define GetExceptionCode()    (_VeaFrame.ExceptionCode)
#define AbnormalTermination() (_VeaFrame.AbnormalTermination)

/* EOF */