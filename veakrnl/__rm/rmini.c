/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : rmini.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : RingMode Syscall Init
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 30-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file rmini.c
 * --------------------------------------------------------------------- */

/* MACRO */
#define MSR_IA32_SYSENTER_CS  0x174
#define MSR_IA32_SYSENTER_ESP 0x175
#define MSR_IA32_SYSENTER_EIP 0x176

VOID
VEAFAST
KsiFastCallService();

VOID
VEAPI
RmInitSyscall(VOID)
{
    PKTHREAD KernelThread = PtGetCurrentThread();
    ULONG UpperStack;

    UpperStack = (ULONG)KernelThread->InitialStack;
    /* Modern CPUs now support SYSENTER */
    __WriteMSR(MSR_IA32_SYSENTER_CS, KGDT32_R0_CODE);
    __WriteMSR(MSR_IA32_SYSENTER_ESP, (ULONG_PTR)UpperStack);
    __WriteMSR(MSR_IA32_SYSENTER_EIP, (ULONG_PTR)KsiFastCallService);
}