/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   FILE        : seheng3.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Hardware Trap EIP Rewiring Exception Dispatcher
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

//
// Primitif asm. Nama simbol mengikuti prefix target (PE: "_", ELF: "").  
//

#define _VEA_STR_(x)  #x
#define _VEA_STR(x)   _VEA_STR_(x)
#define _VEA_SYM(n)   _VEA_STR(__USER_LABEL_PREFIX__) #n

//
// VeaSehRegister(eax = Frame)
//   Simpan ebp/ebx/esi/edi, esp pasca-return, dan return address, lalu link
//   frame ke FS:[0]. Return 0. "Return kedua" dilakukan oleh VeaSehResume. 
//  

__asm__(
    ".text\n"
    ".globl " _VEA_SYM(VeaSehRegister) "\n"
    _VEA_SYM(VeaSehRegister) ":\n\t"
    "movl (%esp), %ecx\n\t"
    "movl %ecx, 32(%eax)\n\t"        /* ReturnAddr */
    "leal 4(%esp), %ecx\n\t"
    "movl %ecx, 16(%eax)\n\t"        /* Esp (setelah ret) */
    "movl %ebp, 12(%eax)\n\t"
    "movl %ebx, 20(%eax)\n\t"
    "movl %esi, 24(%eax)\n\t"
    "movl %edi, 28(%eax)\n\t"
    "movl %fs:0, %ecx\n\t"
    "movl %ecx, (%eax)\n\t"          /* Next */
    "movl %eax, %fs:0\n\t"           /* head = frame */
    "xorl %eax, %eax\n\t"
    "ret\n"
);

//
// VeaSehResume(eax = Frame, edx = Mode)
//   Mode 1: simpan register dispatcher, sisakan scratch, pulihkan ebp/ebx/esi/edi
//           frame TAPI ESP TETAP DALAM (stack fault utuh), lompat ke ReturnAddr
//           dengan eax=1. Filter diakhiri _VEA_FILTER_RETURN -> kembali ke sini.
//   Mode 2: pulihkan SEMUA register termasuk esp, lompat dengan eax=2. 
//

__asm__(
    ".text\n"
    ".globl " _VEA_SYM(VeaSehResume) "\n"
    _VEA_SYM(VeaSehResume) ":\n\t"
    "cmpl $1, %edx\n\t"
    "jne 1f\n\t"
    "pushl %ebp\n\t"
    "pushl %ebx\n\t"
    "pushl %esi\n\t"
    "pushl %edi\n\t"
    "movl %esp, 36(%eax)\n\t"        /* FilterEsp */
    "subl $1024, %esp\n\t"           /* VEA_SEH_FILTER_SCRATCH */
    "jmp 2f\n"
    "1:\n\t"
    "movl 16(%eax), %esp\n"
    "2:\n\t"
    "movl 12(%eax), %ebp\n\t"
    "movl 20(%eax), %ebx\n\t"
    "movl 24(%eax), %esi\n\t"
    "movl 28(%eax), %edi\n\t"
    "movl 32(%eax), %ecx\n\t"
    "movl %edx, %eax\n\t"
    "jmp *%ecx\n"
);

VOID
VeaSehGetStackBounds(
    OUT PULONG Low,
    OUT PULONG High
)
{
    // Ambil thread yang sedang berjalan di CPU saat ini
    PKTHREAD CurrentThread = PtGetCurrentThread();

    if (CurrentThread != NULL) {
        // Asumsi struktur PKTHREAD punya field StackLimit dan StackBase
        *Low  = (ULONG)CurrentThread->StackLimit; // Batas bawah stack (low address)
        *High = (ULONG)CurrentThread->InitialStack;  // Batas atas stack (high address)
    } else {
        // Fallback darurat kalau dipanggil sebelum scheduler/thread init
        // Ambil dari register ESP saat ini
        ULONG CurrentEsp;
        asm volatile ("movl %%esp, %0" : "=r"(CurrentEsp));
        
        *Low  = CurrentEsp & 0xFFFF0000; // Asumsi page-aligned 64KB stack
        *High = *Low + 0x10000;          // 64KB stack size
    }
}
 
static BOOLEAN
VeaSehIsValidFrame(
    PVEA_SEH_FRAME Frame,
    PVEA_SEH_FRAME Prev,
    ULONG Low,
    ULONG High)
{
    ULONG A = (ULONG)Frame;
 
    if (A == 0 || A == 0xFFFFFFFF) return FALSE;       /* akhir chain */
    if (A & 3) return FALSE;                           /* alignment */
    if (A < Low || A + sizeof(*Frame) > High) return FALSE;
    if (Prev != NULL && A <= (ULONG)Prev) return FALSE; /* harus naik */
    return Frame->Scope != NULL;
}

BOOLEAN
VEAPI
KsiDispatchSehException(
    IN PKTRAP_FRAME TrapFrame,
    IN VEASTATUS ExceptionCode
)
{
    VEA_EXCEPTION_POINTERS Pointers;
    PVEA_SEH_FRAME Head = VeaSehGetFrameHead();
    PVEA_SEH_FRAME Frame, Prev = NULL, Target = NULL;
    ULONG Low, High;
 
    /* Fault dari user mode bukan urusan SEH kernel */
    if (TrapFrame != NULL && (TrapFrame->SegCs & 3) != 0)
        return FALSE;
 
    Pointers.ExceptionCode = ExceptionCode;
    Pointers.TrapFrame     = TrapFrame;
    VeaSehGetStackBounds(&Low, &High);
 
    for (Frame = Head; VeaSehIsValidFrame(Frame, Prev, Low, High);
         Prev = Frame, Frame = Frame->Next)
    {
        LONG Action;
 
        if (Frame->Scope->Target == NULL)
            continue;                       /* __finally dilewati di pass 1 */
 
        Frame->ExceptionCode     = ExceptionCode;
        Frame->ExceptionPointers = &Pointers;       
 
        /* Fault di dalam filter harus naik ke frame luar, bukan re-entrant */
        VeaSehSetFrameHead(Frame->Next);
        Action = (LONG)VeaSehResume(Frame, 1);
        VeaSehSetFrameHead(Head);
 
        if (Action == VEA_EXCEPTION_EXECUTE_HANDLER) {
            Target = Frame;
            Target->ExceptionCode = ExceptionCode;
            Target->TrapFrame = TrapFrame;
            break;
        }
        if (Action == VEA_EXCEPTION_CONTINUE_EXECUTION) {
            /* TrapFrame tidak diubah: iret mengulang instruksi fault
               dengan konteks CPU asli, bukan jmp buta */
            Frame->ExceptionPointers = NULL;
            return TRUE;
        }
        /* CONTINUE_SEARCH: lanjut ke frame luar */
    }
 
    if (Target == NULL) {
        if (TrapFrame == NULL)
            KsBugCheckEx(KMODE_EXCEPTION_NOT_HANDLED, ExceptionCode, 0, 0, 0);
        return FALSE;                       /* pemanggil trap yang bugcheck */
    }
 
    /* ---- PASS 2: unwind. Jalankan __finally dari dalam ke luar. ---- */
    for (Frame = Head; Frame != Target; Frame = Frame->Next)
    {
        VeaSehSetFrameHead(Frame->Next);    /* lepas dulu, fault di finally naik ke luar */
 
        if (Frame->Scope->Target == NULL) {
            Frame->ExceptionCode = ExceptionCode;
            /* ExceptionPointers tetap VEA_SEH_ABNORMAL => AbnormalTermination() == TRUE */
            VeaSehResume(Frame, 1);
        }
    }
 
    VeaSehSetFrameHead(Target->Next);
 
    /* Kita lompat langsung, tanpa iret: kembalikan IF jika konteks asal menyala */
    if (TrapFrame != NULL && (TrapFrame->EFlags & 0x200))
        asm volatile ("sti" : : : "memory");
 
    VeaSehResume(Target, 2);                /* tidak kembali */
    __builtin_unreachable();
}


/* EOF */