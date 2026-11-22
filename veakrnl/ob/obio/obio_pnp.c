/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_pnp.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Pnp initializing
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 02-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_pnp.c
 * --------------------------------------------------------------------- */

typedef struct _PNP_EVENT_ENTRY
{
    LIST_ENTRY ListEntry;
    PLUGPLAY_EVENT_BLOCK Event;
} PNP_EVENT_ENTRY, *PPNP_EVENT_ENTRY;

PDRIVER_OBJECT ObioPnpRootDriver;
PDEVICE_NODE ObioRootDeviceNode;

VEASTATUS
VEAPI
ObioInitializePnPService(VOID)
{
    HANDLE CCSHandle, ControlHandle, EnumHandle, RootHandle, TreeHandle;
    VEASTATUS Status; 
    ANSI_STRING CCSString = RTL_CONSTANT_ANSI_STRING("/REGISTRY/MACHINE/SYSTEM/CurrentControlSet");
    ANSI_STRING KeyName; 
    ULONG Disposition;
    PDEVICE_OBJECT PnpPdo;
    PCHAR PnpDevNodeRoot = "HTREE/ROOT/0";

    /* Initialie ListHead*/
    InitializeListHead(&ObioDeviceActionRequestList);
    InitializeListHead(&ObioPnpNotificationList);
    InitializeListHead(&ObioPnpRootPdoList);

    /* Initialize SpinLock */
    KsInitializeSpinlock(&ObioDeviceTreeLock);
    KsInitializeSpinlock(&ObioPnpRootLock);

    /* Initialize Group Order Load */
    ObioInitGroupOrderLoad();

    /* Sometimes we go here cause of new device. So we
       need to make several Stable Key's here
       
       1. Control Key at CurrentControlSet
       2. Enum Key at CurrentControlSet
       2.5. Root Key at Control in CurrentControlSet
       3. DeviceClasses at Control in CurrentControlSet */

    /* Open CCS Key */
    Status = ObioOpenVeaKey(NULL, &CCSHandle, &CCSString, KEY_ALL_ACCESS);
    if(!VEA_SUCCESS(Status))
    {
        DPRINT("CCS Fail to open.\n");
        ObDumpObjectTree();
        return Status;
    }

    /* Create Control key there */
    RtlInitAnsiString(&KeyName, "Control");
    Status = ObioCreateVeaKey(
        CCSHandle,
        &KeyName,
        REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS,
        &Disposition,
        &ControlHandle
    );

    if(!VEA_SUCCESS(Status))
    {
        DPRINT("ERROR: Something fail while making the key Control.\n");
        return Status;
    }

    /* Check disposition. It should be Creating new key */
    if(Disposition == REG_CREATED_NEW_KEY)
    {
        HANDLE DeviceClassesHandle;

        RtlInitAnsiString(&KeyName, "DeviceClasses");
        Status = ObioCreateVeaKey(
            ControlHandle,
            &KeyName,
            REG_OPTION_NON_VOLATILE,
            KEY_ALL_ACCESS,
            &Disposition,
            &DeviceClassesHandle
        );

        if(!VEA_SUCCESS(Status))
        {
            return Status;
        }

        VeaClose(DeviceClassesHandle);
    }

    VeaClose(ControlHandle);

    /* Create Enum Key */
    RtlInitAnsiString(&KeyName, "Enum");
    Status = ObioCreateVeaKey(
        CCSHandle,
        &KeyName,
        REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS,
        &Disposition,
        &EnumHandle
    );
    if(!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* Create Root Key */
    RtlInitAnsiString(&KeyName, "Root");
    Status = ObioCreateVeaKey(
        EnumHandle,
        &KeyName,
        REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS,
        &Disposition,
        &RootHandle
    );
    VeaClose(RootHandle);
    if(!VEA_SUCCESS(Status))
    {
        return Status;
    }
    VeaClose(EnumHandle);

    /* Open Root Key now bro */
    RtlInitAnsiString(&KeyName, "/REGISTRY/MACHINE/SYSTEM/CurrentControlSet/Enum");
    Status = ObioOpenVeaKey(
        NULL,
        &EnumHandle,
        &KeyName,
        KEY_ALL_ACCESS
    );

    if(!VEA_SUCCESS(Status))
    {
        return Status;
    }

    /* Now we can make Root Dev Node. This is core of PnP Node */
    RtlInitAnsiString(&KeyName, "HTREE/ROOT/0");
    Status = ObioCreateVeaKey(
        EnumHandle,
        &KeyName,
        REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS,
        &Disposition,
        &TreeHandle
    );

    /* Close old Enum KEY */
    VeaClose(EnumHandle);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("ERROR: Gagal bikin HTREE/ROOT/0\n");
        return Status; 
    }
    VeaClose(TreeHandle);

    /* Create our PNP driver Node */
    Status = ObioCreateDriver("PnpManager", ObioPnpRootDriverEntry, &ObioPnpRootDriver);
    if(!VEA_SUCCESS(Status))
    {
        DPRINT("ERROR: Fail to create PnP Driver Node\n");
        return Status;
    }

    /* Driver is orphaned. How do we fix it? OFC MAKE PDO */
    Status = ObioCreateNewDevice(
        ObioPnpRootDriver, 
        sizeof(PNPROOT_PDO_EXTENSION),
        "PnpManager", 
        FILE_DEVICE_CONTROLLER,
        &PnpPdo
    );
    
    if(!VEA_SUCCESS(Status))
    {
        DPRINT("ERROR: Fail to create PnP Device Node\n");
        return Status;
    }

    {
        PPNPROOT_PDO_EXTENSION RootExt = (PPNPROOT_PDO_EXTENSION)PnpPdo->DeviceExtension;
        RtlZeroMemory(RootExt, sizeof(PNPROOT_PDO_EXTENSION));
        RootExt->IsRootBus = TRUE;
        RtlCopyRawString(RootExt->DeviceId, "HTREE/ROOT");
        RtlCopyRawString(RootExt->InstanceId, "0");
        PnpPdo->Flags &= ~DO_DEVICE_INITIALIZING;
    }

    /* Create our DeviceNode as ROOT */
    ObioRootDeviceNode = ObioCreateDeviceNode(PnpPdo);
    if(!ObioRootDeviceNode)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* Set the state */
    ObioRootDeviceNode->Flags = OBIO_DEVNODE_SYNTHETIC;
    ObioPnpChangeState(ObioRootDeviceNode, DeviceNodeStarted);

    /* Set instance path */
    ObioRootDeviceNode->InstancePath.Buffer = UlAllocatePoolZero(
        NonPagedPool,
        RtlRawStringLength(PnpDevNodeRoot) + 1,
        VEA_DEVNODE_TAG
    );

    if(ObioRootDeviceNode->InstancePath.Buffer == NULL)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        return Status;
    }

    RtlCopyRawString(ObioRootDeviceNode->InstancePath.Buffer, PnpDevNodeRoot);

    ObioRootDeviceNode->InstancePath.Length = RtlRawStringLength(PnpDevNodeRoot);
    ObioRootDeviceNode->InstancePath.MaximumLength = RtlRawStringLength(PnpDevNodeRoot);

    /* Enumerate them */
    ObioQueueDeviceAction(PnpPdo, PiActionEnumRootDevices, NULL, NULL);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObioReportDetectedDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PCSTR DeviceId OPTIONAL,
    IN PCSTR ServiceName OPTIONAL,
    IN POBIO_RESOURCE_LIST ResourceList OPTIONAL,
    IN OUT PDEVICE_OBJECT *DeviceObject
)
{
    VEASTATUS Status;
    PDEVICE_OBJECT Pdo = NULL;
    PPNPROOT_PDO_EXTENSION PdoExt;
    PDEVICE_NODE DevNode;
    CHAR TargetDeviceId[64];
    CHAR InstanceIdBuf[32];
    ANSI_STRING TargetIdAnsi;
    PLIST_ENTRY Entry;
    KIRQL OldIrql;
    ULONG InstanceNumber = 0;
    HANDLE EnumHandle = NULL;
    HANDLE InstanceKeyHandle = NULL;
    ANSI_STRING EnumRootPath = RTL_CONSTANT_ANSI_STRING("/REGISTRY/MACHINE/SYSTEM/CurrentControlSet/Enum");
    ULONG Disposition;

    if (!DriverObject || !DeviceObject)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (!ObioPnpRootDriver || !ObioRootDeviceNode)
    {
        DPRINT("[OBIO] ObioReportDetectedDevice called before PnP initialization!\n");
        return STATUS_UNSUCCESSFUL;
    }

    RtlZeroMemory(TargetDeviceId, sizeof(TargetDeviceId));
    if (DeviceId && DeviceId[0] != '\0')
    {
        if ((DeviceId[0] == 'R' || DeviceId[0] == 'r') &&
            (DeviceId[1] == 'O' || DeviceId[1] == 'o') &&
            (DeviceId[2] == 'O' || DeviceId[2] == 'o') &&
            (DeviceId[3] == 'T' || DeviceId[3] == 't') &&
            (DeviceId[4] == '/' || DeviceId[4] == '\\'))
        {
            RtlStringCbPrintfA(TargetDeviceId, sizeof(TargetDeviceId), "%s", DeviceId);
        }
        else
        {
            RtlStringCbPrintfA(TargetDeviceId, sizeof(TargetDeviceId), "ROOT/%s", DeviceId);
        }
    }
    else
    {
        RtlCopyRawString(TargetDeviceId, "ROOT/LEGACY_DEVICE");
    }

    RtlInitAnsiString(&TargetIdAnsi, TargetDeviceId);

    KsAcquireSpinLock(&ObioPnpRootLock, &OldIrql);
    for (Entry = ObioPnpRootPdoList.Flink;
         Entry != &ObioPnpRootPdoList;
         Entry = Entry->Flink)
    {
        PPNPROOT_PDO_EXTENSION ExistingExt;
        ANSI_STRING ExistingIdAnsi;

        ExistingExt = CONTAINING_RECORD(Entry, PNPROOT_PDO_EXTENSION, ListEntry);
        RtlInitAnsiString(&ExistingIdAnsi, ExistingExt->DeviceId);

        if (RtlEqualString(&TargetIdAnsi, &ExistingIdAnsi, TRUE))
        {
            InstanceNumber++;
        }
    }
    KsReleaseSpinLock(&ObioPnpRootLock, OldIrql);

    RtlStringCbPrintfA(InstanceIdBuf, sizeof(InstanceIdBuf), "%04u", InstanceNumber);

    if (*DeviceObject == NULL)
    {
        Status = ObioCreateNewDevice(
            ObioPnpRootDriver,
            sizeof(PNPROOT_PDO_EXTENSION),
            NULL,
            FILE_DEVICE_UNKNOWN,
            &Pdo
        );
        if (!VEA_SUCCESS(Status))
        {
            DPRINT("[OBIO] ObioReportDetectedDevice: failed to create PDO (0x%x)\n", Status);
            return Status;
        }
    }
    else
    {
        Pdo = *DeviceObject;
    }

    PdoExt = (PPNPROOT_PDO_EXTENSION)Pdo->DeviceExtension;
    RtlZeroMemory(PdoExt, sizeof(PNPROOT_PDO_EXTENSION));
    PdoExt->IsRootBus = FALSE;
    RtlCopyRawString(PdoExt->DeviceId, TargetDeviceId);
    RtlCopyRawString(PdoExt->InstanceId, InstanceIdBuf);

    Pdo->Flags &= ~DO_DEVICE_INITIALIZING;
    Pdo->Flags |= DO_BUS_ENUMERATED_DEVICE;

    DevNode = ObioCreateDeviceNode(Pdo);
    if (!DevNode)
    {
        DPRINT("[OBIO] ObioReportDetectedDevice: ObioCreateDeviceNode failed for %s/%s\n",
               TargetDeviceId, InstanceIdBuf);
        ObDereferenceObject(Pdo);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    DevNode->Flags |= (OBIO_DEVNODE_SYNTHETIC | OBIO_DEVNODE_ENUMERATED);
    if (ResourceList != NULL)
    {
        DevNode->AdditionalResources = ResourceList;
        DevNode->Flags |= DNF_HAS_BOOT_CONFIG;
    }

    KsAcquireSpinLock(&ObioPnpRootLock, &OldIrql);
    InsertTailList(&ObioPnpRootPdoList, &PdoExt->ListEntry);
    KsReleaseSpinLock(&ObioPnpRootLock, OldIrql);

    Status = ObioOpenVeaKey(NULL, &EnumHandle, &EnumRootPath, KEY_ALL_ACCESS);
    if (VEA_SUCCESS(Status))
    {
        Status = ObioCreateVeaKey(
            EnumHandle,
            &DevNode->InstancePath,
            REG_OPTION_NON_VOLATILE,
            KEY_ALL_ACCESS,
            &Disposition,
            &InstanceKeyHandle
        );
        if (VEA_SUCCESS(Status))
        {
            if (ServiceName && ServiceName[0] != '\0')
            {
                ANSI_STRING ServiceValName = RTL_CONSTANT_ANSI_STRING("Service");
                ULONG ServiceLen = (ULONG)RtlRawStringLength((PCHAR)ServiceName);

                VeaSetValueKey(
                    InstanceKeyHandle,
                    &ServiceValName,
                    0,
                    FILE_KEY_STRING,
                    (PVOID)ServiceName,
                    ServiceLen
                );
            }
            VeaClose(InstanceKeyHandle);
        }
        VeaClose(EnumHandle);
    }

    ObioInsertDeviceNode(DevNode, ObioRootDeviceNode);
    ObioSetDevNodeState(DevNode, DeviceNodeStarted);

    *DeviceObject = Pdo;
    DPRINT("[OBIO] Reported detected device: %Z (PDO=%p)\n", &DevNode->InstancePath, Pdo);

    return STATUS_SUCCESS;
}
