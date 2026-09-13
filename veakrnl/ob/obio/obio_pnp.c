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

    /* Initialize Group Order Load */
    ObioInitGroupOrderLoad();

    /* Sometimes we go here cause of new device. So we
       need to make several Stable Key's here
       
       1. Control Key at CurrentControlSet
       2. Enum Key at CurrentControlSet
       2.5. Root Key at Control in CurrentControlSet
       3. DeviceClasses at Control in CurrentControlSet */

    /* Open CCS Key */
    Status = ObioOpenVeaKey(&CCSHandle, &CCSString, KEY_ALL_ACCESS);
    if(!VEA_SUCCESS(Status))
    {
        DPRINT("CCS Fail to open.\n");
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

        // VeaClose(DeviceClassesHandle);
    }

    // VeaClose(ControlHandle);

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
    // VeaClose(ParentHandle);
    if(!VEA_SUCCESS(Status))
    {
        return Status;
    }
    // VeaClose(EnumHandle);

    /* Open Root Key now bro */
    RtlInitAnsiString(&KeyName, "/REGISTRY/MACHINE/SYSTEM/CurrentControlSet/Enum");
    Status = ObioOpenVeaKey(
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
    // VeaClose(EnumHandle);
    if (!VEA_SUCCESS(Status))
    {
        DPRINT("ERROR: Gagal bikin HTREE/ROOT/0\n");
        return Status; 
    }
    // VeaClose(TreeHandle);

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
        0,
        "PnpManager", 
        FILE_DEVICE_CONTROLLER,
        &PnpPdo
    );
    
    if(!VEA_SUCCESS(Status))
    {
        DPRINT("ERROR: Fail to create PnP Device Node\n");
        return Status;
    }

    /* Create our DeviceNode as ROOT */
    ObioRootDeviceNode = ObioCreateDeviceNode(PnpPdo);
    if(!ObioRootDeviceNode)
    {
        return STATUS_INSUFFICIENT_MEMORY;
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
        Status = STATUS_INSUFFICIENT_MEMORY;
        return Status;
    }

    RtlCopyRawString(ObioRootDeviceNode->InstancePath.Buffer, PnpDevNodeRoot);

    ObioRootDeviceNode->InstancePath.Length = RtlRawStringLength(PnpDevNodeRoot);
    ObioRootDeviceNode->InstancePath.MaximumLength = RtlRawStringLength(PnpDevNodeRoot);

    /* Enumerate them */
    // ObioQueueDeviceAction(PnpPdo, PiActionEnumRootDevices, NULL, NULL);
    return STATUS_SUCCESS;
}