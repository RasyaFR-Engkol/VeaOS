/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : seheng.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : SEH Engine
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <rtl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 10-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file seheng.c
 * --------------------------------------------------------------------- */

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

/* EOF */