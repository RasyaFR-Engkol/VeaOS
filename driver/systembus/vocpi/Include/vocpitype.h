#pragma once

#include "internal/hcttype.h"
#include <veaddk.h>
#include <procbind.h>

typedef struct _VOCPI_DEVICE_EXTENSION {
    PDEVICE_OBJECT Pdo;             // PDO fisik milik HCT
    PDEVICE_OBJECT LowerDevice;     // Balikan dari ObioAttachObject (Target OIP selanjutnya)
    PVEA_ACPI_INTERFACE AcpiInterface; // Interface ACPI yang nanti diminta dari HCT
    BOOLEAN IsStarted;
} VOCPI_DEVICE_EXTENSION, *PVOCPI_DEVICE_EXTENSION;

#define AML_ZERO_OP         0x00
#define AML_ONE_OP          0x01
#define AML_NAME_OP         0x08
#define AML_BYTE_PREFIX     0x0A
#define AML_WORD_PREFIX     0x0B
#define AML_DWORD_PREFIX    0x0C
#define AML_STRING_PREFIX   0x0D
#define AML_SCOPE_OP        0x10
#define AML_BUFFER_OP       0x11
#define AML_PACKAGE_OP      0x12
#define AML_VAR_PACKAGE_OP  0x13
#define AML_METHOD_OP       0x14
#define AML_EXT_OP_PREFIX   0x5B
#define AML_IF_OP           0xA0
#define AML_ELSE_OP         0xA1
#define AML_WHILE_OP        0xA2
#define AML_RETURN_OP       0xA4

/* VOCPI Object Type */
typedef enum _VOCPI_OBJECT_TYPE
{
    VocpiTypeUninitialized = 0,
    VocpiTypeInteger,
    VocpiTypeString,
    VocpiTypeBuffer,
    VocpiTypeObjectReference
} VOCPI_OBJECT_TYPE;

/* VOCPI Object Op */
typedef struct _VOCPI_OBJECT
{
    VOCPI_OBJECT_TYPE Type;
    union
    {
        ULONGLONG Integer;
        struct
        {
            ULONG Length;
            PCHAR String;
        } String;
        struct
        {
            ULONG Length;
            PUCHAR Pointer;
        } Buffer;
        PVOID Reference;
    } Value;
} VOCPI_OBJECT, *PVOCPI_OBJECT;

/* VOCPI Stack */
#define VOCPI_STACK_MAX_DEPTH 64

typedef struct _VOCPI_STACK
{
    ULONG Top;
    VOCPI_OBJECT Items[VOCPI_STACK_MAX_DEPTH];
} VOCPI_STACK, *PVOCPI_STACK;

/* Context / Call Frame */
typedef struct _VOCPI_EXECUTION_CONTEXT
{
    /* Variabel lokal method (Local0 - Local7) */
    VOCPI_OBJECT Local[8];

    /* Argumen yang dipassing ke method (Arg0 - Arg6) */
    VOCPI_OBJECT Arg[7];

    /* Context bytecode khusus untuk method ini */
    PCHAR MethodAmlByte;
    ULONG MethodPointer;
    ULONG MethodLength;

    /* Stack evaluasi lokal */
    VOCPI_STACK EvaluationStack;

    /* Tempat nampung hasil ReturnOp */
    VOCPI_OBJECT ReturnValue;
    BOOLEAN HasReturned;

    /* Pointer ke frame sebelumnya (buat handling nested method call / recursion) */
    struct _VOCPI_EXECUTION_CONTEXT *PreviousFrame;
} VOCPI_EXECUTION_CONTEXT, *PVOCPI_EXECUTION_CONTEXT;

/* VOCPI Node Type */
typedef enum _VOCPI_NODE_TYPE
{
    VocpiNodeRoot = 0,
    VocpiNodeScope,
    VocpiNodeDevice,
    VocpiNodeName,
    VocpiNodeMethod,
    VocpiNodeOpRegion,
    VocpiNodeField
} VOCPI_NODE_TYPE;

/* VOCPI Namespace Node */
typedef struct _VOCPI_NAMESPACE_NODE
{
    CHAR Name[4];                          /* 4-byte NameSeg (contoh: '_SB_', 'THRM') */
    VOCPI_NODE_TYPE NodeType;              /* Tipe node */
    VOCPI_OBJECT Object;                   /* Data/Value (jika ini NameOp / Integer / String) */

    struct _VOCPI_NAMESPACE_NODE *Parent;  /* Parent node */
    struct _VOCPI_NAMESPACE_NODE *Child;   /* Child pertama */
    struct _VOCPI_NAMESPACE_NODE *Next;    /* Sibling (node sejajar) */

    struct _DEVICE_OBJECT *RepresentativeDO;
} VOCPI_NAMESPACE_NODE, *PVOCPI_NAMESPACE_NODE;

typedef
VOID
(VEAPI *VOCPI_NODE_CALLBACK)(
    IN PVOCPI_NAMESPACE_NODE Node,
    IN PVOID Context
);

extern PVOCPI_NAMESPACE_NODE VOCPIRootNode;
extern PVOCPI_NAMESPACE_NODE VOCPICurrentScope;
extern ULONG VOCPIPointerAmlByte;
extern ULONG VOCPIAmlLength;
extern PUCHAR VOCPIAmlByte;

typedef enum _EXTENDED_OP
{
    OpRegionOp = 0x80,
    FieldOp,
    DeviceOp,
    ProcessorOp,
    PowerResourceOp,
    ThermalZoneOp,
    IndexFieldOp,
    BankFieldOp,
    DataRegionOp
} EXTENDED_OP, *PEXTENDED_OP;

/* VOCPI Table */
typedef struct _ACPI_SDT_TABLE
{
    ACPI_HEADER AcpiHeader;
    UCHAR AMLData[];
} ACPI_SSDT_TABLE, 
  *PACPI_SSDT_TABLE,
  ACPI_DSDT_TABLE,
  *PACPI_DSDT_TABLE;

typedef struct _ACPI_GAS{
    UCHAR  AddressSpaceId; // 0 = System Memory, 1 = System I/O
    UCHAR  RegisterBitWidth;
    UCHAR  RegisterBitOffset;
    UCHAR  AccessSize;
    ULONG64 Address;
} ACPI_GAS, *PACPI_GAS;

typedef struct _ACPI_FADT_TABLE
{
    ACPI_HEADER AcpiHeader;

    ULONG FirmwareControl;
    ULONG Dsdt;
    UCHAR Reserved1;
    UCHAR PreferredPMProfile;
    USHORT SCIInterrupt;
    ULONG SMICMDPort;
    UCHAR AcpiEnable;
    UCHAR AcpiDisable;
    UCHAR S4BiosReq;
    UCHAR PStateControl;
    ULONG PM1AEventBlock;
    ULONG PM1BEventBlock;
    ULONG PM1AControlBlock;
    ULONG PM1BControlBlock;
    ULONG PM2ControlBlock;
    ULONG PM1TimerBlock;
    ULONG GPE0Block;
    ULONG GPE1Block;
    UCHAR PM1EventLength;
    UCHAR PM1ControlLength;
    UCHAR PM2ControlLength;
    UCHAR PMTimerLength;
    UCHAR GPE0Length;
    UCHAR GPE1Length;
    UCHAR GPE1Base;
    UCHAR CStateControl;
    USHORT WorstC2Latency;
    USHORT WorstC3Latency;
    USHORT FlushSize;
    USHORT FlushStride;
    UCHAR DutyOffset;
    UCHAR DutyWidth;
    UCHAR DayAlarm;
    UCHAR MountAlarm;
    UCHAR Century;
    USHORT BootArchitectureFlags;
    UCHAR Reserved2;
    ULONG Flags;
    ACPI_GAS ResetRegister;
    UCHAR ResetValue;
    USHORT ArmBootArch;
    UCHAR MinorVersion;
    ULONG64 X_FirmwareControl;
    ULONG64 X_Dsdt;
    ACPI_GAS X_Pm1aEventBlock;
    ACPI_GAS X_Pm1bEventBlock;
    ACPI_GAS X_Pm1aControlBlock;
    ACPI_GAS X_Pm1bControlBlock;
    ACPI_GAS X_Pm2ControlBlock;
    ACPI_GAS X_PmTimerBlock;
    ACPI_GAS X_Gpe0Block;
    ACPI_GAS X_Gpe1Block;
} ACPI_FADT_TABLE, *PACPI_FADT_TABLE;

extern PVOCPI_NAMESPACE_NODE VOCPIRootNode;
extern PVOCPI_NAMESPACE_NODE VOCPICurrentScope;

#define VOCPI_PDO_SIGNATURE 'VPDO'
typedef struct _VOCPI_PDO_EXTENSION {
    ULONG Signature;                      // Buat bedain mana PDO mana FDO pas nerima OIP
    PDEVICE_OBJECT Fdo;                   // FDO pembuat PDO ini (VOCPI)
    PVOCPI_NAMESPACE_NODE AcpiNode;       // Pointer balik ke node namespace AML-nya
    BOOLEAN IsStarted;
    // ... bisa tambah resource list ACPI (IRQ, I/O Port) nanti di sini
} VOCPI_PDO_EXTENSION, *PVOCPI_PDO_EXTENSION;

/* ACPI Resource Header Macros */
#define ACPI_RESOURCE_IS_LARGE(Header)           (((UCHAR)(Header) & 0x80) != 0)

/* Small Resource Items (Bit 7 = 0) */
#define ACPI_SMALL_RESOURCE_TAG(Header)          (((UCHAR)(Header) >> 3) & 0x0F)
#define ACPI_SMALL_RESOURCE_LENGTH(Header)       ((UCHAR)(Header) & 0x07)

/* Large Resource Items (Bit 7 = 1) */
#define ACPI_LARGE_RESOURCE_TAG(Header)          ((UCHAR)(Header) & 0x7F)

/* Known Small Resource Tags */
#define ACPI_SMALL_TAG_IRQ                       0x04
#define ACPI_SMALL_TAG_DMA                       0x05
#define ACPI_SMALL_TAG_IO                        0x08
#define ACPI_SMALL_TAG_FIXED_IO                  0x09
#define ACPI_SMALL_TAG_END                       0x0F
