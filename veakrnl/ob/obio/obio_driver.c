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

/* STRUCT */
typedef enum _ADD_DEV_DRIVER_TYPE
{
    LowerFilter,
    LowerClassFilter,
    DeviceDriver,
    UpperFilter,
    UpperClassFilter
} ADD_DEV_DRIVER_TYPE;

typedef struct _ADD_DEV_DRIVERS_LIST
{
    LIST_ENTRY ListEntry;
    PDRIVER_OBJECT DriverObject;
    ADD_DEV_DRIVER_TYPE DriverType;
} ADD_DEV_DRIVERS_LIST, *PADD_DEV_DRIVERS_LIST;

typedef struct _ATTACH_FILTER_DRIVERS_CONTEXT
{
    ADD_DEV_DRIVER_TYPE DriverType;
    PDEVICE_NODE DeviceNode;
    PLIST_ENTRY DriversListHead;
} ATTACH_FILTER_DRIVERS_CONTEXT, *PATTACH_FILTER_DRIVERS_CONTEXT;

/* FUNCTION */

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
        Status = STATUS_INSUFFICIENT_RESOURCES;
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

#define OBIO_MATCH_VERBOSE 1

#if OBIO_MATCH_VERBOSE
#define MTRACE(...) DPRINT(__VA_ARGS__)
#else
#define MTRACE(...) ((void)0)
#endif

static UCHAR
ObiopUpAscii(UCHAR c)
{
    return (c >= 'a' && c <= 'z') ? (UCHAR)(c - 'a' + 'A') : c;
}

static VOID
ObiopDumpMismatch(
    IN PANSI_STRING EnumValue,
    IN PANSI_STRING NodePath
)
{
    USHORT Common = EnumValue->Length < NodePath->Length ?
                    EnumValue->Length : NodePath->Length;
    USHORT i;

    for (i = 0; i < Common; i++)
    {
        if (ObiopUpAscii((UCHAR)EnumValue->Buffer[i]) !=
            ObiopUpAscii((UCHAR)NodePath->Buffer[i]))
        {
            break;
        }
    }

    DPRINT("[MATCH]   mismatch: enumLen=%u nodeLen=%u firstDiff=%u "
           "enumCh=0x%x nodeCh=0x%x\n",
           (ULONG)EnumValue->Length,
           (ULONG)NodePath->Length,
           (ULONG)i,
           i < EnumValue->Length ? (ULONG)(UCHAR)EnumValue->Buffer[i] : 0xFFFF,
           i < NodePath->Length  ? (ULONG)(UCHAR)NodePath->Buffer[i]  : 0xFFFF);
}


static
VEASTATUS
ObioDriverMatchesDeviceNode(
    IN HANDLE ServiceHandle,
    IN PDEVICE_NODE DeviceNode,
    OUT PBOOLEAN Matches
)
{
    static const CHAR EnumName[]  = "Enum";
    static const CHAR CountName[] = "Count";
    VEASTATUS Status;
    HANDLE EnumHandle = NULL;
    ANSI_STRING ValueName;
    ULONG ValueType = 0;
    ULONG InstanceCount = 0;
    ULONG DataSize = 0;

    /* statistik untuk ringkasan */
    ULONG Scanned = 0;
    ULONG SkipQuery = 0;      /* query size != BUFFER_TOO_SMALL */
    ULONG SkipSize = 0;       /* DataSize == 0 atau > 0xFFFE */
    ULONG SkipRead = 0;      /* query kedua gagal */
    ULONG SkipType = 0;       /* tipe bukan FILE_KEY_STRING */
    ULONG Mismatch = 0;
    VEASTATUS FirstBadStatus = STATUS_SUCCESS;
    ULONG FirstBadIndex = 0xFFFFFFFF;

    if (!ServiceHandle || !DeviceNode || !Matches ||
        !DeviceNode->InstancePath.Buffer ||
        DeviceNode->InstancePath.Length == 0)
    {
        DPRINT("[MATCH] invalid param: svc=%p node=%p matches=%p\n",
               ServiceHandle, DeviceNode, Matches);
        return STATUS_INVALID_PARAMETER;
    }

    *Matches = FALSE;

    RtlInitAnsiString(&ValueName, (PCHAR)EnumName);
    Status = ObioOpenVeaKey(ServiceHandle, &EnumHandle, &ValueName, KEY_READ);
    if (!VEA_SUCCESS(Status))
    {
        /* tampilkan status MENTAH sebelum dipetakan ke NOT_FOUND */
        DPRINT("[MATCH] svc=%p: open 'Enum' failed raw=0x%x node=%Z\n",
               ServiceHandle, Status, &DeviceNode->InstancePath);
        return Status == STATUS_OBJECT_NAME_NOT_FOUND ? STATUS_NOT_FOUND : Status;
    }

    RtlInitAnsiString(&ValueName, (PCHAR)CountName);
    DataSize = 0;
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
        DPRINT("[MATCH] svc=%p: query 'Count' failed st=0x%x\n",
               ServiceHandle, Status);
        goto Exit;
    }

    if (ValueType != FILE_KEY_DWORD || DataSize != sizeof(InstanceCount))
    {
        DPRINT("[MATCH] svc=%p: 'Count' bad type=%u (want %u) size=%u (want %u)\n",
               ServiceHandle, ValueType, (ULONG)FILE_KEY_DWORD,
               DataSize, (ULONG)sizeof(InstanceCount));
        Status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    MTRACE("[MATCH] svc=%p node=%Z Count=%u\n",
           ServiceHandle, &DeviceNode->InstancePath, InstanceCount);

    Status = STATUS_NOT_FOUND;
    for (ULONG Index = 0; Index < InstanceCount; Index++)
    {
        CHAR IndexBuffer[12];
        ANSI_STRING InstanceValue;
        PCHAR InstancePathBuffer;
        VEASTATUS ValueStatus;

        Scanned++;

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
            SkipQuery++;
            if (FirstBadIndex == 0xFFFFFFFF)
            {
                FirstBadIndex = Index;
                FirstBadStatus = ValueStatus;
            }
            MTRACE("[MATCH]   [%u] size query st=0x%x (want 0x%x) size=%u\n",
                   Index, ValueStatus, (ULONG)STATUS_BUFFER_TOO_SMALL, DataSize);
            continue;
        }

        if (DataSize == 0 || DataSize > 0xFFFE)
        {
            SkipSize++;
            MTRACE("[MATCH]   [%u] bad size=%u\n", Index, DataSize);
            continue;
        }

        /* +1 untuk terminator supaya MaximumLength valid */
        InstancePathBuffer = UlAllocatePoolWithTag(
            PagedPool,
            DataSize + 1,
            TAG_OBIO
        );
        if (!InstancePathBuffer)
        {
            DPRINT("[MATCH] out of memory (size=%u)\n", DataSize + 1);
            Status = STATUS_INSUFFICIENT_RESOURCES;
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
            SkipRead++;
            if (FirstBadIndex == 0xFFFFFFFF)
            {
                FirstBadIndex = Index;
                FirstBadStatus = ValueStatus;
            }
            MTRACE("[MATCH]   [%u] read failed st=0x%x\n", Index, ValueStatus);
            UlFreePoolWithTag(InstancePathBuffer, TAG_OBIO);
            continue;
        }

        if (ValueType != FILE_KEY_STRING)
        {
            SkipType++;
            MTRACE("[MATCH]   [%u] bad type=%u (want %u)\n",
                   Index, ValueType, (ULONG)FILE_KEY_STRING);
            UlFreePoolWithTag(InstancePathBuffer, TAG_OBIO);
            continue;
        }

        InstancePathBuffer[DataSize] = '\0';
        if (DataSize > 0 && InstancePathBuffer[DataSize - 1] == '\0')
        {
            DataSize--;
        }

        InstanceValue.Buffer = InstancePathBuffer;
        InstanceValue.Length = (USHORT)DataSize;
        InstanceValue.MaximumLength = (USHORT)(DataSize + 1);

        if (RtlEqualString(&InstanceValue, &DeviceNode->InstancePath, TRUE))
        {
            DPRINT("[MATCH] svc=%p MATCH at [%u]: %Z\n",
                   ServiceHandle, Index, &InstanceValue);
            *Matches = TRUE;
            Status = STATUS_SUCCESS;
            UlFreePoolWithTag(InstancePathBuffer, TAG_OBIO);
            goto Exit;
        }

        Mismatch++;
#if OBIO_MATCH_VERBOSE
        DPRINT("[MATCH]   [%u] enum='%Z' node='%Z'\n",
               Index, &InstanceValue, &DeviceNode->InstancePath);
        ObiopDumpMismatch(&InstanceValue, &DeviceNode->InstancePath);
#endif
        UlFreePoolWithTag(InstancePathBuffer, TAG_OBIO);
    }

    /* tidak ada yang cocok: ringkasan kenapa */
    DPRINT("[MATCH] svc=%p NO MATCH node=%Z | count=%u scanned=%u "
           "mismatch=%u skipQuery=%u skipSize=%u skipRead=%u skipType=%u "
           "firstBad=[%u]/0x%x\n",
           ServiceHandle, &DeviceNode->InstancePath,
           InstanceCount, Scanned, Mismatch,
           SkipQuery, SkipSize, SkipRead, SkipType,
           FirstBadIndex, FirstBadStatus);

Exit:
    VeaClose(EnumHandle);
    return Status;
}

VEASTATUS
VEAPI
ObioCallDriverAddDevice(
    IN PDEVICE_NODE DeviceNode,
    IN BOOLEAN MustLoadDriver
)
{
    VEASTATUS Status;
    HANDLE EnumRootKey, SubKey;
    HANDLE ClassKey = NULL;
    ANSI_STRING EnumRoot = RTL_CONSTANT_ANSI_STRING("/REGISTRY/MACHINE/SYSTEM/CurrentControlSet/Enum");
    LIST_ENTRY DriverListHead;
    ATTACH_FILTER_DRIVERS_CONTEXT RoutineContext;
    PLIST_ENTRY listEntry;
    ANSI_STRING ServiceValueName = RTL_CONSTANT_ANSI_STRING("Service");
    ULONG ValueType = 0;
    ULONG ResultSize = 0;
    PCHAR ServiceNameBuffer = NULL;

    /* Open ENUM Key */
    Status = ObioOpenVeaKey(
        NULL,
        &EnumRootKey,
        &EnumRoot,
        KEY_READ
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[OBIO] ObioOpenVeaKey failed for ENUM root ('%Z'): 0x%08x\n", &EnumRoot, Status);
        return Status;
    }

    /* Open Instance SubKey */
    Status = ObioOpenVeaKey(
        EnumRootKey,
        &SubKey,
        &DeviceNode->InstancePath,
        KEY_READ
    );
    VeaClose(EnumRootKey);

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[OBIO] ObioOpenVeaKey failed for InstancePath '%Z': 0x%08x\n", 
               &DeviceNode->InstancePath, Status);
        return Status;
    }

    /* TODO: Handle Class GUID */

    InitializeListHead(&DriverListHead);

    /* TODO: Implement this 
    
    Status = PiAttachFilterDrivers(&drvListHead, DeviceNode, SubKey, ClassKey, TRUE, LoadDrivers);
    if (!NT_SUCCESS(Status))
    {
        goto Cleanup;
    }

    */

    RoutineContext.DriversListHead = &DriverListHead;
    RoutineContext.DriverType = DeviceDriver;
    RoutineContext.DeviceNode = DeviceNode;

    Status = VeaQueryValueKey(
        SubKey,
        &ServiceValueName,
        &ValueType,
        NULL,
        0,
        &ResultSize
    );

    if (Status == STATUS_BUFFER_TOO_SMALL)
    {
        /* 2. Alokasikan buffer + 1 byte buat Null-Terminator safety */
        ServiceNameBuffer = (PCHAR)UlAllocatePoolZero(NonPagedPool, ResultSize + 1, 'vRcS');
        if (!ServiceNameBuffer)
        {
            DPRINT("[OBIO] Out of memory allocating %u bytes for Service name!\n", ResultSize + 1);
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Cleanup;
        }

        /* 3. Query kedua: Ambil data stringnya secara utuh */
        Status = VeaQueryValueKey(
            SubKey,
            &ServiceValueName,
            &ValueType,
            ServiceNameBuffer,
            ResultSize,
            NULL
        );
    }

    if (VEA_SUCCESS(Status))
    {
        PDRIVER_OBJECT DriverObject = NULL;
        CHAR DriverObjPathBuf[256];
        ANSI_STRING DriverObjPath;
        OBJECT_ATTRIBUTES DriverObjAttr;
        HANDLE DriverHandle = NULL;

        RtlZeroMemory(DriverObjPathBuf, sizeof(DriverObjPathBuf));
        RtlStringCbPrintfA(DriverObjPathBuf, sizeof(DriverObjPathBuf), "/Driver/%s", ServiceNameBuffer);
        RtlInitAnsiString(&DriverObjPath, DriverObjPathBuf);

        InitializeObjectAttributes(&DriverObjAttr, &DriverObjPath, 0, NULL, NULL);

        Status = ObOpenObjectByName(
            &DriverObjAttr,
            ObpDriverObjectType,
            KernelMode,
            0,
            NULL,
            &DriverHandle
        );

        if (VEA_SUCCESS(Status))
        {
            /* Driver SUDAH LOADED di RAM! Ambil reference-nya */
            Status = ObReferenceObjectByHandle(
                DriverHandle,
                0,
                ObpDriverObjectType,
                KernelMode,
                (PVOID*)&DriverObject
            );
            VeaClose(DriverHandle);
        }
        else
        {
            /* B. Driver BELUM ADA di RAM */
            if (MustLoadDriver)
            {
                CHAR ServiceKeyPathBuf[256];
                ANSI_STRING ServiceKeyPath;
                HANDLE ServiceKeyHandle = NULL;

                RtlZeroMemory(ServiceKeyPathBuf, sizeof(ServiceKeyPathBuf));
                RtlStringCbPrintfA(ServiceKeyPathBuf, sizeof(ServiceKeyPathBuf),
                                   "/REGISTRY/MACHINE/SYSTEM/CurrentControlSet/Services/%s",
                                   ServiceNameBuffer);
                RtlInitAnsiString(&ServiceKeyPath, ServiceKeyPathBuf);

                Status = ObioOpenVeaKey(NULL, &ServiceKeyHandle, &ServiceKeyPath, KEY_READ);
                if (VEA_SUCCESS(Status))
                {
                    Status = ObioLoadDriver(ServiceKeyHandle, &DriverObject);
                    VeaClose(ServiceKeyHandle);
                }
            }
            else
            {
                DPRINT("[OBIO] Service '%s' will not be loaded now (MustLoadDriver=FALSE)\n", ServiceNameBuffer);
                Status = STATUS_NOT_FOUND;
                goto Cleanup;
            }
        }

        if (VEA_SUCCESS(Status) && DriverObject)
        {
            PADD_DEV_DRIVERS_LIST DriverEntry = (PADD_DEV_DRIVERS_LIST)UlAllocatePoolWithTag(
                PagedPool,
                sizeof(ADD_DEV_DRIVERS_LIST),
                TAG_OBIO
            );

            if (DriverEntry)
            {
                RtlZeroMemory(DriverEntry, sizeof(ADD_DEV_DRIVERS_LIST));
                DriverEntry->DriverType = DeviceDriver;
                DriverEntry->DriverObject = DriverObject;

                InsertTailList(&DriverListHead, &DriverEntry->ListEntry);
            }
            else
            {
                ObDereferenceObject(DriverObject);
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto Cleanup;
            }
        }
        else
        {
            DPRINT("[OBIO] Failed to obtain DriverObject for '%s': 0x%08x\n", ServiceNameBuffer, Status);
            goto Cleanup;
        }
    }
    else if (Status == STATUS_OBJECT_NAME_NOT_FOUND && 
            (DeviceNode->CapabilityFlags & 0x00000040)) // DEVCAP_RAWDEVICEOK
    {
        PADD_DEV_DRIVERS_LIST DriverEntry = UlAllocatePoolWithTag(
            PagedPool,
            sizeof(*DriverEntry),
            TAG_OBIO
        );
        if (DriverEntry)
        {
            RtlZeroMemory(DriverEntry, sizeof(*DriverEntry));
            DriverEntry->DriverType = DeviceDriver;
            DriverEntry->DriverObject = NULL;
            InsertTailList(&DriverListHead, &DriverEntry->ListEntry);
        }
        else
        {
            DPRINT("[OBIO] Out of memory allocating raw ADD_DEV_DRIVERS_LIST!\n");
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Cleanup;
        }
    }
    else
    {
        if (Status == STATUS_OBJECT_TYPE_MISMATCH && !(DeviceNode->Flags & DNF_PROBLEM))
        {
            DPRINT("[OBIO] Service value type mismatch for '%Z'. Setting DN_PROB_REGISTRY.\n", 
                   &DeviceNode->InstancePath);
            ObioSetDevNodeProblem(DeviceNode, DN_PROB_REGISTRY);
        }
        DPRINT("[OBIO] FAILED: No valid service for '%Z' (Status: 0x%08x, MustLoadDriver: %u)\n", 
               &DeviceNode->InstancePath, Status, MustLoadDriver);
        goto Cleanup;
    }

    /* TODO: Implement this:
    
    // upper (class) filters
    Status = PiAttachFilterDrivers(&drvListHead, DeviceNode, SubKey, ClassKey, FALSE, LoadDrivers);
    if (!NT_SUCCESS(Status))
    {
        goto Cleanup;
    }
        
    */

    for (listEntry = DriverListHead.Flink;
         listEntry != &DriverListHead;
         listEntry = listEntry->Flink)
    {
        PADD_DEV_DRIVERS_LIST DriverEntry;
        PDRIVER_OBJECT DriverObject;

        DriverEntry = CONTAINING_RECORD(listEntry, ADD_DEV_DRIVERS_LIST, ListEntry);
        DriverObject = DriverEntry->DriverObject;

        if (DriverObject && DriverObject->AddDeviceRoutine)
        {
            Status = DriverObject->AddDeviceRoutine(DriverEntry->DriverObject, DeviceNode->PhysicalDeviceObject);
        }
        else if (DriverObject == NULL)
        {
            Status = STATUS_SUCCESS;
        }
        else
        {
            DPRINT("[OBIO] ERROR: DriverObject 0x%p has NO AddDeviceRoutine!\n", DriverObject);
            Status = STATUS_UNSUCCESSFUL;
        }

        if (DriverEntry->DriverType == DeviceDriver)
        {
            if (VEA_SUCCESS(Status))
            {
                PDEVICE_OBJECT Fdo = ObioGetTopDeviceObject(DeviceNode->PhysicalDeviceObject);
                
                ObDereferenceObject(Fdo);
                ObioSetDevNodeState(DeviceNode, DeviceNodeDriversAdded);
            }
            else
            {
                DPRINT("[OBIO] AddDevice FAILED (0x%08x)! Setting DN_PROB_FAILED_ADD.\n", Status);
                ObioSetDevNodeProblem(DeviceNode, DN_PROB_FAILED_ADD);
                ObioSetDevNodeState(DeviceNode, DeviceNodeAwaitingQueuedRemoval);
                break;
            }
        }
    }

Cleanup:
    while (!IsListEmpty(&DriverListHead))
    {
        PLIST_ENTRY listEntry = RemoveHeadList(&DriverListHead);
        PADD_DEV_DRIVERS_LIST DriverEntry;

        DriverEntry = CONTAINING_RECORD(listEntry, ADD_DEV_DRIVERS_LIST, ListEntry);
        if (DriverEntry->DriverObject)
        {
            ObDereferenceObject(DriverEntry->DriverObject);
        }
        UlFreePoolWithTag(DriverEntry, TAG_OBIO);
    }

    VeaClose(SubKey);
    if (ClassKey != NULL)
    {
        VeaClose(ClassKey);
    }

    return Status;
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
                    Status = STATUS_INSUFFICIENT_RESOURCES;
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

                    Status = ObioCallDriverAddDevice(PDO->DeviceNode, FALSE);

                    if (!VEA_SUCCESS(Status))
                    {
                        UlFreePoolWithTag(InstancePath.Buffer, TAG_OBIO);
                        ObDereferenceObject(PDO);
                        goto cleanup;
                    }

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
ObioInitializeDriverModule(
    IN PDATA_TABLE_ENTRY ModuleObject,
    IN HANDLE ServiceHandle,
    OUT PDRIVER_OBJECT *OutDriverObject,
    OUT PVEASTATUS DriverEntryStatus
)
{
    PCHAR DriverNameStr = NULL;
    ANSI_STRING DriverName, RegistryPath;
    HANDLE DriverDirHandle = NULL;
    PDRIVER_OBJECT DriverObject = NULL;
    OBJECT_ATTRIBUTES Attr, DirAttr;
    ANSI_STRING DirString;
    VEASTATUS Status;
    PDRIVER_INITIALIZE DriverInitRoutine;

    if (!ModuleObject || !ServiceHandle || !OutDriverObject)
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(&RegistryPath, sizeof(ANSI_STRING));
    RtlZeroMemory(&DriverName, sizeof(ANSI_STRING));

    Status = ObioGetDriverName(ServiceHandle, &DriverNameStr);
    if (!VEA_SUCCESS(Status) || !DriverNameStr)
    {
        DPRINT("[OBIO] Failed to get driver name from ServiceHandle: 0x%08X\n", Status);
        return Status;
    }

    RtlInitAnsiString(&DriverName, DriverNameStr);

    Status = VeaQueryKeyPath(ServiceHandle, &RegistryPath);
    if (Status == STATUS_BUFFER_TOO_SMALL)
    {
        /* Alokasikan buffer sesuai kebutuhan yang dilaporkan VeaQueryKeyPath */
        RegistryPath.Buffer = (PCHAR)UlAllocatePoolWithTag(
            NonPagedPool,
            RegistryPath.MaximumLength,
            'gReR'
        );

        if (!RegistryPath.Buffer)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        /* Query ulang (Pass 2) dengan buffer yang sudah siap */
        Status = VeaQueryKeyPath(ServiceHandle, &RegistryPath);
    }

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[OBIO] Failed to query registry path for service '%s': 0x%08X\n",
                DriverNameStr, Status);
        if (RegistryPath.Buffer) UlFreePoolWithTag(RegistryPath.Buffer, 'gReR');
        return Status;
    }

    DPRINT("[OBIO] Initializing Driver '%s' (RegPath: %s) at 0x%p\n",
           DriverNameStr, RegistryPath.Buffer, ModuleObject->ImageBase);

    RtlInitAnsiString(&DirString, "/Driver");
    InitializeObjectAttributes(&DirAttr, &DirString, OBJ_CASE_INSENSITIVE, NULL, NULL);

    Status = ObOpenObjectByName(
        &DirAttr,
        ObpDirectoryObjectType,
        KernelMode,
        0x10000,
        NULL,
        &DriverDirHandle
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[OBIO] Failed to open '/Driver' directory object: 0x%08X\n", Status);
        goto CleanupStrings;
    }

    InitializeObjectAttributes(&Attr, &DriverName, OBJ_PERMANENT, DriverDirHandle, NULL);

    Status = ObCreateObject(
        KernelMode,
        ObpDriverObjectType,
        &Attr,
        KernelMode,
        NULL,
        sizeof(DRIVER_OBJECT),
        0,
        0,
        (PVOID*)&DriverObject
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[OBIO] Failed to create DriverObject for '%s': 0x%08X\n", DriverNameStr, Status);
        goto CleanupHandle;
    }

    RtlZeroMemory(DriverObject, sizeof(DRIVER_OBJECT));

    /* Populate Metadata Driver Object */
    DriverObject->DriverStart    = ModuleObject->ImageBase;
    DriverObject->DeviceObject  = NULL;
    DriverObject->InterruptHandler = NULL;

    Status = ObInsertObject(
        DriverObject,
        NULL,
        0,
        0,
        NULL,
        NULL
    );

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[OBIO] Failed to insert DriverObject for '%s': 0x%08X\n", DriverNameStr, Status);
        ObDereferenceObject(DriverObject);
        goto CleanupHandle;
    }

    DriverInitRoutine = (PDRIVER_INITIALIZE)ModuleObject->EntryPoint;

    if (!DriverInitRoutine)
    {
        DPRINT("[OBIO] Driver '%s' has NO valid EntryPoint!\n", DriverNameStr);
        Status = STATUS_INVALID_IMAGE_FORMAT;
        goto CleanupObject;
    }

    DPRINT("Calling DriverInitRoutine()\n");
    Status = DriverInitRoutine(DriverObject);
    DPRINT("Calling Status: 0x%x\n", Status);

    if (DriverEntryStatus)
    {
        *DriverEntryStatus = Status;
    }

    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[OBIO] Driver '%s' DriverEntry failed with status: 0x%08X\n", DriverNameStr, Status);
        goto CleanupObject;
    }

    ObioReadyDeviceObject(DriverObject);

    *OutDriverObject = DriverObject;
    Status = STATUS_SUCCESS;
    goto CleanupHandle;

CleanupObject:
    /* Jika DriverEntry gagal, bersihkan objek dari tree */
    ObMakeTemporaryObject(DriverObject);
    ObDereferenceObject(DriverObject);
    DriverObject = NULL;

CleanupHandle:
    if (DriverDirHandle) VeaClose(DriverDirHandle);

CleanupStrings:
    if (RegistryPath.Buffer) UlFreePool(RegistryPath.Buffer);

    return Status;
}

VEASTATUS
VEAPI
ObioLoadDriver(
    IN HANDLE ServiceHandle,
    OUT PDRIVER_OBJECT *DriverObject
)
{
    ANSI_STRING ImagePath;
    VEASTATUS Status, DriverStatus;
    PVOID BaseAddress;
    ULONG ResultSizeOut;
    PDATA_TABLE_ENTRY ModuleObject;
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

    KsEnterCriticalRegion();

    Status = MmLoadSystemImage(&ImagePath, NULL, NULL, 0, (PVOID*)&ModuleObject, &BaseAddress);
    DPRINT("Loading driver %Z at address %p\n", &ImagePath, BaseAddress);

    UlFreePool(ImagePath.Buffer);
    
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[OBIO] Failed: MmLoadSystemImage failed: 0x%x\n", Status);
        KsLeaveCriticalRegion();
        return Status;
    }

    Status = ObioInitializeDriverModule(
        ModuleObject,
        ServiceHandle,
        DriverObject,
        &DriverStatus
    );
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("[Obio] Fail to initialize driver module: 0x%x driver 0x%x\n", Status, DriverStatus);
    }

    KsLeaveCriticalRegion();

    return Status;
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

    return Status;
}

VOID
VEAPI
ObioDeleteDriver(IN PVOID ObjectBody)
{
    /* UNIMPLEMENTED */
    UNIMPLEMENTED_DBGBREAK;
}

/* EOF */