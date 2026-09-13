/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : obio_devnode.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : DevNode
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 02-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file obio_devnode.c
 * --------------------------------------------------------------------- */

PDEVICE_NODE
VEAPI
ObioCreateDeviceNode(IN PDEVICE_OBJECT Pdo)
{
    if(!Pdo) return NULL;

    PDEVICE_NODE NewNode = (PDEVICE_NODE)UlAllocatePoolWithTag(
        NonPagedPool,
        sizeof(DEVICE_NODE),
        VEA_DEVNODE_TAG
    );

    if (!NewNode)
    {
        DPRINT("[Ob] ERROR | Gagal alokasi DEVICE_NODE buat Pdo 0x%p\n", Pdo);
        return NULL;
    }

    RtlZeroMemory(NewNode, sizeof(DEVICE_NODE));

    NewNode->PhysicalDeviceObject = Pdo;
    NewNode->Parent               = NULL;
    NewNode->Sibling              = NULL;
    NewNode->Child                = NULL;
    NewNode->State                = DeviceNodeUninitialized;
    NewNode->Flags                = 0;
    RtlInitAnsiString(&NewNode->InstancePath, NULL);

    return NewNode;
}

VEASTATUS
VEAPI
ObioRegisterRootDeviceNode(
    IN PDEVICE_OBJECT Pdo,
    IN PCSTR HardwareId        // <--- Masukkan HardwareID sebagai parameter!
)
{
    PDEVICE_NODE DevNode;
    PCHAR FinalPathBuffer = NULL;
    USHORT FinalPathLen = 0;

    if (!Pdo) {
        return STATUS_INVALID_PARAMETER;
    }

    if (!ObioRootDeviceNode) {
        DPRINT("ERROR: ObioRootDeviceNode belum diinisialisasi!\n");
        return STATUS_UNSUCCESSFUL;
    }

    DevNode = ObioCreateDeviceNode(Pdo);
    if (!DevNode) {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    /* 1. Sambungkan ke Root Tree */
    DevNode->Parent = ObioRootDeviceNode;
    DevNode->Sibling = ObioRootDeviceNode->Child;
    ObioRootDeviceNode->Child = DevNode;

    /* 2. Format Path "ROOT\<HardwareId>" secara dinamis */
    PCSTR TargetHwId = (HardwareId != NULL) ? HardwareId : "ACPI_HAL/0000";
    USHORT HwIdLen   = RtlRawStringLength(TargetHwId);
    USHORT PrefixLen = 5; // "ROOT\"

    FinalPathLen = PrefixLen + HwIdLen;
    FinalPathBuffer = (PCHAR)UlAllocatePoolZero(
        NonPagedPool,
        FinalPathLen + 1,
        VEA_DEVNODE_TAG
    );

    if (!FinalPathBuffer) {
        ObioPnpChangeState(DevNode, DeviceNodeFailed);
        return STATUS_INSUFFICIENT_MEMORY;
    }

    RtlCopyRawString(FinalPathBuffer, "ROOT/");
    RtlCopyRawString(FinalPathBuffer + PrefixLen, TargetHwId);

    /* 3. Assign ke InstancePath */
    DevNode->InstancePath.Buffer        = FinalPathBuffer;
    DevNode->InstancePath.Length        = FinalPathLen;
    DevNode->InstancePath.MaximumLength = FinalPathLen + 1;

    DevNode->Flags |= (OBIO_DEVNODE_SYNTHETIC | OBIO_DEVNODE_ENUMERATED | OBIO_DEVNODE_BOOT_CRITICAL);
    
    ObioPnpChangeState(DevNode, DeviceNodeStarted);

    DPRINT("Obio: Root Device Node [%s] successfully registered under HTREE/ROOT/0\n", DevNode->InstancePath.Buffer);

    return STATUS_SUCCESS;
}