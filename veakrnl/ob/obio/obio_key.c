/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_key.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Wrapper abstraction to VeaKey from Obio
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 02-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_key.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
ObioOpenVeaKey(
    OUT PHANDLE Handle, 
    IN PANSI_STRING KeyName,
    IN ACCESS_MASK DesiredAccess
)
{
    OBJECT_ATTRIBUTES Attributes;
    PEPROCESS Process = (PEPROCESS)PtGetCurrentProcess();

    if (!Process) return STATUS_UNSUCCESSFULL;

    InitializeObjectAttributes(&Attributes, KeyName, 0, NULL, NULL);

    // Lewat ObOpenObjectByName langsung -- BUKAN VeaCreateKey.
    // Ini beneran cuma "open", gak ada efek samping create sama sekali.
    return ObOpenObjectByName(
        Process->ObjectTable,
        &Attributes,
        VkKeyObjectType,
        KernelMode,
        DesiredAccess,
        Handle
    );
}

VEASTATUS
VEAPI
ObioCreateVeaKey(
    IN HANDLE RootDirectory OPTIONAL,
    IN PANSI_STRING StringKey,
    IN ULONG Option,
    IN ACCESS_MASK DesiredAccess,
    OUT PULONG Disposition,
    OUT PHANDLE Handle
)
{
    VEASTATUS Status;
    OBJECT_ATTRIBUTES Attributes;

    InitializeObjectAttributes(&Attributes, StringKey, 0, RootDirectory, NULL);

    Status = VeaCreateKey(Handle, DesiredAccess, &Attributes, 0, NULL, Option, Disposition);
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    return STATUS_SUCCESS;
}