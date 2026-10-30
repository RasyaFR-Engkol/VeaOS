#pragma once

#include <veaddk.h>

//
// Dispacther Function
//
VEASTATUS
VEAPI
HDAOipMjPnp(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
);

VEASTATUS
VEAPI
HDAOipMjWrite(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
);

VEASTATUS
VEAPI
HDAOipMjIOCTL(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
);

VEASTATUS
VEAPI
HDAOipMjPower(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
);

#define HDA_MAX_WIDGETS   128
#define HDA_MAX_CONN      16
#define HDA_MAX_PATH      8

typedef struct _HDA_WIDGET {
    UCHAR   Nid;
    UCHAR   Type;
    UCHAR   ConnCount;
    UCHAR   Conn[HDA_MAX_CONN];
    ULONG   Caps;           /* WIDGET_CAPS */
    ULONG   PinCaps;        /* khusus pin */
    ULONG   PinConfig;      /* khusus pin (config default) */
} HDA_WIDGET, *PHDA_WIDGET;

typedef struct _HDA_BDL_ENTRY {
    ULONG AddrLow;
    ULONG AddrHigh;
    ULONG Length;
    ULONG Flags;            /* bit 0 = IOC (interrupt on completion) */
} HDA_BDL_ENTRY, *PHDA_BDL_ENTRY;

typedef struct _HDA_DEVICE_EXTENSION {
    // =========================================================
    // 1. PnP & Device Object Context
    // =========================================================
    PDEVICE_OBJECT      PhysicalDeviceObject;   // PDO dari PCI Bus
    PDEVICE_OBJECT      LowerDeviceObject;      // Target kirim OIP ke bawah
    BOOLEAN             IsStarted;
    ANSI_STRING         SymbolicLinkName;

    // =========================================================
    // 2. Hardware Resources (MMIO & Interrupt)
    // =========================================================
    PUCHAR              HdaMMIOBase;            // Virtual MMIO Address (BAR0)
    ULONG               HdaMmioLength;          // Panjang MMIO BAR0
    ULONG               InterruptVector;        // Assigned Vector Interrupt
    PKINTERRUPT         InterruptObject;        // Ref ke Object Interrupt
    KSPIN_LOCK          ControllerLock;         // Lock buat proteksi sync register / CORB-RIRB
    KIRQL               InterruptIrql;          // isi sama seperti driver PS/2
    volatile ULONG      IrqCount;               // debug
    volatile ULONG      IrqTotal;
    volatile ULONG      IrqSpurious;
    volatile ULONG      IrqStuck;
    volatile ULONG      LastIntSts;
    ULONG               TonePhase;
    ULONG               ToneStep;
    ULONG               ToneFramesLeft;
    ULONG               Rng;
    ULONG               FillChunk;

    // =========================================================
    // 3. CORB (Command Outbound Ring Buffer - DMA)
    // =========================================================
    PULONG              CorbBuffer;             // Virtual Base Address (256 entries x 4 bytes = 1 KB)
    PHYSICAL_ADDRESS    CorbPhysicalAddress;    // Physical Base Address buat Reg 0x40/0x44
    USHORT              CorbEntries;            // Ukuran buffer (256)

    // =========================================================
    // 4. RIRB (Response Inbound Ring Buffer - DMA)
    // =========================================================
    PVOID               RirbBuffer;             // Virtual Base Address (256 entries x 8 bytes = 2 KB)
    PHYSICAL_ADDRESS    RirbPhysicalAddress;    // Physical Base Address buat Reg 0x50/0x54
    USHORT              RirbEntries;            // Ukuran buffer (256)
    USHORT              RirbReadPtr;            // Software tracking index RIRB yang udah dibaca

    // =========================================================
    // 5. Codec Detection & Audio Routing Nodes
    // =========================================================
    USHORT              CodecMask;              // Bitmask codec yang aktif di SDIN (STATESTS 0x0E)
    UCHAR               PrimaryCodecAddress;    // CAD / Codec Address utama (biasanya 0)
    USHORT              VendorId;               // Codec Vendor ID (misal: 0x10EC = Realtek)
    USHORT              DeviceId;               // Codec Device ID (misal: 0x0269 = ALC269)

    // Routing Node ID (NID) sederhana buat output awal
    UCHAR               AudioOutputNID;         // Node ID DAC (Audio Output Converter)
    UCHAR               PinWidgetNID;           // Node ID Pin Complex (Headphone / Speaker)

    // =========================================================
    // 6. Stream Engine Capabilities (Global Capabilities Register / GCAP)
    // =========================================================
    UCHAR               NumInputStreams;        // Jumlah Input Stream Engine (ISS)
    UCHAR               NumOutputStreams;       // Jumlah Output Stream Engine (OSS)
    UCHAR               NumBidirStreams;        // Jumlah Bidirectional Stream Engine (BSS)
    ULONG               StreamDescriptorOffset; // Base MMIO offset ke Stream Descriptor

    // =========================================================
    // 7. Widget Tree & Output Path
    // =========================================================
    UCHAR       AfgNid;
    UCHAR       WidgetStart;
    UCHAR       WidgetCount;
    HDA_WIDGET  Widgets[HDA_MAX_WIDGETS];
    UCHAR       OutPath[HDA_MAX_PATH];      // [0] = pin ... [Len-1] = DAC
    UCHAR       OutPathSel[HDA_MAX_PATH];   // index koneksi yang dipilih per node
    ULONG       OutPathLen;

    // =========================================================
    // 8. Output Stream (stream descriptor output #0)
    // =========================================================
    PVOID               BdlBuffer;              // BDL (virtual)
    PHYSICAL_ADDRESS    BdlPhysical;
    PVOID               StreamBuffer;           // Buffer audio DMA (virtual)
    PHYSICAL_ADDRESS    StreamBufferPhysical;
    ULONG               StreamBufferSize;
    UCHAR               StreamTag;              // 1..15, tidak boleh 0
    USHORT              StreamFormat;           // nilai SDnFMT / converter format
    BOOLEAN             StreamRunning;
    BOOLEAN             UserModeActive;
    PKEVENT             BufferedEvent;
} HDA_DEVICE_EXTENSION, *PHDA_DEVICE_EXTENSION;

#define HDA_BDL_CHUNK   (HDA_STREAM_BUF_SIZE / HDA_BDL_ENTRIES)
VOID 
VEAPI 
HDAGenerateAudio(IN PHDA_DEVICE_EXTENSION DevExt, IN ULONG ByteOffset, IN ULONG ByteLength);

// Offsets Register Intel HD Audio (MMIO)
#define HDA_REG_GCAP            0x00    // Global Capabilities
#define HDA_REG_VMIN            0x02    // Minor Version
#define HDA_REG_VMAJ            0x03    // Major Version
#define HDA_REG_GCTL            0x08    // Global Control
#define HDA_REG_WAKEEN          0x0C    // Wake Enable
#define HDA_REG_STATESTS        0x0E    // State Change Status

// Bit Mask Global Control (GCTL)
#define HDA_GCTL_CRST_MASK      (1 << 0) // Controller Reset (0 = Reset, 1 = Ready)

//
// GCAP
//
#define HDA_GCAP_ISS(g)   (((g) >> 8)  & 0x0F)
#define HDA_GCAP_OSS(g)   (((g) >> 12) & 0x0F)
#define HDA_GCAP_BSS(g)   (((g) >> 3)  & 0x1F)

//
// Widget Type
//
#define HDA_WTYPE_AUDIO_OUT   0x0
#define HDA_WTYPE_AUDIO_IN    0x1
#define HDA_WTYPE_MIXER       0x2
#define HDA_WTYPE_SELECTOR    0x3
#define HDA_WTYPE_PIN         0x4

//
// WIDGET_CAPS bits
//
#define HDA_WCAP_IN_AMP        (1 << 1)
#define HDA_WCAP_OUT_AMP       (1 << 2)
#define HDA_WCAP_AMP_OVERRIDE  (1 << 3)
#define HDA_WCAP_CONN_LIST     (1 << 8)
#define HDA_WCAP_POWER_CTRL    (1 << 10)

//
// PIN_CAPS bits
//
#define HDA_PINCAP_HP_DRIVE    (1 << 3)
#define HDA_PINCAP_OUTPUT      (1 << 4)
#define HDA_PINCAP_EAPD        (1 << 16)

//
// Pin Widget Control
//
#define HDA_PINCTL_OUT_EN      0x40
#define HDA_PINCTL_HP_EN       0x80

//
// Stream Define Register
//
#define HDA_SD_CTL      0x00    /* 3 byte: bit0 SRST, bit1 RUN, bit2 IOCE, bit 23:20 stream tag */
#define HDA_SD_STS      0x03    /* bit2 BCIS, bit3 FIFOE, bit4 DESE, bit5 FIFORDY */
#define HDA_SD_LPIB     0x04    /* Link Position In Buffer */
#define HDA_SD_CBL      0x08    /* Cyclic Buffer Length */
#define HDA_SD_LVI      0x0C    /* Last Valid Index */
#define HDA_SD_FMT      0x12
#define HDA_SD_BDPL     0x18
#define HDA_SD_BDPU     0x1C

#define HDA_SD_CTL_SRST 0x01
#define HDA_SD_CTL_RUN  0x02
#define HDA_SD_CTL_IOCE 0x04

/* SDnFMT: 48 kHz, 16-bit, stereo */
#define HDA_FMT_48K_16BIT_STEREO   0x0011
#define HDA_STREAM_BUF_SIZE   16384
#define HDA_BDL_ENTRIES       8

//
// HDA Interrupt
//
#define HDA_REG_INTCTL   0x20
#define HDA_REG_INTSTS   0x24
#define HDA_INT_GIE      0x80000000UL   /* INTCTL: Global Interrupt Enable */
#define HDA_INT_CIE      0x40000000UL   /* INTCTL: Controller Interrupt Enable */
#define HDA_INT_GIS      0x80000000UL   /* INTSTS: Global Interrupt Status */
#define HDA_INT_CIS      0x40000000UL   /* INTSTS: Controller Interrupt Status */
#define HDA_SD_STS_BCIS  0x04           /* Buffer Completion Interrupt Status */
#define HDA_BDL_IOC      0x00000001     /* flag IOC di BDL entry */


BOOLEAN   
VEAPI 
HDAInterruptHandler(IN PKINTERRUPT Interrupt, IN PVOID ServiceContext);

VEASTATUS 
VEAPI 
HDAConnectInterrupt(IN PHDA_DEVICE_EXTENSION DevExt);

VOID      
VEAPI 
HDAEnableInterrupts(IN PHDA_DEVICE_EXTENSION DevExt);

VOID      
VEAPI 
HDADisableInterrupts(IN PHDA_DEVICE_EXTENSION DevExt);

VEASTATUS 
VEAPI 
HDASetupOutputStream(IN PHDA_DEVICE_EXTENSION DevExt);

VEASTATUS 
VEAPI 
HDAStartOutputStream(IN PHDA_DEVICE_EXTENSION DevExt);

VOID      
VEAPI 
HDAStopOutputStream(IN PHDA_DEVICE_EXTENSION DevExt);

VOID      
VEAPI 
HDAFillTestTone(IN PHDA_DEVICE_EXTENSION DevExt);

ULONG     
VEAPI 
HDASendCommand(IN PHDA_DEVICE_EXTENSION DevExt, IN ULONG Command);

VEASTATUS 
VEAPI 
HDAEnumerateWidgets(IN PHDA_DEVICE_EXTENSION DevExt);

VEASTATUS 
VEAPI 
HDAFindOutputPath(IN PHDA_DEVICE_EXTENSION DevExt);

VEASTATUS 
VEAPI 
HDASetupOutputPath(IN PHDA_DEVICE_EXTENSION DevExt);

// Resource Type Constants
#define OBIO_RESOURCE_TYPE_MEMORY     3
#define OBIO_RESOURCE_TYPE_INTERRUPT  2

VEASTATUS
VEAPI
HDAInitCorbRirb(
    IN PHDA_DEVICE_EXTENSION DevExt
);

VEASTATUS
VEAPI
HDAResetController(
    IN PHDA_DEVICE_EXTENSION DevExt
);

// Struct Entry RIRB (8 Bytes)
typedef struct _HDA_RIRB_ENTRY {
    ULONG Response;             // 32-bit Response Data from Codec
    ULONG ResponseEx;           // Bit 0..3: Codec Address (CAD), Bit 4: Unsolicited flag
} HDA_RIRB_ENTRY, *PHDA_RIRB_ENTRY;

ULONG
VEAPI
HDASendVerb(
    IN PHDA_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR Cad,
    IN UCHAR Nid,
    IN ULONG Verb12,
    IN ULONG Parameter
);

VEASTATUS
VEAPI
HDAProbeCodecs(
    IN PHDA_DEVICE_EXTENSION DevExt
);

/* Command With param */
#define COMMAND_HDA_CAD(c)   ((ULONG)((c)  & 0x0F) << 28)
#define COMMAND_HDA_NID(n)   ((ULONG)((n)  & 0xFF) << 20)
#define COMMAND_HDA_MAKE12(cad, nid, verb, payload) \
    (COMMAND_HDA_CAD(cad) | COMMAND_HDA_NID(nid) | \
     ((ULONG)((verb) & 0xFFF) << 8) | (ULONG)((payload) & 0xFF))
#define COMMAND_HDA_MAKE4(cad, nid, verb, payload) \
    (COMMAND_HDA_CAD(cad) | COMMAND_HDA_NID(nid) | \
     ((ULONG)((verb) & 0xF) << 16) | (ULONG)((payload) & 0xFFFF))

//
// Intel HDA Command
//
#define COMMAND_HDA_VERB_GET_PARAMETER        0xF00
#define COMMAND_HDA_VERB_GET_CONN_SELECT      0xF01
#define COMMAND_HDA_VERB_SET_CONN_SELECT      0x701
#define COMMAND_HDA_VERB_GET_CONN_LIST        0xF02
#define COMMAND_HDA_VERB_GET_PROC_STATE       0xF03
#define COMMAND_HDA_VERB_SET_PROC_STATE       0x703
#define COMMAND_HDA_VERB_GET_POWER_STATE      0xF05
#define COMMAND_HDA_VERB_SET_POWER_STATE      0x705
#define COMMAND_HDA_VERB_GET_STREAM_CHAN      0xF06
#define COMMAND_HDA_VERB_SET_STREAM_CHAN      0x706
#define COMMAND_HDA_VERB_GET_PIN_CTRL         0xF07
#define COMMAND_HDA_VERB_SET_PIN_CTRL         0x707
#define COMMAND_HDA_VERB_GET_UNSOL_RESP       0xF08
#define COMMAND_HDA_VERB_SET_UNSOL_RESP       0x708
#define COMMAND_HDA_VERB_GET_PIN_SENSE        0xF09
#define COMMAND_HDA_VERB_EXEC_PIN_SENSE       0x709
#define COMMAND_HDA_VERB_GET_EAPD             0xF0C
#define COMMAND_HDA_VERB_SET_EAPD             0x70C
#define COMMAND_HDA_VERB_GET_GPIO_DATA        0xF15
#define COMMAND_HDA_VERB_SET_GPIO_DATA        0x715
#define COMMAND_HDA_VERB_GET_GPIO_MASK        0xF16
#define COMMAND_HDA_VERB_SET_GPIO_MASK        0x716
#define COMMAND_HDA_VERB_GET_GPIO_DIR         0xF17
#define COMMAND_HDA_VERB_SET_GPIO_DIR         0x717
#define COMMAND_HDA_VERB_GET_CONFIG_DEFAULT   0xF1C
#define COMMAND_HDA_VERB_GET_SUBSYSTEM_ID     0xF20
#define COMMAND_HDA_VERB_FUNC_GROUP_RESET     0x7FF
#define COMMAND_HDA_VERB_GET_CONV_FORMAT      0xA
#define COMMAND_HDA_VERB_SET_CONV_FORMAT      0x2
#define COMMAND_HDA_VERB_GET_AMP_GAIN         0xB
#define COMMAND_HDA_VERB_SET_AMP_GAIN         0x3
#define COMMAND_HDA_PARAM_VENDOR_ID           0x00
#define COMMAND_HDA_PARAM_REVISION_ID         0x02
#define COMMAND_HDA_PARAM_SUB_NODE_COUNT      0x04
#define COMMAND_HDA_PARAM_FUNC_GROUP_TYPE     0x05
#define COMMAND_HDA_PARAM_AFG_CAPS            0x08
#define COMMAND_HDA_PARAM_WIDGET_CAPS         0x09
#define COMMAND_HDA_PARAM_PCM_SIZE_RATE       0x0A
#define COMMAND_HDA_PARAM_STREAM_FORMATS      0x0B
#define COMMAND_HDA_PARAM_PIN_CAPS            0x0C
#define COMMAND_HDA_PARAM_INPUT_AMP_CAPS      0x0D
#define COMMAND_HDA_PARAM_CONN_LIST_LEN       0x0E
#define COMMAND_HDA_PARAM_POWER_STATES        0x0F
#define COMMAND_HDA_PARAM_PROC_CAPS           0x10
#define COMMAND_HDA_PARAM_GPIO_COUNT          0x11
#define COMMAND_HDA_PARAM_OUTPUT_AMP_CAPS     0x12
#define COMMAND_HDA_PARAM_VOLUME_KNOB_CAPS    0x13

//
// Amplifier Gain Payload Bits
//
#define COMMAND_HDA_AMP_SET_OUTPUT   0x8000
#define COMMAND_HDA_AMP_SET_INPUT    0x4000
#define COMMAND_HDA_AMP_SET_LEFT     0x2000
#define COMMAND_HDA_AMP_SET_RIGHT    0x1000
#define COMMAND_HDA_AMP_INDEX(i)     (((i) & 0x0F) << 8)
#define COMMAND_HDA_AMP_MUTE         0x0080
#define COMMAND_HDA_AMP_GAIN(g)      ((g) & 0x7F)

// 
// Shortcut Commands
//
#define COMMAND_HDA_GET_PARAMETER(cad, nid, param) \
    COMMAND_HDA_MAKE12(cad, nid, COMMAND_HDA_VERB_GET_PARAMETER, param)

#define COMMAND_HDA_GET_VENDOR_ID(cad, nid) \
    COMMAND_HDA_GET_PARAMETER(cad, nid, COMMAND_HDA_PARAM_VENDOR_ID)

#define COMMAND_HDA_GET_SUB_NODE_COUNT(cad, nid) \
    COMMAND_HDA_GET_PARAMETER(cad, nid, COMMAND_HDA_PARAM_SUB_NODE_COUNT)

#define COMMAND_HDA_GET_WIDGET_CAPS(cad, nid) \
    COMMAND_HDA_GET_PARAMETER(cad, nid, COMMAND_HDA_PARAM_WIDGET_CAPS)

#define COMMAND_HDA_GET_CONFIG_DEFAULT(cad, nid) \
    COMMAND_HDA_MAKE12(cad, nid, COMMAND_HDA_VERB_GET_CONFIG_DEFAULT, 0)

#define COMMAND_HDA_SET_POWER_STATE(cad, nid, d) \
    COMMAND_HDA_MAKE12(cad, nid, COMMAND_HDA_VERB_SET_POWER_STATE, d)

#define COMMAND_HDA_SET_PIN_CTRL(cad, nid, v) \
    COMMAND_HDA_MAKE12(cad, nid, COMMAND_HDA_VERB_SET_PIN_CTRL, v)

#define COMMAND_HDA_SET_STREAM_CHAN(cad, nid, stream, chan) \
    COMMAND_HDA_MAKE12(cad, nid, COMMAND_HDA_VERB_SET_STREAM_CHAN, \
                       (((stream) & 0x0F) << 4) | ((chan) & 0x0F))

#define COMMAND_HDA_SET_CONV_FORMAT(cad, nid, fmt) \
    COMMAND_HDA_MAKE4(cad, nid, COMMAND_HDA_VERB_SET_CONV_FORMAT, fmt)

#define COMMAND_HDA_SET_AMP_GAIN(cad, nid, flags) \
    COMMAND_HDA_MAKE4(cad, nid, COMMAND_HDA_VERB_SET_AMP_GAIN, flags)

VEASTATUS
VEAPI
HDAOipMjDevCtl(
    IN PVOID Object,
    IN POIP Oip,
    IN POIP_STACK OipStack
);