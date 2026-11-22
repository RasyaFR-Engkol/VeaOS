/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : crt_internal.h
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VEA Internal CRT Helpers
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#pragma once

#include "pttypes.h"
#include <procbind.h>
#include <veastatus.h>
#include <rtl.h>
#include <vz_wrappers.h>

PRTL_USER_PROCESS_PARAMETERS
VEAPI
RtlNormalizeProcessParams(
    IN OUT PRTL_USER_PROCESS_PARAMETERS Params
);

typedef VOID (__cdecl *PVEA_INIT_FN)(VOID);

extern PVEA_INIT_FN __init_array_start[] __attribute__((weak));
extern PVEA_INIT_FN __init_array_end[]   __attribute__((weak));
extern PVEA_INIT_FN __fini_array_start[] __attribute__((weak));
extern PVEA_INIT_FN __fini_array_end[]   __attribute__((weak));

static inline VOID
CrtpRunInitArray(VOID)
{
    if (!__init_array_start || !__init_array_end)
    {
        return;
    }

    for (PVEA_INIT_FN *Fn = __init_array_start; Fn < __init_array_end; Fn++)
    {
        if (*Fn)
        {
            (*Fn)();
        }
    }
}

static inline VOID
CrtpRunFiniArray(VOID)
{
    if (!__fini_array_start || !__fini_array_end)
    {
        return;
    }

    for (PVEA_INIT_FN *Fn = __fini_array_end; Fn > __fini_array_start; )
    {
        Fn--;
        if (*Fn)
        {
            (*Fn)();
        }
    }
}