/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : kpcinit.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : KPC Initialization
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 10-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file kpcinit.c
 * --------------------------------------------------------------------- */

static GENERIC_MAPPING KpcpPortMapping = 
{
    READ_CONTROL | PORT_CONNECT,
    VEA_DELETE | PORT_CONNECT,
    0,
    PORT_ALL_ACCESS
};

POBJECT_TYPE KpcPortObjectType;

BOOLEAN
VEAPI
KpcInitSystem(VOID)
//
// Routine Description:
// Initializing KPC Subsystem in Kernel
// 
// Return BOOLEAN:
// FALSE if Initialize failed, TRUE if Initialize succeed
//
{
    OBJECT_TYPE_INITIALIZER Initializer;

    RtlZeroMemory(&Initializer, sizeof(Initializer));

    Initializer.AllowAttachByDefault = FALSE;
    Initializer.DefaultNonPagedPoolCharge = sizeof(KPC_PORT_OBJECT);
    Initializer.ObjectSize = sizeof(KPC_PORT_OBJECT);
    Initializer.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    Initializer.GenericMapping = KpcpPortMapping;
    Initializer.CloseRoutine = NULL; //KpcpClosePort;
    Initializer.DeleteRoutine = NULL; //KpcpDeletePort;
    Initializer.ValidAccessMask = PORT_ALL_ACCESS;
    ObCreateObjectType(
        "Port",
        &Initializer,
        &KpcPortObjectType
    );

    return TRUE;
}