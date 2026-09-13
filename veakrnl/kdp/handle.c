/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : handle.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Handling Serial command task
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 12-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file handle.c
 * --------------------------------------------------------------------- */

BOOLEAN
VEAPI
KdpProcessInput(PCHAR String)
{
    BOOLEAN Handled = FALSE;
    /* Incoming string could be like this:
    
       1. ok
       2. step 10
       3. makebreak main.c at 10

       TODO: Process it and pass the function to token
    */

    
    
    return Handled;
}