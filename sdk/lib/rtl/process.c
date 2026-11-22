/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : process.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Creating Process
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <rtl.h>
#include <obtype.h>
#include <vz_wrappers.h>

/* Revision History ------------------------------------------------------
 * DATE       : 08-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file process.c
 * --------------------------------------------------------------------- */

#if defined (BUILDING_VEAKRNL) || defined(BUILD_USER)

VEASTATUS
VEAPI
RtlpMapFile(
    IN PANSI_STRING FileName,
    IN ULONG Attributes,
    OUT HANDLE *Section
)
{
    OBJECT_ATTRIBUTES ObjectAttributes;
    VEASTATUS Status;
    HANDLE hFile = NULL;
    OIP_INFORMATION IoStatusBlock;

    InitializeObjectAttributes(
        &ObjectAttributes,
        FileName,
        Attributes & (OBJ_INHERIT),
        NULL,
        NULL
    );

    Status = VzOpenFile(
        &hFile,
        VEA_SYNCHRONIZE | FILE_EXECUTE | FILE_READ_DATA,
        &ObjectAttributes,
        &IoStatusBlock,
        FILE_SHARE_DELETE | FILE_SHARE_READ,
        FILE_SYNCHRONOUS_IO_ALERT | FILE_NON_DIRECTORY_FILE
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("RtlpMapFile failed: 0x%x\n", Status);
        return Status;
    }

    Status = VzCreateSection(
        Section,
        SECTION_ALL_ACCESS,
        NULL,
        NULL,
        PAGE_EXECUTE,
        SEC_IMAGE,
        hFile
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("ZvCreateSection Failed: 0x%x\n", Status);
        return Status;
    }

    VzClose(hFile);
    return Status;
}

VEASTATUS
VEAPI
RtlpInitEnvironment(
    IN HANDLE ProcessHandle,
    IN PPEB Peb,
    IN OUT PRTL_USER_PROCESS_PARAMETERS ProcessParameters
)
{
    VEASTATUS Status;
    PVOID BaseAddress = NULL;
    SIZE_T EnviroSize;
    SIZE_T Size;
    PCHAR Environment = NULL;
    BOOLEAN WasNormalized = FALSE;

    if (ProcessParameters->Flags & RTL_USER_PROCESS_PARAMETERS_RESERVE_1MB)
    {
        BaseAddress = (PVOID)4;
        EnviroSize = (1024 * 1024) - 256;
        Status = VzAllocateVirtualMemory(
            ProcessHandle,
            &BaseAddress,
            0,
            &EnviroSize,
            MEM_RESERVE,
            PAGE_READWRITE
        );

        if (!VEA_SUCCESS(Status))
        {
            DPRINT("Failed to reserve 1 mb: 0x%x\n", Status);
            return Status;
        }
    }

    if ((Environment = (PCHAR)ProcessParameters->Environment))
    {
        while (*Environment++) while (*Environment++);

        EnviroSize = (ULONG)((ULONG_PTR)Environment -
                             (ULONG_PTR)ProcessParameters->Environment);

        BaseAddress = NULL;
        Size = EnviroSize;
        Status = VzAllocateVirtualMemory(
            ProcessHandle,
            &BaseAddress,
            0,
            &Size,
            MEM_RESERVE | MEM_COMMIT,
            PAGE_READWRITE
        );
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("Failed to allocate Environment Block: 0x%x\n", Status);
            return Status;
        }

        Status = VzWriteVirtualMemory(
            ProcessHandle,
            BaseAddress,
            ProcessParameters->Environment,
            EnviroSize,
            NULL
        );
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("Failed to write Environment Block: 0x%x\n", Status);
            return Status;
        }

        ProcessParameters->Environment = BaseAddress;
    }

    /* Ubah pointer absolut jadi offset relatif SEBELUM disalin ke user-space */
    if (ProcessParameters->Flags & RTL_USER_PROCESS_PARAMETERS_NORMALIZED)
    {
        ULONG_PTR ParamBase = (ULONG_PTR)ProcessParameters;

        if (ProcessParameters->CurrentDirectory.DosPath.Buffer)
            ProcessParameters->CurrentDirectory.DosPath.Buffer = (PCHAR)((ULONG_PTR)ProcessParameters->CurrentDirectory.DosPath.Buffer - ParamBase);

        if (ProcessParameters->DllPath.Buffer)
            ProcessParameters->DllPath.Buffer = (PCHAR)((ULONG_PTR)ProcessParameters->DllPath.Buffer - ParamBase);

        if (ProcessParameters->ImagePathName.Buffer)
            ProcessParameters->ImagePathName.Buffer = (PCHAR)((ULONG_PTR)ProcessParameters->ImagePathName.Buffer - ParamBase);

        if (ProcessParameters->CommandLine.Buffer)
            ProcessParameters->CommandLine.Buffer = (PCHAR)((ULONG_PTR)ProcessParameters->CommandLine.Buffer - ParamBase);

        if (ProcessParameters->WindowTitle.Buffer)
            ProcessParameters->WindowTitle.Buffer = (PCHAR)((ULONG_PTR)ProcessParameters->WindowTitle.Buffer - ParamBase);

        if (ProcessParameters->DesktopInfo.Buffer)
            ProcessParameters->DesktopInfo.Buffer = (PCHAR)((ULONG_PTR)ProcessParameters->DesktopInfo.Buffer - ParamBase);

        if (ProcessParameters->ShellInfo.Buffer)
            ProcessParameters->ShellInfo.Buffer = (PCHAR)((ULONG_PTR)ProcessParameters->ShellInfo.Buffer - ParamBase);

        if (ProcessParameters->RuntimeData.Buffer)
            ProcessParameters->RuntimeData.Buffer = (PCHAR)((ULONG_PTR)ProcessParameters->RuntimeData.Buffer - ParamBase);

        ProcessParameters->Flags &= ~RTL_USER_PROCESS_PARAMETERS_NORMALIZED;
        WasNormalized = TRUE;
    }

    BaseAddress = NULL;
    Size = ProcessParameters->MaximumLength;
    Status = VzAllocateVirtualMemory(
        ProcessHandle,
        &BaseAddress,
        0,
        &Size,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Failed to allocate Parameter Block: 0x%x\n", Status);
        return Status;
    }

    Status = VzWriteVirtualMemory(
        ProcessHandle,
        BaseAddress,
        ProcessParameters,
        ProcessParameters->Length,
        NULL
    );

    /* Balikin lagi pointer milik caller setelah selesai di-copy */
    if (WasNormalized)
    {
        ULONG_PTR ParamBase = (ULONG_PTR)ProcessParameters;

        if (ProcessParameters->CurrentDirectory.DosPath.Buffer)
            ProcessParameters->CurrentDirectory.DosPath.Buffer = (PCHAR)(ParamBase + (ULONG_PTR)ProcessParameters->CurrentDirectory.DosPath.Buffer);

        if (ProcessParameters->DllPath.Buffer)
            ProcessParameters->DllPath.Buffer = (PCHAR)(ParamBase + (ULONG_PTR)ProcessParameters->DllPath.Buffer);

        if (ProcessParameters->ImagePathName.Buffer)
            ProcessParameters->ImagePathName.Buffer = (PCHAR)(ParamBase + (ULONG_PTR)ProcessParameters->ImagePathName.Buffer);

        if (ProcessParameters->CommandLine.Buffer)
            ProcessParameters->CommandLine.Buffer = (PCHAR)(ParamBase + (ULONG_PTR)ProcessParameters->CommandLine.Buffer);

        if (ProcessParameters->WindowTitle.Buffer)
            ProcessParameters->WindowTitle.Buffer = (PCHAR)(ParamBase + (ULONG_PTR)ProcessParameters->WindowTitle.Buffer);

        if (ProcessParameters->DesktopInfo.Buffer)
            ProcessParameters->DesktopInfo.Buffer = (PCHAR)(ParamBase + (ULONG_PTR)ProcessParameters->DesktopInfo.Buffer);

        if (ProcessParameters->ShellInfo.Buffer)
            ProcessParameters->ShellInfo.Buffer = (PCHAR)(ParamBase + (ULONG_PTR)ProcessParameters->ShellInfo.Buffer);

        if (ProcessParameters->RuntimeData.Buffer)
            ProcessParameters->RuntimeData.Buffer = (PCHAR)(ParamBase + (ULONG_PTR)ProcessParameters->RuntimeData.Buffer);

        ProcessParameters->Flags |= RTL_USER_PROCESS_PARAMETERS_NORMALIZED;
    }

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Failed to write Parameter Block to Virtual Memory: 0x%x\n", Status);
        return Status;
    }

    Status = VzWriteVirtualMemory(
        ProcessHandle,
        &Peb->ProcessParameters,
        &BaseAddress,
        sizeof(BaseAddress),
        NULL
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Failed to write pointer to parameter block: 0x%x\n", Status);
        return Status;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
RtlCreateUserProcess(
    IN PANSI_STRING FileImageName,
    IN ULONG Attributes,
    IN OUT PRTL_USER_PROCESS_PARAMETERS ProcessParameters,
    IN PVEA_SECURITY_DESCRIPTOR ProcessSecurityDescriptor OPTIONAL,
    IN PVEA_SECURITY_DESCRIPTOR ThreadSecurityDescriptor OPTIONAL,
    IN HANDLE ParentProcess OPTIONAL,
    IN BOOLEAN InheritHandles,
    IN HANDLE DebugPort OPTIONAL,
    IN HANDLE ExceptionPort OPTIONAL,
    OUT PRTL_USER_PROCESS_INFORMATION ProcessInfo
)
{
    VEASTATUS Status;
    HANDLE hSection;
    OBJECT_ATTRIBUTES ObjAttr;
    PROCESS_BASIC_INFORMATION ProcBasicInfo;

    Status = RtlpMapFile(
        FileImageName,
        Attributes,
        &hSection
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("RtlpMapFile failed: 0x%x\n", Status);
        return Status;
    }

    if (!InheritHandles) ProcessParameters->CurrentDirectory.Handle = NULL;

    if (!ParentProcess) ParentProcess = NtCurrentProcess();

    InitializeObjectAttributes(
        &ObjAttr,
        NULL,
        0,
        NULL,
        ProcessSecurityDescriptor
    );

    Status = VzCreateProcess(
        &ProcessInfo->ProcessHandle,
        PROCESS_ALL_ACCESS,
        &ObjAttr,
        ParentProcess,
        InheritHandles,
        hSection,
        DebugPort,
        ExceptionPort
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Could not create Kernel Process Object: 0x%x\n", Status);
        VzClose(hSection);
        return Status;
    }

    Status = VzQuerySection(
        hSection,
        SectionImageInformation,
        &ProcessInfo->ImageInformation,
        sizeof(SECTION_IMAGE_INFORMATION),
        NULL
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Could not query Section Info: 0x%x\n", Status);
        VzClose(ProcessInfo->ProcessHandle);
        VzClose(hSection);
        return Status;
    }

    Status = VzQueryInformationProcess(
        ProcessInfo->ProcessHandle,
        ProcessBasicInformation,
        &ProcBasicInfo,
        sizeof (ProcBasicInfo),
        NULL
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Failed to query Process: 0x%x\n", Status);
        VzClose(ProcessInfo->ProcessHandle);
        VzClose(hSection);
        return Status;
    }

    Status = STATUS_SUCCESS;
    _VEA_TRY
    {
        if (ProcessParameters->StandardInput)
        {
            Status = VzDuplicateObject(
                ParentProcess,
                ProcessParameters->StandardInput,
                ProcessInfo->ProcessHandle,
                &ProcessParameters->StandardInput,
                0,
                0,
                DUPLICATE_SAME_ACCESS | DUPLICATE_SAME_ATTRIBUTES
            );
            if (!VEA_SUCCESS(Status))
            {
                _VEA_LEAVE;
            }
        }

        if (ProcessParameters->StandardOutput)
        {
            Status = VzDuplicateObject(
                ParentProcess,
                ProcessParameters->StandardOutput,
                ProcessInfo->ProcessHandle,
                &ProcessParameters->StandardOutput,
                0,
                0,
                DUPLICATE_SAME_ACCESS | DUPLICATE_SAME_ATTRIBUTES
            );
            if (!VEA_SUCCESS(Status))
            {
                _VEA_LEAVE;
            }
        }

        if (ProcessParameters->StandardError)
        {
            Status = VzDuplicateObject(
                ParentProcess,
                ProcessParameters->StandardError,
                ProcessInfo->ProcessHandle,
                &ProcessParameters->StandardError,
                0,
                0,
                DUPLICATE_SAME_ACCESS | DUPLICATE_SAME_ATTRIBUTES
            );
            if (!VEA_SUCCESS(Status))
            {
                _VEA_LEAVE;
            }
        }
    }
    _VEA_FINALLY
    {
        if (!VEA_SUCCESS(Status))
        {
            VzClose(ProcessInfo->ProcessHandle);
            VzClose(hSection);
        }
    }
    _VEA_END;

    if (!VEA_SUCCESS(Status)) return Status;

    Status = RtlpInitEnvironment(
        ProcessInfo->ProcessHandle,
        ProcBasicInfo.PebBaseAddress,
        ProcessParameters
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Could not create Process Parameter: 0x%x\n", Status);
        VzClose(ProcessInfo->ProcessHandle);
        VzClose(hSection);
        return Status;
    }

    Status = RtlCreateUserThread(
        ProcessInfo->ProcessHandle,
        ThreadSecurityDescriptor,
        TRUE,
        ProcessInfo->ImageInformation.ZeroBits,
        ProcessInfo->ImageInformation.MaximumStackSize,
        ProcessInfo->ImageInformation.CommittedStackSize,
        ProcessInfo->ImageInformation.TransferAddress,
        ProcBasicInfo.PebBaseAddress,
        &ProcessInfo->ThreadHandle,
        &ProcessInfo->ClientId
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("Can't create Thread: 0x%x\n", Status);
        VzClose(ProcessInfo->ProcessHandle);
        VzClose(hSection);
        return Status;
    }

    VzClose(hSection);
    return STATUS_SUCCESS;
}

#endif