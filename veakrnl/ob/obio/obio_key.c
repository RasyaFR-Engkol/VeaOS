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
    IN HANDLE RootDirectory OPTIONAL,
    OUT PHANDLE Handle, 
    IN PANSI_STRING KeyName,
    IN ACCESS_MASK DesiredAccess
)
{
    OBJECT_ATTRIBUTES Attributes;

    /* Validate the wrapper arguments before constructing attributes. */
    if (!Handle || !KeyName || !KeyName->Buffer || KeyName->Length == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    *Handle = NULL;

    /* Keep RootDirectory as a handle; VeaOpenVeaKey resolves it before the
       object-manager lookup. */
    InitializeObjectAttributes(&Attributes, KeyName, 0, RootDirectory, NULL);

    /* Open an existing key only; this path never creates a missing key. */
    return VeaOpenVeaKey(Handle, DesiredAccess, &Attributes);
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

VEASTATUS
VEAPI
ObioGetValueKey(
    IN HANDLE Handle,
    IN PANSI_STRING ValueName,
    OUT PVOID Buffer,
    IN ULONG BufferSize,
    OUT PULONG Type,
    OUT PULONG ResultSizeOut
)
{
    VEASTATUS Status;
    ULONG ResultSize, AvailSize;
    ULONG Disposition;
    ULONG TypeLocal;
    PVOID LocalBuffer;

    /* Condition checking */
    if (!Handle || 
        !ValueName || 
        !ValueName->Buffer ||
        ValueName->Length == 0
    ) return STATUS_INVALID_PARAMETER;

    /* Get the value */
    Status = VeaQueryValueKey(
        Handle,
        ValueName,
        NULL,
        NULL,
        0,
        &ResultSize
    );

    /* Is status okay? */
    if (Status != STATUS_BUFFER_TOO_SMALL)
    {
        /* Another Status */
        return Status;
    }

    /* Make sure caller buffer size is enough */
    if (BufferSize < ResultSize)
    {
        /* Caller must know size of the data so it can be allocated 
           as flexible. 
        */
        if (ResultSizeOut)
            *ResultSizeOut = ResultSize;

        return STATUS_BUFFER_TOO_SMALL;
    }

    /* Set AvailSize */
    AvailSize = ResultSize;

    LocalBuffer = UlAllocatePoolZero(NonPagedPool, ResultSize, TAG_VEAKEY_BUFFER);
    if (!LocalBuffer) return STATUS_INSUFFICIENT_MEMORY;

    /* Easy to get the buffer now */
    Status = VeaQueryValueKey(
        Handle,
        ValueName,
        &TypeLocal,
        LocalBuffer,
        AvailSize,
        &ResultSize
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("ERR: VeaQueryValueKey failed: 0x%x\n", Status);
        return Status;
    }

    /* I hope the buffer is okay. We can copy it directly */
    RtlCopyMemory(
        Buffer,
        LocalBuffer,
        ResultSize
    );

    if (Type)
    {
        *Type = TypeLocal;
    }

    if (ResultSizeOut)
    {
        *ResultSizeOut = ResultSize;
    }

    if (LocalBuffer)
    {
        UlFreePoolWithTag(LocalBuffer, TAG_VEAKEY_BUFFER);
    }

    return STATUS_SUCCESS;
}