/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : rtlp.h
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Header file for private RTL 
 ----- Effective since 08-2026 til FOREVER  ------------------------------- */

#pragma once
#include "procbind.h"

/* Revision History ------------------------------------------------------
 * DATE       : 04-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file rtlp.h
 * --------------------------------------------------------------------- */


#if defined(BUILDING_VEAKRNL)
    /* 
     * PERHATIKAN: Include HANYA header spesifik allocator (ul.h / pool manager), 
     * JANGAN include umbrella header <veakrnl.h>!
     */
    #include <mm.h> 

    #define RtlpAllocateStringMemory(Bytes, Tag) UlAllocatePoolWithTag(0, (Bytes), (Tag))
    #define RtlpFreeStringMemory(Ptr, Tag)       UlFreePoolWithTag((Ptr), (Tag))

#elif defined(BUILDING_VEA_LOADER)
    /* Di Loader (osi386): Gunakan alokator loader jika ada, atau kembalikan NULL */
    #define RtlpAllocateStringMemory(Bytes, Tag) NULL
    #define RtlpFreeStringMemory(Ptr, Tag)       ((void)0)

#else
    /* Di Userland (ntdll/veadll): Gunakan Process Heap */
    #define RtlpAllocateStringMemory(Bytes, Tag) RtlAllocateHeap(RtlGetProcessHeap(), 0, (Bytes))
    #define RtlpFreeStringMemory(Ptr, Tag)       RtlFreeHeap(RtlGetProcessHeap(), 0, (Ptr))
#endif