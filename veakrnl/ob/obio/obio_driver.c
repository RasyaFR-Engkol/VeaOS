/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_driver.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Driver
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 14-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_driver.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
ObioGetDriverName(
    IN HANDLE ServiceHandle,
    OUT PCHAR *OutServiceName)
{
    VEASTATUS Status;
    ANSI_STRING KeyPath;
    PCHAR ServiceName;
    PCHAR SubStringStart;

    if (!ServiceHandle || !OutServiceName) return STATUS_INVALID_PARAMETER;

    /* Initialize AnsiString */
    RtlInitAnsiString(&KeyPath, NULL);

    /* To get driver name, get it from key */
    Status = VeaQueryKeyPath(ServiceHandle, &KeyPath);

    /* Status Checking */
    if (Status == STATUS_BUFFER_TOO_SMALL)
    {
        ULONG KeyPathSize;

        /* KeyPathSize is returned via ANSI_STRING */
        KeyPathSize = (ULONG)KeyPath.Length + 1;

        /* Allocate memory for it and update metadata */
        KeyPath.Buffer = (PCHAR)UlAllocatePoolWithTag(
            NonPagedPool,
            KeyPathSize,
            TAG_RTL_STRING
        );
        if (!KeyPath.Buffer)
        {
            goto Cleanup;
        }

        KeyPath.MaximumLength = (USHORT)KeyPathSize;

        /* Retry */
        Status = VeaQueryKeyPath(ServiceHandle, &KeyPath);
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("ERROR: Failed to get key path but size valid: 0x%x\n", Status);
            goto Cleanup;
        }
    }
    else if (!VEA_SUCCESS(Status))
    {
        DPRINT("ERROR: Failed to get key path: 0x%x\n", Status);
        goto Cleanup;
    }

    DPRINT("KeyPath: %Z\n", &KeyPath);

    USHORT charCount = KeyPath.Length;
    LONG lastSlashIndex = -1;

    /* Iterate until '/' or '\\' */
    for (LONG i = (LONG)charCount - 1; i >= 0; i--)
    {
        if (KeyPath.Buffer[i] == '/' || KeyPath.Buffer[i] == '\\')
        {
            lastSlashIndex = i;
            break;
        }
    }

    /* Cut the string */
    if (lastSlashIndex != -1)
    {
        SubStringStart = &KeyPath.Buffer[lastSlashIndex + 1];
    }
    else
    {
        SubStringStart = KeyPath.Buffer;
    }

    SIZE_T NameLen = RtlRawStringLength(SubStringStart);

    /* Allocate memory for ServiceName */
    ServiceName = (PCHAR)UlAllocatePoolWithTag(
        NonPagedPool,
        NameLen + 1,
        TAG_RTL_STRING
    );
    if(!ServiceName)
    {
        DPRINT("ERROR: Failed to allocate memory for ServiceName\n");
        Status = STATUS_INSUFFICIENT_MEMORY;
        goto Cleanup;
    }

    RtlCopyMemory(ServiceName, SubStringStart, NameLen);
    ServiceName[NameLen] = '\0';

    *OutServiceName = ServiceName;

Cleanup:
    if (KeyPath.Buffer)
    {
        UlFreePoolWithTag(KeyPath.Buffer, TAG_RTL_STRING);
    }

    return Status;
}

static
VEASTATUS
ObioDriverMatchesDeviceNode(
    IN HANDLE ServiceHandle,
    IN PDEVICE_NODE DeviceNode,
    OUT PBOOLEAN Matches
)
{
    static const CHAR EnumName[] = "Enum";
    static const CHAR CountName[] = "Count";
    VEASTATUS Status;
    HANDLE EnumHandle = NULL;
    ANSI_STRING ValueName;
    ULONG ValueType;
    ULONG InstanceCount = 0;
    ULONG DataSize;

    if (!ServiceHandle || !DeviceNode || !Matches ||
        !DeviceNode->InstancePath.Buffer ||
        DeviceNode->InstancePath.Length == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *Matches = FALSE;
    RtlInitAnsiString(&ValueName, (PCHAR)EnumName);
    Status = ObioOpenVeaKey(ServiceHandle, &EnumHandle, &ValueName, KEY_READ);
    if (!VEA_SUCCESS(Status))
    {
        return Status == STATUS_OBJECT_NAME_NOT_FOUND ? STATUS_NOT_FOUND : Status;
    }

    RtlInitAnsiString(&ValueName, (PCHAR)CountName);
    Status = VeaQueryValueKey(
        EnumHandle,
        &ValueName,
        &ValueType,
        &InstanceCount,
        sizeof(InstanceCount),
        &DataSize
    );
    if (!VEA_SUCCESS(Status))
    {
        goto Exit;
    }

    if (ValueType != FILE_KEY_DWORD || DataSize != sizeof(InstanceCount))
    {
        Status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    Status = STATUS_NOT_FOUND;
    for (ULONG Index = 0; Index < InstanceCount; Index++)
    {
        CHAR IndexBuffer[12];
        ANSI_STRING InstanceValue;
        PCHAR InstancePathBuffer;
        VEASTATUS ValueStatus;

        RtlStringCbPrintfA(IndexBuffer, sizeof(IndexBuffer), "%u", Index);
        RtlInitAnsiString(&ValueName, IndexBuffer);

        DataSize = 0;
        ValueStatus = ObioGetValueKey(
            EnumHandle,
            &ValueName,
            NULL,
            0,
            NULL,
            &DataSize
        );
        if (ValueStatus != STATUS_BUFFER_TOO_SMALL)
        {
            continue;
        }

        if (DataSize == 0 || DataSize > 0xFFFE)
        {
            continue;
        }

        InstancePathBuffer = UlAllocatePoolWithTag(
            PagedPool,
            DataSize,
            TAG_OBIO
        );
        if (!InstancePathBuffer)
        {
            Status = STATUS_INSUFFICIENT_MEMORY;
            goto Exit;
        }

        ValueStatus = ObioGetValueKey(
            EnumHandle,
            &ValueName,
            InstancePathBuffer,
            DataSize,
            &ValueType,
            &DataSize
        );
        if (!VEA_SUCCESS(ValueStatus))
        {
            UlFreePoolWithTag(InstancePathBuffer, TAG_OBIO);
            continue;
        }
        if (ValueType != FILE_KEY_STRING)
        {
            UlFreePoolWithTag(InstancePathBuffer, TAG_OBIO);
            continue;
        }

        if (DataSize > 0 && InstancePathBuffer[DataSize - 1] == '\0')
        {
            DataSize--;
        }

        InstanceValue.Buffer = InstancePathBuffer;
        InstanceValue.Length = (USHORT)DataSize;
        InstanceValue.MaximumLength = (USHORT)(DataSize + 1);
        if (RtlEqualString(&InstanceValue, &DeviceNode->InstancePath, TRUE))
        {
            *Matches = TRUE;
            Status = STATUS_SUCCESS;
            UlFreePoolWithTag(InstancePathBuffer, TAG_OBIO);
            goto Exit;
        }

        UlFreePoolWithTag(InstancePathBuffer, TAG_OBIO);
    }

Exit:
    VeaClose(EnumHandle);
    return Status;
}

VEASTATUS
VEAPI
ObioCallDriverAddDevice(
    IN PDEVICE_NODE DeviceNode
)
{
    VEASTATUS Status;
    VEASTATUS SearchStatus = STATUS_NOT_FOUND;

    if (!DeviceNode || !DeviceNode->PhysicalDeviceObject)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (!ObioGroupTable || ObioGroupTableCount == 0)
    {
        return STATUS_NOT_FOUND;
    }

    for (ULONG GroupIndex = 0; GroupIndex < ObioGroupTableCount; GroupIndex++)
    {
        PLIST_ENTRY Entry;

        for (Entry = ObioGroupTable[GroupIndex].Flink;
             Entry != &ObioGroupTable[GroupIndex];
             Entry = Entry->Flink)
        {
            POBIO_DRIVER_INFORMATION DriverInformation;
            BOOLEAN Matches;

            DriverInformation = CONTAINING_RECORD(
                Entry,
                OBIO_DRIVER_INFORMATION,
                Link
            );

            if (!DriverInformation->DriverObject ||
                !DriverInformation->ServiceHandle)
            {
                continue;
            }

            Status = ObioDriverMatchesDeviceNode(
                DriverInformation->ServiceHandle,
                DeviceNode,
                &Matches
            );
            if (Status == STATUS_NOT_FOUND)
            {
                continue;
            }
            if (!VEA_SUCCESS(Status))
            {
                SearchStatus = Status;
                DPRINT("Failed to match driver service to %Z: 0x%x\n",
                       &DeviceNode->InstancePath,
                       Status);
                continue;
            }
            if (!Matches)
            {
                continue;
            }

            DPRINT("Calling AddDevice for %Z using driver %p\n",
                   &DeviceNode->InstancePath,
                   DriverInformation->DriverObject);
            return ObioAddDeviceByDeviceObject(
                DeviceNode,
                NULL,
                DriverInformation->DriverObject,
                NULL
            );
        }
    }

    return SearchStatus;
}

VEASTATUS
VEAPI
ObioLoadBootDriver(
    IN PBOOT_DRIVER_LIST_ENTRY DriverEntry,
    IN HANDLE ServiceHandle,
    IN POBIO_DRIVER_INFORMATION DriverInfomation
)
{
    VEASTATUS Status;
    PDRIVER_INITIALIZE DriverEntryRoutine;
    PDRIVER_OBJECT DriverObject = NULL;
    PCHAR ServiceName = NULL;

    DPRINT("ObioLoadBootDriver\n");

    /* Basic validation */
    if (!DriverEntry || !ServiceHandle || !DriverInfomation)
    {
        Status = STATUS_INVALID_PARAMETER;
        goto cleanup;
    }

    /* TODO: Print driver name into the screen if 
       /DEBUG option is preserved */

    /* Get DriverRoutine */
    DriverEntryRoutine = (PDRIVER_INITIALIZE)DriverEntry->EntryPoint;

    /* Get the driver name */
    Status = ObioGetDriverName(ServiceHandle, &ServiceName);
    if (!VEA_SUCCESS(Status))
    {
        goto cleanup;
    }

    DPRINT("Driver will be loaded: %s at 0x%x\n", ServiceName, DriverEntry->ImageBase);

    /* Load the driver */
    Status = ObioCreateDriver(ServiceName, DriverEntryRoutine, &DriverObject);
    if (!VEA_SUCCESS(Status))
    {
        goto cleanup;
    }

    DriverInfomation->DriverObject = DriverObject;

    DPRINT("Driver Loaded: %s at 0x%x\n", ServiceName, DriverEntry->ImageBase);
    
    /*
        In NT, Drivers could have listed the path of any related PDOs
        inside Enum key. And i see it. There were number string, and
        there are some Tree Path
    */
    HANDLE EnumServiceHandle;
    ANSI_STRING EnumString = RTL_CONSTANT_ANSI_STRING("Enum");

    Status = ObioOpenVeaKey(
        ServiceHandle,
        &EnumServiceHandle,
        &EnumString,
        KEY_READ
    );

    if (VEA_SUCCESS(Status))
    {
        ULONG InstanceCount = 0;
        ULONG Type;
        ANSI_STRING AnsiCount = RTL_CONSTANT_ANSI_STRING("Count");

        /* Query Instance value */
        Status = VeaQueryValueKey(
            EnumServiceHandle,
            &AnsiCount,
            &Type,
            &InstanceCount,
            sizeof(ULONG),
            NULL
        );

        for (ULONG i = 0; i < InstanceCount; i++)
        {
            CHAR Number[12];
            ANSI_STRING InstanceString, InstancePath;
            ULONG ResultSize;

            /* The FILE_KEY is REG_SZ type */
            RtlStringCbPrintfA(Number, sizeof(Number), "%u", i);

            /* Init InstanceString */
            RtlInitAnsiString(&InstanceString, Number);

            /* Get data key */
            Status = ObioGetValueKey(
                EnumServiceHandle,
                &InstanceString,
                NULL,
                0,
                NULL,
                &ResultSize
            );

            if (Status == STATUS_BUFFER_TOO_SMALL)
            {
                PCHAR Buffer;
                PDEVICE_OBJECT PDO;

                /* Allocate Buffer */
                Buffer = (PCHAR)UlAllocatePoolWithTag(NonPagedPool, ResultSize, TAG_VEAKEY_BUFFER);
                if (!Buffer) goto cleanup;

                /* Re get */
                Status = ObioGetValueKey(
                    EnumServiceHandle,
                    &InstanceString,
                    Buffer,
                    ResultSize,
                    NULL,
                    &ResultSize
                );

                if (!VEA_SUCCESS(Status))
                {
                    break;
                }

                /* Allocate InstancePath */
                InstancePath.Length = ResultSize;
                InstancePath.MaximumLength = ResultSize;
                InstancePath.Buffer = UlAllocatePoolWithTag(
                    NonPagedPool,
                    ResultSize + 1, /* plus NULL */
                    TAG_OBIO
                );

                if (!InstancePath.Buffer)
                {
                    Status = STATUS_INSUFFICIENT_MEMORY;
                    break;
                }

                /* Copy string to InstancePath */
                RtlCopyMemory(
                    InstancePath.Buffer,
                    Buffer,
                    InstancePath.Length
                );

                /* Free the buffer now */
                UlFreePoolWithTag(Buffer, TAG_VEAKEY_BUFFER);

                InstancePath.Buffer[InstancePath.Length] = '\0';

                PDO = ObioGetDeviceObjectFromDeviceInstance(&InstancePath);
                if (PDO != NULL)
                {
                    DPRINT("DEVICE NODE Found: %Z. Is try to attach\n", &InstancePath);

                    Status = ObioCallDriverAddDevice(PDO->DeviceNode);

                    if (!VEA_SUCCESS(Status))
                    {
                        UlFreePoolWithTag(InstancePath.Buffer, TAG_OBIO);
                        ObDereferenceObject(PDO);
                        goto cleanup;
                    }

                    ObioSetDevNodeState(PDO->DeviceNode, DeviceNodeDriversAdded);
                    ObDereferenceObject(PDO);
                }
                else
                {
                    DPRINT("DEVICE NODE Not found: %Z\n", &InstancePath);
                }

                UlFreePoolWithTag(InstancePath.Buffer, TAG_OBIO);
            }
        }

        VeaClose(EnumServiceHandle);
    }
    else
    {
        /* So there is no Enum, just set STATUS_SUCCESS */
        DPRINT("Driver %s has NO ENUM. Fail with Status = 0x%x\n", ServiceName, Status);
        Status = STATUS_SUCCESS;
    }
    
cleanup:
    if (ServiceName)
    {
        UlFreePoolWithTag(ServiceName, TAG_RTL_STRING);
    }

    if (!VEA_SUCCESS(Status) && DriverObject)
    {
        DriverInfomation->DriverObject = NULL;
        ObDereferenceObject(DriverObject);
    }

    if (!VEA_SUCCESS(Status)) 
        DPRINT("ERR: ObioLoadBootDriver failed: 0x%x\n", Status);

    return Status;
}

BOOLEAN
VEAPI
ObioValidateDriverEntry(PBOOT_DRIVER_LIST_ENTRY DriverEntry)
{
    ULONG_PTR ImageStart, ImageEnd, EntryAddress;

    /* Continue if there is no driver (weird quirk?) */
    if (!DriverEntry)
    {
        return FALSE;
    }

    /* Check flags */
    if (!(DriverEntry->Flags & LDRP_DRIVER_LOADED))
    {
        /* Skip */
        DPRINT("[Obio] Driver image is not loaded\n");
        return FALSE;
    }

    /* make sure ImageBase is OK */
    if(!DriverEntry->ImageBase || DriverEntry->ImageSize == 0)
    {
        DPRINT("[Obio] Invalid driver image metadata\n");
        return FALSE;
    }

    /* Make sure entry point exist */
    if (!DriverEntry->EntryPoint)
    {
        DPRINT(
            "[Obio] Driver has no entry point: %s\n",
            DriverEntry->FilePath.Buffer
        );
        return FALSE;
    }

    /* Calculate image */
    ImageStart = (ULONG_PTR)DriverEntry->ImageBase;
    EntryAddress = (ULONG_PTR)DriverEntry->EntryPoint;
    ImageEnd = ImageStart + DriverEntry->ImageSize;

    /* Check overflow */
    if (ImageEnd < ImageStart)
    {
        DPRINT("[Obio] Driver image range overflow\n");
        return FALSE;
    }

    /* EntryAddress should inside ImageBase */
    if (EntryAddress < ImageStart || EntryAddress >= ImageEnd)
    {
        DPRINT(
            "[Obio] Entry point outside image: %s\n",
            DriverEntry->FilePath.Buffer
        );
        return FALSE;
    }

    return TRUE;
}

/*
    Routine Description:
    Initializing BootDriver from what Bootloader has passed to us.

    Parameter:
    NONE

    Return:
    Boolean - TRUE for success, FALSE for failed
*/
BOOLEAN
VEAPI
ObioInitializeBootDriver(VOID)
{
    PLIST_ENTRY ListEntry, NextEntry;
    PBOOT_DRIVER_LIST_ENTRY BootDriverListEntry;
    USHORT ObioGroupIndex;
    VEASTATUS Status;

    DPRINT("ObioInitializeBootDriver");

    /* TODO: 
       We're now blindly load drivers. we need Group tho so we can load them
       by order. by i don't have implementing that so we try later */

    /* Get highest group order index */
    ObioGroupIndex = ObioGetGroupOrderIndex(NULL);
    if (ObioGroupIndex == 0xFFFF)
    {
        /* Bugcheck PNP because it's part of PNP that is broken */
        KsBugCheck(PNP_DETECTED_FATAL_ERROR);
    }

    /* Save Table count */
    ObioGroupTableCount = ObioGroupOrderCount;

    /* Allocate table for it */
    ObioGroupTable = UlAllocatePoolWithTag(
        PagedPool,
        ObioGroupIndex * sizeof(LIST_ENTRY),
        OBIO_TAG
    );
    if (!ObioGroupTable)
    {
        /* Bugcheck IO1 because GroupTable is failed to allocate */
        KsBugCheck(IO1_INITIALIZATION_FAILED);
    }

    /* Allocate every part of table Group */
    for (ULONG i = 0; i < ObioGroupIndex; i++)
    {
        InitializeListHead(&ObioGroupTable[i]);
    }

    /* BootDriverListEntry is in KsLoaderBlock */
    ListEntry = &KsLoaderBlock->BootDriverList; 
    for (NextEntry = ListEntry->Flink; 
            NextEntry != ListEntry; 
            NextEntry = NextEntry->Flink
        )
    {
        HANDLE ServiceHandle;
        POBIO_DRIVER_INFORMATION DriverInformation;
        USHORT DriverGroupIndex;

        BootDriverListEntry = CONTAINING_RECORD(NextEntry, BOOT_DRIVER_LIST_ENTRY, Link);

        /* Validate Driver */
        if (!ObioValidateDriverEntry(BootDriverListEntry))
        {
            continue;
        }
        
        /* Try open key */
        Status = ObioOpenVeaKey(
            NULL,
            &ServiceHandle,
            &BootDriverListEntry->RegistryPath,
            KEY_READ
        );

        if(!VEA_SUCCESS(Status))
        {
            /* Failed */
            DPRINT(
                "[Obio] Failed to open service key %s: 0x%x\n",
                BootDriverListEntry->RegistryPath.Buffer,
                Status
            );

            continue;
        }

        /* Get the driver group index */
        DriverGroupIndex = ObioGetGroupOrderIndex(ServiceHandle);

        /* Make sure DriverGroupIndex is valid */
        if (DriverGroupIndex == 0xFFFF ||
            DriverGroupIndex >= ObioGroupTableCount)
        {
            /* Invalid Group */
            DPRINT(
                "[Obio] Invalid group index for driver %s: %u\n",
                BootDriverListEntry->FilePath.Buffer,
                DriverGroupIndex
            );

            VeaClose(ServiceHandle);
            continue;
        }

        DPRINT("Driver %s Group Index: %d\n", BootDriverListEntry->FilePath.Buffer, DriverGroupIndex);

        /* Allocate Driver Information */
        DriverInformation = UlAllocatePoolZero(
            PagedPool,
            sizeof(OBIO_DRIVER_INFORMATION),
            OBIO_TAG
        );

        if (!DriverInformation)
        {
            /* Allocation Failed */
            DPRINT(
                "[Obio] Failed to allocate driver metadata for %s\n",
                BootDriverListEntry->FilePath.Buffer
            );

            VeaClose(ServiceHandle);
            continue;
        }

        /* Initialize it's list head*/
        InitializeListHead(&DriverInformation->Link);

        /* Insert our DriverInformation */
        DriverInformation->BootEntry = BootDriverListEntry;
        DriverInformation->ServiceHandle = ServiceHandle;
        DriverInformation->DriverObject = NULL;
        DriverInformation->GroupIndex = DriverGroupIndex;

        /* Insert DriverInfomation to GroupTable */
        InsertTailList(
            &ObioGroupTable[DriverGroupIndex],
            &DriverInformation->Link
        );

    }

    /* Initialize drivers in ServiceGroupOrder order. */
    for (ULONG GroupIndex = 0;
         GroupIndex < ObioGroupTableCount;
         GroupIndex++)
    {
        PLIST_ENTRY CurrentEntry;

        for (CurrentEntry = ObioGroupTable[GroupIndex].Flink;
             CurrentEntry != &ObioGroupTable[GroupIndex];
             CurrentEntry = CurrentEntry->Flink)
        {
            POBIO_DRIVER_INFORMATION DriverInformation;

            DriverInformation = CONTAINING_RECORD(
                CurrentEntry,
                OBIO_DRIVER_INFORMATION,
                Link
            );

            Status = ObioLoadBootDriver(
                DriverInformation->BootEntry,
                DriverInformation->ServiceHandle,
                DriverInformation
            );

            if (!VEA_SUCCESS(Status))
            {
                DPRINT(
                    "[Obio] Driver in group %u failed: 0x%x\n",
                    GroupIndex,
                    Status
                );
            }
            else
            {
                ObioQueueDeviceAction(
                    ObioRootDeviceNode->PhysicalDeviceObject,
                    PiActionEnumRootDevices,
                    NULL,
                    NULL
                );
            }
        }
    }

    return TRUE;
}

VEASTATUS
VEAPI
ObioAssignDeviceResources(IN PDEVICE_NODE DeviceNode)
{
    /* NOTE: Let FDO ask the Resouces. */
    /* Modern hardware doesn't have Resource conflict */
    ObioSetDevNodeState(DeviceNode, DeviceNodeResourcesAssigned);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObioReferenceDeviceObjectByName(
    IN PANSI_STRING SymbolicLinkName,
    OUT PDEVICE_OBJECT *DeviceObject
)
{
    VEASTATUS Status;
    PDEVICE_OBJECT Do;
    OBJECT_ATTRIBUTES ObjAttr;

    InitializeObjectAttributes(&ObjAttr, SymbolicLinkName, 0, NULL, NULL);

    Status = ObReferenceObjectByName(
        &ObjAttr,
        ObpDeviceObjectType,
        (PVOID*)&Do
    );
    if (!VEA_SUCCESS(Status))
    {
        return Status;
    }

    if (DeviceObject)
    {
        *DeviceObject = Do;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObioNormalizeImagePath(
    IN OUT PANSI_STRING ImagePath,
    OUT PANSI_STRING ServiceName OPTIONAL
)
{
    PANSI_STRING OldString;
    PCHAR OldBuffer;
    PCHAR NewBuffer;
    USHORT NewLength;
    USHORT RemainingLen;
    BOOLEAN HasSystemRoot = FALSE;

    UNREFERENCED_PARAMETER(ServiceName);

    OldBuffer = ImagePath->Buffer;

    if (ImagePath->Length >= SYSTEMROOT_VAR_LEN)
    {
        if (RtlRawStringCompareN(OldBuffer, SYSTEMROOT_VAR, SYSTEMROOT_VAR_LEN) == 0)
        {
            HasSystemRoot = TRUE;
        }
    }

    if (HasSystemRoot)
    {
        RemainingLen = ImagePath->Length - SYSTEMROOT_VAR_LEN;
        NewLength = SYSTEMROOT_TGT_LEN + RemainingLen;

        NewBuffer = UlAllocatePoolZero(NonPagedPool, NewLength + 1, TAG_OBIO);
        if (!NewBuffer)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlCopyMemory(NewBuffer, SYSTEMROOT_TARGET, SYSTEMROOT_TGT_LEN);

        if (RemainingLen > 0)
        {
            RtlCopyMemory(NewBuffer + SYSTEMROOT_TGT_LEN, 
                          OldBuffer + SYSTEMROOT_VAR_LEN, 
                          RemainingLen);
        }

        NewBuffer[NewLength] = '\0';

        UlFreePool(OldBuffer);
        ImagePath->Buffer = NewBuffer;
        ImagePath->Length = NewLength;
        ImagePath->MaximumLength = NewLength + 1;
    }
    else
    {
        if (ImagePath->MaximumLength > ImagePath->Length)
        {
            ImagePath->Buffer[ImagePath->Length] = '\0';
        }
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObioLoadDriver(
    IN HANDLE ServiceHandle,
    OUT PDRIVER_OBJECT *DriverObject
)
{
    ANSI_STRING ImagePath;
    VEASTATUS Status;
    PVOID BaseAddress;
    ULONG ResultSizeOut;
    ANSI_STRING ImagePathPrefix = RTL_CONSTANT_ANSI_STRING("ImagePath");

    Status = ObioGetValueKey(ServiceHandle, &ImagePathPrefix, NULL, 0, NULL, &ResultSizeOut);
    if (Status == STATUS_BUFFER_TOO_SMALL)
    {
        ImagePath.Length = ResultSizeOut;
        ImagePath.MaximumLength = ResultSizeOut + 1;
        ImagePath.Buffer = UlAllocatePoolZero(NonPagedPool, ResultSizeOut + 1, TAG_OBIO);
        if (!ImagePath.Buffer)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Status = ObioGetValueKey(
            ServiceHandle,
            &ImagePathPrefix,
            ImagePath.Buffer,
            ImagePath.Length,
            NULL,
            NULL
        );

        if (!VEA_SUCCESS(Status))
        {
            DPRINT("[OBIO] Failed: Failed to get ImagePath value key\n");
            UlFreePool(ImagePath.Buffer);
            return STATUS_UNSUCCESSFUL;
        }
    }
    else
    {
        DPRINT("[OBIO] Failed: Failed to get ImagePath size value key\n");
        return STATUS_UNSUCCESSFUL;
    }

    Status = ObioNormalizeImagePath(&ImagePath, NULL);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[OBIO] Failed: Failed to normalize image path: 0x%x\n", Status);
        return Status;
    }

    DPRINT("Normalized Image Path: %Z\n", &ImagePath);

    // Status = MmLoadSystemImage(&ImagePath, NULL, NULL, 0, NULL, &BaseAddress);
    UlFreePool(ImagePath.Buffer);
    
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[OBIO] Failed: MmLoadSystemImage failed: 0x%x\n", Status);
        return Status;
    }
}

static
VOID
VEAPI
ObioDoLoadUnloadDriverWorker(
    IN PVOID Parameter
)
{
    PLOAD_UNLOAD_PARAMS LoadParams = Parameter;

    if (LoadParams->DriverObject)
    {
        LoadParams->DriverObject->RemoveDeviceRoutine(LoadParams->DriverObject);
        LoadParams->Status = STATUS_SUCCESS;
    }
    else
    {
        HANDLE Handle;
        VEASTATUS Status;

        Status = ObioOpenVeaKey(NULL, &Handle, LoadParams->RegistryPath, KEY_READ);
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("ObioLoadDriver failed: 0x%x\n", Status);
            LoadParams->Status = Status;
        }
        else
        {
            LoadParams->Status = ObioLoadDriver(Handle, &LoadParams->DriverObject);
            VeaClose(Handle);
        }
    }

    if (LoadParams->SetEvent)
    {
        KsSetEvent(&LoadParams->Event, FALSE);
    }
}

VEASTATUS
VEAPI
ObioDoLoadUnloadDriver(
    IN PANSI_STRING RegistryPath,
    OUT PDRIVER_OBJECT *DriverObject
)
{
    LOAD_UNLOAD_PARAMS LoadParams;

    LoadParams.RegistryPath = RegistryPath;
    LoadParams.DriverObject = *DriverObject;

    if ((PEPROCESS)PtGetCurrentProcess() != PtSystemProcess)
    {
        LoadParams.SetEvent = TRUE;
        KsInitializeEvent(&LoadParams.Event, NotificationEvent, FALSE);

        UlInitializeWorkItem(&LoadParams.WorkItem, ObioDoLoadUnloadDriverWorker, &LoadParams);
        UlQueueWorkItem(&LoadParams.WorkItem, DelayedWorkQueue);

        KsWaitForSingleObject(&LoadParams.Event);
    }
    else
    {
        LoadParams.SetEvent = FALSE;
        ObioDoLoadUnloadDriverWorker(&LoadParams);
    }

    return LoadParams.Status;
}

VEASTATUS
VEAPI
VeaLoadDriver(IN PANSI_STRING DriverServiceName)
{
    KPROCESSOR_MODE PreviousMode;
    PDRIVER_OBJECT DriverObject;
    VEASTATUS Status;

    UNREFERENCED_PARAMETER(PreviousMode);

    DriverObject = NULL;
    Status = ObioDoLoadUnloadDriver(DriverServiceName, &DriverObject);
}

/* EOF */