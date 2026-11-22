/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : sknowndll.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Known DLL caching
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include "session.h"

/* Revision History ------------------------------------------------------
 * DATE       : 17-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file sknowndll.c
 * --------------------------------------------------------------------- */

static const PCHAR SmpFallbackKnownDlls[] = {
    "veaso.so",
    NULL
};

static CHAR SmpKnownDllDiskDir[260] = "/D1P1:/VeaOS/SystemReserved";

static
VOID
SmpBuildPath(
    OUT PCHAR OutBuffer,
    IN ULONG BufferSize,
    IN PCHAR Prefix,
    IN PCHAR Name
)
{
    ULONG Pos = 0;

    if (!OutBuffer || BufferSize == 0) return;

    while (Prefix && *Prefix && Pos < BufferSize - 1)
    {
        OutBuffer[Pos++] = *Prefix++;
    }

    if (Pos > 0 && OutBuffer[Pos - 1] != '/' && Pos < BufferSize - 1)
    {
        OutBuffer[Pos++] = '/';
    }

    while (Name && *Name && Pos < BufferSize - 1)
    {
        OutBuffer[Pos++] = *Name++;
    }

    OutBuffer[Pos] = '\0';
}

static
VEASTATUS
SmpPreloadSingleKnownDll(
    IN PCHAR DllName
)
{
    CHAR DiskPath[260];
    CHAR SectionObjPath[260];
    ANSI_STRING DiskPathStr;
    ANSI_STRING SectionNameStr;
    OBJECT_ATTRIBUTES FileAttr;
    OBJECT_ATTRIBUTES SecAttr;
    OIP_INFORMATION IoStatus;
    HANDLE FileHandle = NULL;
    HANDLE SectionHandle = NULL;
    VEASTATUS Status;

    SmpBuildPath(DiskPath, sizeof(DiskPath), SmpKnownDllDiskDir, DllName);
    SmpBuildPath(SectionObjPath, sizeof(SectionObjPath), "/KnownDLLs", DllName);

    RtlInitAnsiString(&DiskPathStr, DiskPath);
    InitializeObjectAttributes(&FileAttr, &DiskPathStr, 0, NULL, NULL);

    Status = VzCreateFile(
        &FileHandle,
        FILE_READ_DATA | FILE_EXECUTE | VEA_SYNCHRONIZE,
        &FileAttr,
        &IoStatus,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ,
        FILE_OPEN,
        FILE_SYNCHRONOUS_IO_NONALERT,
        NULL,
        0
    );

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS/KnownDLLs] Lewati %s (file tidak ditemukan: 0x%08X)\n",
                 DiskPath, Status);
        return Status;
    }

    RtlInitAnsiString(&SectionNameStr, SectionObjPath);
    InitializeObjectAttributes(
        &SecAttr,
        &SectionNameStr,
        OBJ_PERMANENT | OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
    );

    Status = VzCreateSection(
        &SectionHandle,
        SECTION_ALL_ACCESS,
        &SecAttr,
        NULL,               
        PAGE_EXECUTE_READ,
        SEC_IMAGE,
        FileHandle
    );

    VzClose(FileHandle);

    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS/KnownDLLs] VzCreateSection gagal untuk %s: 0x%08X\n",
                 SectionObjPath, Status);
        return Status;
    }

    VzClose(SectionHandle);

    KdPrintf("[SMSS/KnownDLLs] Cached: %s -> %s\n", DiskPath, SectionObjPath);
    return STATUS_SUCCESS;
}

static
VOID
SmpLoadKnownDllsFromRegistry(
    IN HANDLE KnownDllsKey
)
{
    ANSI_STRING DirValName;
    CHAR DirBuf[260];
    ULONG ValueType = 0;
    ULONG ResultLen = 0;
    VEASTATUS Status;

    /* 1. Baca DllDirectory jika tersedia */
    RtlInitAnsiString(&DirValName, "DllDirectory");
    RtlZeroMemory(DirBuf, sizeof(DirBuf));

    Status = VzQueryValueKey(
        KnownDllsKey,
        &DirValName,
        &ValueType,
        DirBuf,
        sizeof(DirBuf) - 1,
        &ResultLen
    );

    if (VEA_SUCCESS(Status) && ValueType == FILE_KEY_STRING && ResultLen > 0)
    {
        DirBuf[ResultLen] = '\0';
        RtlCopyMemory(SmpKnownDllDiskDir, DirBuf, ResultLen + 1);
    }

    KdPrintf("[SMSS/KnownDLLs] DllDirectory = %s\n", SmpKnownDllDiskDir);

    /* 2. Enumerasi seluruh entry .so di dalam key KnownDLLs
     * Sesuaikan signature VzEnumerateValueKey dengan modul Vk kamu */
    for (ULONG Index = 0; ; Index++)
    {
        CHAR ValName[64];
        CHAR ValData[260];
        ULONG NameLen = sizeof(ValName) - 1;
        ULONG DataLen = sizeof(ValData) - 1;

        RtlZeroMemory(ValName, sizeof(ValName));
        RtlZeroMemory(ValData, sizeof(ValData));

        Status = VzEnumerateValueKey(
            KnownDllsKey,
            Index,
            ValName,
            sizeof(ValName),
            &NameLen,
            &ValueType,
            ValData,
            sizeof(ValData) - 1,
            &DataLen
        );

        if (!VEA_SUCCESS(Status))
        {
            /* STATUS_NO_MORE_ENTRIES (0x8000001A) menandakan seluruh value sudah dibaca */
            break;
        }

        ValName[NameLen] = '\0';
        ValData[DataLen] = '\0';

        /* Lewati konfigurasi direktori agar tidak di-load sebagai .so */
        if (SmpStrEqualI(ValName, "DllDirectory") ||
            SmpStrEqualI(ValName, "DllDirectory32"))
        {
            continue;
        }

        if (ValueType == FILE_KEY_STRING && DataLen > 0)
        {
            SmpPreloadSingleKnownDll(ValData);
        }
    }
}

VEASTATUS
VEAPI
SmpInitializeKnownDlls(VOID)
{
    OBJECT_ATTRIBUTES DirAttr;
    OBJECT_ATTRIBUTES KeyAttr;
    ANSI_STRING DirName;
    ANSI_STRING KeyPath;
    HANDLE KnownDllsDirHandle = NULL;
    HANDLE KnownDllsKeyHandle = NULL;
    VEASTATUS Status;

    /* 1. Buat direktori permanen /KnownDLLs di Object Manager */
    RtlInitAnsiString(&DirName, "/KnownDLLs");
    InitializeObjectAttributes(
        &DirAttr,
        &DirName,
        OBJ_PERMANENT,
        NULL,
        NULL
    );

    Status = VzCreateDirectoryNamespace(&KnownDllsDirHandle, DIRECTORY_ALL_ACCESS, &DirAttr);
    if (!VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS/KnownDLLs] Gagal membuat /KnownDLLs: 0x%08X\n", Status);
        return Status;
    }

    VzClose(KnownDllsDirHandle);
    KdPrintf("[SMSS/KnownDLLs] Direktori /KnownDLLs siap.\n");

    /* 2. Coba baca daftar KnownDLLs dari Registry */
    RtlInitAnsiString(&KeyPath, "/REGISTRY/MACHINE/SYSTEM/CurrentControlSet/Control/SessionManager/KnownDLLs");
    InitializeObjectAttributes(&KeyAttr, &KeyPath, 0, NULL, NULL);

    Status = VzOpenVeaKey(&KnownDllsKeyHandle, KEY_READ, &KeyAttr);
    if (VEA_SUCCESS(Status))
    {
        KdPrintf("[SMSS/KnownDLLs] Membaca daftar dari Registry...\n");
        SmpLoadKnownDllsFromRegistry(KnownDllsKeyHandle);
        VzClose(KnownDllsKeyHandle);
    }
    else
    {
        KdPrintf("[SMSS/KnownDLLs] Key Registry belum ada (0x%08X), pakai daftar fallback.\n", Status);
        for (ULONG i = 0; SmpFallbackKnownDlls[i] != NULL; i++)
        {
            SmpPreloadSingleKnownDll(SmpFallbackKnownDlls[i]);
        }
    }

    return STATUS_SUCCESS;
}

/* EOF */