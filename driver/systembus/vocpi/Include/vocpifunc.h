#pragma once

#include <veaddk.h>
#include "vocpitype.h"

PUCHAR
VEAPI
VOCPIGetAndPushNextAmlByte();

PVOCPI_NAMESPACE_NODE
VEAPI
VOCPICreateNamespaceNode(
    IN PVOCPI_NAMESPACE_NODE Parent,
    IN PCHAR NameSeg,
    IN VOCPI_NODE_TYPE NodeType
);

VEASTATUS
VEAPI
VOCPIProcessNameOp();

VEASTATUS
VEAPI
VOCPIParseNameSeg(
    OUT PCHAR NameSegBuffer
);

BOOLEAN
VEAPI
VOCPIProcessByteCode(
    IN UCHAR ByteCode,
    OUT PVEASTATUS VeaStatus);

VEASTATUS
VEAPI
VOCPIProcessScopeOp();

VEASTATUS
VEAPI
VOCPIProcessMethodOp();

VEASTATUS
VEAPI
VOCPIProcessOpRegionOp();

VEASTATUS
VEAPI
VOCPIProcessDataRegionOp();

VEASTATUS
VEAPI
VOCPIProcessFieldOp();

VEASTATUS
VEAPI
VOCPIProcessPackageBodyOp();

VEASTATUS
VEAPI
VOCPIProcessDeviceOp();

VEASTATUS
VEAPI
VOCPIStateMachine();

VEASTATUS
VEAPI
VOCPIRegisterAmlByte(
    IN PUCHAR AmlByte,
    IN ULONG Length
);

VOID
VEAPI
VOCPIDumpNamespace(
    IN PVOCPI_NAMESPACE_NODE Node,
    IN ULONG Depth
);

VEASTATUS
VEAPI
VOCPIInitNamespace(VOID);

VEASTATUS
VEAPI
VOCPIQueryDeviceRelations(IN PDEVICE_OBJECT Fdo, IN OUT PDEVICE_RELATIONS *DeviceRelations);

VEASTATUS
VEAPI
VOCPIEvaluateHardwareId(
    IN PVOCPI_NAMESPACE_NODE DeviceNode,
    IN PCHAR TargetName,  // "_HID", "_CID", atau "_UID"
    OUT PCHAR OutBuffer,
    IN ULONG BufferLength
);

VEASTATUS
VEAPI
VOCPIQueryDeviceId(
    IN PVOCPI_NAMESPACE_NODE DeviceNode,
    OUT PCHAR Buffer,
    IN ULONG BufferLength
);

VEASTATUS
VEAPI
VOCPIQueryInstanceId(
    IN PVOCPI_NAMESPACE_NODE DeviceNode,
    OUT PCHAR Buffer,
    IN ULONG BufferLength
);

PVOCPI_NAMESPACE_NODE
VEAPI
VOCPIGetChildNode(
    IN PVOCPI_NAMESPACE_NODE Parent,
    IN PCHAR NameSeg
);

VEASTATUS
VEAPI
VOCPIEvaluateMethod(
    IN PVOCPI_NAMESPACE_NODE DeviceNode,
    IN PCHAR MethodName,
    OUT PVOCPI_OBJECT *OutObject
);

VEASTATUS
VEAPI
VOCPIQueryNodeResourcesRequirement(
    IN PVOCPI_NAMESPACE_NODE NamespaceNode,
    OUT POBIO_REQUIRED_RESOURCES_LIST_EX *ListEx
);

VEASTATUS
VEAPI
VOCPIObInterruptHandler(IN PVOID Object, IN POIP Oip);
