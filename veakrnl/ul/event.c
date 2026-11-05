/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : event.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Upper Layer Event
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 30-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file event.c
 * --------------------------------------------------------------------- */

GENERIC_MAPPING UlpEventMapping =
{
    VEA_STANDARD_RIGHTS_READ | EVENT_QUERY_STATE,
    VEA_STANDARD_RIGHTS_WRITE | EVENT_MODIFY_STATE,
    VEA_STANDARD_RIGHTS_EXECUTE | VEA_SYNCHRONIZE,
    EVENT_ALL_ACCESS
};

POBJECT_TYPE UlEventObjectType;

BOOLEAN
VEAPI
UlInitializeEventImplementation(VOID)
{
    OBJECT_TYPE_INITIALIZER Initializer;
    VEASTATUS Status;

    RtlZeroMemory(&Initializer, sizeof(Initializer));

    Initializer.ObjectSize = sizeof(KEVENT);
    Initializer.Length = sizeof(Initializer);
    Initializer.AllowAttachByDefault = FALSE;
    Initializer.DefaultNonPagedPoolCharge = sizeof(KEVENT);
    Initializer.PoolTag = 'Evnt';
    Initializer.PoolType = NonPagedPool;
    Initializer.GenericMapping = UlpEventMapping;
    Initializer.ValidAccessMask = EVENT_ALL_ACCESS;
    
    Status = ObCreateObjectType(
        "Event",
        &Initializer,
        &UlEventObjectType
    );
    if (!VEA_SUCCESS(Status)) return FALSE;

    return TRUE;
}