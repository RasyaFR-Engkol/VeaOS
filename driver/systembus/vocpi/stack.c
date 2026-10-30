/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : stack.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : STACK
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>
#include "Include/vocpifunc.h"

/* Revision History ------------------------------------------------------
 * DATE       : 23-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file stack.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
VOCPIPushStack(
    IN PVOCPI_STACK Stack,
    IN VOCPI_OBJECT Object
)
{
    if (!Stack) return STATUS_INVALID_PARAMETER;
    if (Stack->Top >= VOCPI_STACK_MAX_DEPTH) return STATUS_STACK_OVERFLOW;

    Stack->Items[Stack->Top++] = Object;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VOCPIPopStack(
    IN PVOCPI_STACK Stack,
    OUT PVOCPI_OBJECT Object
)
{
    if (!Stack || !Object) return STATUS_INVALID_PARAMETER;
    if (Stack->Top == 0) return STATUS_STACK_UNDERFLOW;

    *Object = Stack->Items[--Stack->Top];
    return STATUS_SUCCESS;
}