/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_triage.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Triage Dump Function
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 03-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_triage.c
 * --------------------------------------------------------------------- */

ULONG ObioDumpLbaBlockMap[32];
ULONG ObioDumpLbaBlockCount = 0;
BOOLEAN ObioDumpSystemArmed = FALSE;
PIO_DUMP_WRITE_ROUTINE ObioDumpWriteRoutine;
PIO_DUMP_FLUSH_ROUTINE ObioDumpFlushRoutine;
BOOLEAN ObioAutoReboot;
static ALIGN(16) UCHAR ObioTriageDumpBuffer[_1KB * 16];

VEASTATUS
VEAPI
ObioGetDumpRoutine(IN HANDLE FileHandle)
//
// Routine Description:
//      Asking related DeviceObject to query IO Dump Routine if Disk is had one
//      or more     
// 
// Parameter:
// HANDLE FileHandle : Handle Pointer to File Handle
// 
// return VEASTATUS:
// STATUS_SUCCESS: Sucessfully querying IO Dump Routine from DISK
// STATUS_NOT_SUPPORTED/STATUS_NOT_IMPLEMENTED: Disk didn't actually had IO Dump Routine
// 
{
    VEASTATUS Status;
    OIP_INFORMATION IoStatusBlock;
    IO_DUMP_ROUTINE DumpRoutine;

    Status = VeaFsControlFile(
        FileHandle,
        NULL,
        NULL,
        NULL,
        &IoStatusBlock,
        IOCTL_DISK_QUERY_DUMP_ROUTINE,
        NULL,
        0,
        &DumpRoutine,        // Pake buffer tameng lu
        sizeof(DumpRoutine), // Kasih tau driver limitnya 64 byte
        TRUE
    );

    if (VEA_SUCCESS(Status))
    {
        ObioDumpWriteRoutine = DumpRoutine.Write;
        ObioDumpFlushRoutine = DumpRoutine.Flush;
    }

    DPRINT("[OBIO] DumpRoutine: Write %p | Flush %p\n", ObioDumpWriteRoutine, ObioDumpFlushRoutine);
    
    return Status;
}

VOID
VEAPI
ObioInitializeDumpSystem(VOID)
//
// Routine Description:
//     Initializing IO Dump Subsystem so IO can dump if there's BSOD.
//     Pre-allocates /SystemRoot/SystemReserved/minidumpreserved.sys,
//     retrieves physical LBA mapping from FS driver & VeaPM, and arms
//     the crash dump system.
// 
// Parameter:
// VOID
// 
{
    VEASTATUS Status;
    ANSI_STRING FilePath = 
        RTL_CONSTANT_ANSI_STRING("/SystemRoot/SystemReserved/minidumpreserved.sys");
    OIP_INFORMATION IoInformation;
    OBJECT_ATTRIBUTES Attributes;
    LARGE_INTEGER FileSize = {.QuadPart = _1KB * 16};
    HANDLE FileHandle;
    STARTING_VCN_INPUT_BUFFER InBuffer;
    PRETRIEVAL_POINTERS_BUFFER OutBuffer;
    ULONG OutBufferSize;
    PARTITION_INFORMATION PartitionInfo;
    LONGLONG PartitionStartLba = 0;
    ULONG i;

    //
    // Open access to Minidump Reserved file. If there is no file, the system
    // will automatically creating a new one mini dump reserved file and set
    // it as SYSTEM_FILE so the user should not know this file is exist
    //
    
    InitializeObjectAttributes(&Attributes, &FilePath, OBJ_KERNEL_HANDLE, NULL, NULL);

    Status = VeaCreateFile(
        &FileHandle,
        FILE_ALL_ACCESS,
        &Attributes,
        &IoInformation,
        &FileSize,
        FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN,
        FILE_SHARE_READ | FILE_SHARE_WRITE,  
        FILE_OPEN_IF,     
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
        NULL,
        0 
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("ObioInitializeDumpSystem: Failed to open/create minidump reserve file! Status: 0x%X\n", Status);
        return;
    }

    InBuffer.StartingVcn.QuadPart = 0;
    OutBufferSize = sizeof(RETRIEVAL_POINTERS_BUFFER) + (sizeof(OutBuffer->Extents[0]) * 7);
    OutBuffer = (PRETRIEVAL_POINTERS_BUFFER)UlAllocatePoolWithTag(
        NonPagedPool,
        OutBufferSize,
        TAG_OBIO
    );

    if (!OutBuffer)
    {
        DPRINT("ObioInitializeDumpSystem: Out of memory for retrieval buffer\n");
        VeaClose(FileHandle);
        return;
    }

    Status = VeaFsControlFile(
        FileHandle,
        NULL,
        NULL,
        NULL,
        &IoInformation,
        FSCTL_GET_RETRIEVAL_POINTERS,
        &InBuffer,
        sizeof(InBuffer),
        OutBuffer,
        OutBufferSize,
        FALSE 
    );

    if (OutBuffer->ExtentCount > 0)
    {
        LONGLONG ClusterLcn = OutBuffer->Extents[0].Lcn.QuadPart;

        for (i = 0; i < 32; i++)
        {
            ObioDumpLbaBlockMap[i] = (ULONG)(PartitionStartLba + ClusterLcn + i);
        }

        ObioDumpLbaBlockCount = 32;
        ObioDumpSystemArmed = TRUE;
        
        DPRINT("ObioInitializeDumpSystem: Triage Dump ARMED! Start LBA: %u, Sectors: %d\n",
               ObioDumpLbaBlockMap[0], ObioDumpLbaBlockCount);
    }

    // 
    // VeaOS entirely just NT Inspired rather than NT Standard itself. We will actually
    // using IOCTL Code called IOCTL_DISK_GET_DUMP_ROUTINE to get routine to dump
    // to disk
    //

    Status = ObioGetDumpRoutine(FileHandle);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("DEVICE ACTUALLY DOESNT HAVE DUMP ROUTINE\n");
        goto Cleanup;
    }

Cleanup:
    UlFreePoolWithTag(OutBuffer, TAG_OBIO);
    VeaClose(FileHandle);
}

