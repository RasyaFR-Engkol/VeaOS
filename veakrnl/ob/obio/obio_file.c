/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_file.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : File Object handling
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 14-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_file.c
 * --------------------------------------------------------------------- */

BOOLEAN
ObioIsOpenPacketValid(IN POPEN_PACKET OpenPacket)
{
    if (!(OpenPacket) ||
        (OpenPacket->Type != IO_TYPE_OPEN_PACKET) ||
        (OpenPacket->Size != sizeof(OPEN_PACKET)))
    {
        return FALSE;
    }

    return TRUE;
}

VEASTATUS
VEAPI
ObioParseFile(
    IN PVOID ParseObject,
    IN POB_LOOKUP_CONTEXT LookupContext,
    IN PVOID Context,
    OUT PVOID* ResolvedObject
)
{
    PVOID DeviceObject;
    POPEN_PACKET OpenPacket = (POPEN_PACKET)Context;

    /* Validate first */
    if (!ObioIsOpenPacketValid(OpenPacket)) return STATUS_OBJECT_TYPE_MISMATCH;

    DeviceObject = ObioGetRelatedDeviceObject(ParseObject);
    OpenPacket->RelatedFileObject = ParseObject;

    return ObioParseDevice(
        DeviceObject,
        LookupContext,
        OpenPacket,
        ResolvedObject
    );
}

VEASTATUS
VEAPI
ObioCreateFile(
    OUT PHANDLE FileHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    OUT POIP_INFORMATION IoStatusBlock,
    IN PLARGE_INTEGER AllocationSize OPTIONAL,
    IN ULONG FileAttributes,
    IN ULONG ShareAccess,
    IN ULONG Disposition,
    IN ULONG CreateOptions,
    IN PVOID EaBuffer OPTIONAL,     // <--- TAMBAHAN EA BUFFER
    IN ULONG EaLength              // <--- TAMBAHAN EA LENGTH
)
{
    OPEN_PACKET OpenPacket;
    HANDLE LocalHandle = NULL;
    VEASTATUS Status;

    if (!FileHandle || !ObjectAttributes)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Safetynet: Kalau ada EaLength tapi EaBuffer NULL (atau sebaliknya)
    if ((EaBuffer && EaLength == 0) || (!EaBuffer && EaLength > 0))
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(&OpenPacket, sizeof(OPEN_PACKET));
    OpenPacket.Type = IO_TYPE_OPEN_PACKET;
    OpenPacket.Size = sizeof(OPEN_PACKET);
    OpenPacket.CreateOptions = CreateOptions;
    OpenPacket.ShareAccess = (USHORT)ShareAccess;
    OpenPacket.Disposition = Disposition;
    OpenPacket.FileAttributes = (USHORT)FileAttributes;

    if (AllocationSize)
    {
        OpenPacket.AllocationSize = *AllocationSize;
    }

    OpenPacket.EaBuffer = EaBuffer;
    OpenPacket.EaLength = EaLength;

    Status = ObOpenObjectByName(
        ObjectAttributes,
        ObpFileObjectType,
        KernelMode,
        DesiredAccess,
        &OpenPacket,
        &LocalHandle
    );

    if (VEA_SUCCESS(Status))
    {
        if (VEA_SUCCESS(OpenPacket.FinalStatus))
        {
            *FileHandle = LocalHandle;
            if (IoStatusBlock)
            {
                IoStatusBlock->Status = OpenPacket.FinalStatus;
                IoStatusBlock->Information = OpenPacket.Information;
            }
            return OpenPacket.FinalStatus;
        }
        else
        {
            VeaClose(LocalHandle);
            return OpenPacket.FinalStatus;
        }
    }

    return Status;
}

VEASTATUS
VEAPI
VeaOpenFile(
    OUT PHANDLE FileHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    OUT POIP_INFORMATION IoStatusBlock,
    IN ULONG ShareAccess,
    IN ULONG OpenOptions
)
{
    return ObioCreateFile(
        FileHandle,
        DesiredAccess,
        ObjectAttributes,
        IoStatusBlock,
        NULL,
        FILE_ATTRIBUTE_NORMAL, 
        ShareAccess,
        FILE_OPEN,
        OpenOptions,
        NULL, // No EA Buffer
        0     // No EA Length
    );
}

VEASTATUS
VEAPI
VeaCreateFile(
    OUT PHANDLE FileHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    OUT POIP_INFORMATION IoStatusBlock,
    IN PLARGE_INTEGER AllocationSize OPTIONAL,
    IN ULONG FileAttributes,
    IN ULONG ShareAccess,
    IN ULONG CreateDisposition,
    IN ULONG CreateOptions,
    IN PVOID EaBuffer OPTIONAL,
    IN ULONG EaLength
)
{
    return ObioCreateFile(
        FileHandle,
        DesiredAccess,
        ObjectAttributes,
        IoStatusBlock,
        AllocationSize,
        FileAttributes,
        ShareAccess,
        CreateDisposition,
        CreateOptions,
        EaBuffer,
        EaLength
    );
}
