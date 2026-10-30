/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : stack.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Stack Check Guard
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 22-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file stack.c
 * --------------------------------------------------------------------- */

ULONG_PTR __stack_chk_guard = 0x1234ABCD;

NORET
VOID
VEAPI
__stack_chk_fail(VOID)
{
    KsBugCheckEx(
        KERNEL_SECURITY_CHECK_FAILURE,
        FAST_FAIL_STACK_COOKIE_CHECK_FAILED,
        (ULONG_PTR)__builtin_return_address(0),
        (ULONG_PTR)__stack_chk_guard,
        0
    );

    while(1) { asm("hlt"); }
}
