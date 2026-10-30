/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obhandle.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Handle
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 19-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obhandle.c
 * --------------------------------------------------------------------- */

PHANDLE_TABLE ObpKernelHandleTable;

PHANDLE_TABLE
VEAPI
ObpGetHandleTable(IN OUT PHANDLE Handle, IN KPROCESSOR_MODE Mode)
{
    PEPROCESS Process;

    if (ObpIsKernelHandle(*Handle, Mode))
    {
        *Handle = ObKernelHandleToHandle(*Handle);   /* buang flag sebelum HANDLE_TO_INDEX */
        return ObpKernelHandleTable;
    }

    Process = (PEPROCESS)PtGetCurrentProcess();
    return Process ? (PHANDLE_TABLE)Process->ObjectTable : NULL;
}

BOOLEAN
VEAPI
ObpInitKernelHandleTable(VOID)
{
    ObpKernelHandleTable = UlCreateHandleTable(OB_KERNEL_HANDLE_TABLE_SIZE);
    if (!ObpKernelHandleTable)
    {
        DPRINT("OB: Failed to create Kernel Handle Table!\n\r");
        return FALSE;
    }
    return TRUE;
}

VEASTATUS
VEAPI
ObpCreateHandle(
    IN  PVOID           Object,
    IN  ULONG           ObjAttributes,
    IN  KPROCESSOR_MODE AccessMode,
    IN  ACCESS_MASK     GrantedAccess,
    OUT PHANDLE         ReturnedHandle
)
{
    PEPROCESS     Process = (PEPROCESS)PtGetCurrentProcess();
    PHANDLE_TABLE Table;
    BOOLEAN       IsKernel = FALSE;
    HANDLE        NewHandle;

    *ReturnedHandle = NULL;

    if ((ObjAttributes & OBJ_KERNEL_HANDLE) || !Process)
    {
        /* User mode tidak boleh minta handle kernel */
        if (AccessMode == UserMode)
            return STATUS_INVALID_PARAMETER;
        IsKernel = TRUE;
    }

    Table = IsKernel ? ObpKernelHandleTable : (PHANDLE_TABLE)Process->ObjectTable;
    if (!Table)
        return STATUS_INVALID_PARAMETER;

    /* Handle punya reference sendiri */
    ObReferenceObject(Object);

    NewHandle = UlCreateHandle(Table, Object, GrantedAccess);
    if (!NewHandle)
    {
        ObDereferenceObject(Object);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    if (IsKernel)
        NewHandle = ObMarkHandleAsKernelHandle(NewHandle);

    *ReturnedHandle = NewHandle;
    return STATUS_SUCCESS;
}