/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : sdir.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Session Directory Handling
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "session.h"

/* Revision History ------------------------------------------------------
 * DATE       : 16-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file sdir.c
 * --------------------------------------------------------------------- */

HANDLE SmpSessionsDirHandle = NULL;
HANDLE SmpDosDevicesDirHandle = NULL;

static
VEASTATUS
SmpCreateObjectDirectory(
    IN PCHAR DirectoryPath,
    OUT PHANDLE PersistentHandle
)
{
    OBJECT_ATTRIBUTES ObjAttr;
    ANSI_STRING DirName;
    HANDLE DirHandle = NULL;
    VEASTATUS Status;

    RtlInitAnsiString(&DirName, DirectoryPath);

    InitializeObjectAttributes(
        &ObjAttr,
        &DirName,
        OBJ_PERMANENT | OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
    );

    Status = VzCreateDirectoryNamespace(&DirHandle, DIRECTORY_ALL_ACCESS, &ObjAttr);
    if (VEA_SUCCESS(Status))
    {
        /* Karena sudah OBJ_PERMANENT, aman untuk langsung ditutup */
        VzClose(DirHandle);
        return STATUS_SUCCESS;
    }

    // HACK
    // Fallback kalo OBJ_PERMANENT failed
    InitializeObjectAttributes(
        &ObjAttr,
        &DirName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
    );

    Status = VzCreateDirectoryNamespace(&DirHandle, DIRECTORY_ALL_ACCESS, &ObjAttr);
    if (VEA_SUCCESS(Status))
    {
        *PersistentHandle = DirHandle;
    }

    return Status;
}

VEASTATUS
VEAPI
SmpInitializeNamespace(VOID)
{
    VEASTATUS Status;
    OBJECT_ATTRIBUTES ObjAttr;
    ANSI_STRING BootDirPath;
    OIP_INFORMATION StatusInfo;
    HANDLE BootDirHandle = NULL;

    Status = SmpCreateObjectDirectory("/Sessions", &SmpSessionsDirHandle);
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] Gagal membuat /Sessions: 0x%08X\n", Status);
        return Status;
    }
    KdPrintf("[SMSS] Direktori /Sessions siap.\n");

    Status = SmpCreateObjectDirectory("/DosDevices", &SmpDosDevicesDirHandle);
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] Gagal membuat /DosDevices: 0x%08X\n", Status);
        return Status;
    }
    KdPrintf("[SMSS] Direktori /DosDevices siap.\n");

    RtlInitAnsiString(&BootDirPath, "/D1P1:/VeaOS");
    InitializeObjectAttributes(&ObjAttr, &BootDirPath, OBJ_CASE_INSENSITIVE, NULL, NULL);

    Status = VzCreateFile(
        &BootDirHandle,
        FILE_LIST_DIRECTORY | VEA_SYNCHRONIZE,
        &ObjAttr,
        &StatusInfo,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_OPEN,
        FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
        NULL,
        0
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS] FATAL: Gagal akses /D1P1:/VeaOS: 0x%08X\n", Status);
        return Status;
    }

    VzClose(BootDirHandle);
    KdPrintf("[SMSS] Akses /D1P1:/VeaOS terverifikasi!\n");

    return STATUS_SUCCESS;
}