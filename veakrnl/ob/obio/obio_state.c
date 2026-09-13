/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_state.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : ObioState
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 02-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_state.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
ObioPnpChangeState(
    IN PDEVICE_NODE DeviceNode,
    IN DEVNODE_STATE DeviceNodeState
)
{
    if(!DeviceNode && !DeviceNodeState)
    {
        return;
    }

    /* Change state */
    if(DeviceNode->State == DeviceNodeState) 
    {
        return;
    }

    DeviceNode->State = DeviceNodeState;
}