/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : hdaengine.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : HDA Engine
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veaddk.h>
#include "hdaheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 02-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file hdaengine.c
 * --------------------------------------------------------------------- */

VEASTATUS
VEAPI
HDAResetController(
    IN PHDA_DEVICE_EXTENSION DevExt
)
{
    ULONG Gctl;
    USHORT StateSts;
    ULONG Timeout;
    USHORT Gcap;

    DPRINT("HDA: Starting Controller Reset sequence...\n");

    // 1. Read Global Control Register (GCTL - Offset 0x08)
    Gctl = READ_REGISTER_ULONG((PULONG)(DevExt->HdaMMIOBase + 0x08));

    // Clear Bit 0 (CRST = 0) -> Force Controller to Reset
    WRITE_REGISTER_ULONG((PULONG)(DevExt->HdaMMIOBase + 0x08), Gctl & ~(1 << 0));

    // Wait loop sampai Hardware mengonfirmasi bit CRST berubah jadi 0
    Timeout = 10000;
    while ((READ_REGISTER_ULONG((PULONG)(DevExt->HdaMMIOBase + 0x08)) & (1 << 0)) != 0)
    {
        if (--Timeout == 0) {
            DPRINT("HDA: ERR - Timeout entering reset state!\n");
            return STATUS_UNSUCCESSFUL;
        }
    }

    // Delay sebentar (biasanya ~50us) biar hardware bener-bener reset state
    KsStallExecutionProcessor(50);

    // Set Bit 0 (CRST = 1) -> Bring Controller OUT of Reset
    WRITE_REGISTER_ULONG((PULONG)(DevExt->HdaMMIOBase + 0x08), Gctl | (1 << 0));

    // Wait loop sampai Hardware CRST aktif (Bit 0 == 1)
    Timeout = 10000;
    while ((READ_REGISTER_ULONG((PULONG)(DevExt->HdaMMIOBase + 0x08)) & (1 << 0)) == 0)
    {
        if (--Timeout == 0) {
            DPRINT("HDA: ERR - Timeout exiting reset state!\n");
            return STATUS_UNSUCCESSFUL;
        }
    }

    // Delay ~521us untuk nunggu Codec wake-up & kirim State Change Status
    KsStallExecutionProcessor(1000);

    // 2. Baca State Change Status (STATESTS - Offset 0x0E)
    StateSts = READ_REGISTER_USHORT((PUSHORT)(DevExt->HdaMMIOBase + 0x0E));
    DevExt->CodecMask = StateSts & 0x7FFF; // Bit 0..14 menandakan Codec ID 0..14 yang terdeteksi

    DPRINT("HDA: Controller Reset complete. Codec Mask detected: 0x%04X\n", DevExt->CodecMask);

    if (DevExt->CodecMask == 0) {
        DPRINT("HDA: WARNING - No codecs detected on SDIN lines!\n");
    }

    Gcap = READ_REGISTER_USHORT((PUSHORT)(DevExt->HdaMMIOBase + HDA_REG_GCAP));

    DevExt->NumInputStreams  = (UCHAR)HDA_GCAP_ISS(Gcap);
    DevExt->NumOutputStreams = (UCHAR)HDA_GCAP_OSS(Gcap);
    DevExt->NumBidirStreams  = (UCHAR)HDA_GCAP_BSS(Gcap);

    DevExt->StreamDescriptorOffset = 0x80 + (DevExt->NumInputStreams * 0x20);

    DPRINT("HDA: GCAP=%04X ISS=%u OSS=%u BSS=%u OutStreamOffset=0x%X\n",
               Gcap, DevExt->NumInputStreams, DevExt->NumOutputStreams,
               DevExt->NumBidirStreams, DevExt->StreamDescriptorOffset);

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
HDAInitCorbRirb(
    IN PHDA_DEVICE_EXTENSION DevExt
)
{
    PHYSICAL_ADDRESS PhysAddr;
    ULONG Timeout;

    // Setup CORB
    DPRINT("Setup CORB for HDA\n");
    DevExt->CorbBuffer = MmAllocateContiguousMemory(1024, (PHYSICAL_ADDRESS){.QuadPart = 0xFFFFFFFF});
    if (!DevExt->CorbBuffer) {
        DPRINT("HDA: Failed to allocate DMA buffer for CORB\n");
        return STATUS_INSUFFICIENT_MEMORY;
    }
    DPRINT("Setup CORB for HDA done\n");
    RtlZeroMemory(DevExt->CorbBuffer, 1024);

    PhysAddr = MmGetPhysicalAddress(DevExt->CorbBuffer);
    DevExt->CorbPhysicalAddress = PhysAddr;

    // Stop CORB DMA Engine dulu sebelum diset (CORBCTL @ 0x4C)
    WRITE_REGISTER_UCHAR(DevExt->HdaMMIOBase + 0x4C, 0);

    // Program CORB Base Address (Lower & Upper 32-bit @ 0x40 & 0x44)
    WRITE_REGISTER_ULONG((PULONG)(DevExt->HdaMMIOBase + 0x40), PhysAddr.LowPart);
    WRITE_REGISTER_ULONG((PULONG)(DevExt->HdaMMIOBase + 0x44), PhysAddr.HighPart);

    // Set CORB Size ke 256 entries (CORBSIZE @ 0x4E: Bit 0..1 = 10b / 0x02)
    WRITE_REGISTER_UCHAR(DevExt->HdaMMIOBase + 0x4E, 0x02);

    // Reset CORB Read Pointer (CORBRP @ 0x4A: Set Bit 15)
    WRITE_REGISTER_USHORT((PUSHORT)(DevExt->HdaMMIOBase + 0x4A), 0x8000);
    for (Timeout = 1000; Timeout; Timeout--) {
        if (READ_REGISTER_USHORT((PUSHORT)(DevExt->HdaMMIOBase + 0x4A)) & 0x8000) break;
        KsStallExecutionProcessor(10);
    }

    WRITE_REGISTER_USHORT((PUSHORT)(DevExt->HdaMMIOBase + 0x4A), 0);
    for (Timeout = 1000; Timeout; Timeout--) {
        if (!(READ_REGISTER_USHORT((PUSHORT)(DevExt->HdaMMIOBase + 0x4A)) & 0x8000)) break;
        KsStallExecutionProcessor(10);
    }

    // Reset CORB Write Pointer (CORBWP @ 0x48)
    WRITE_REGISTER_USHORT((PUSHORT)(DevExt->HdaMMIOBase + 0x48), 0);

    // Enable CORB DMA Engine (CORBCTL @ 0x4C: Set Bit 1 = RUN)
    WRITE_REGISTER_UCHAR(DevExt->HdaMMIOBase + 0x4C, (1 << 1));


    // Setup RIRB
    DPRINT("Setup RIRB for HDA\n");
    DevExt->RirbBuffer = MmAllocateContiguousMemory(2048, (PHYSICAL_ADDRESS){.QuadPart = 0xFFFFFFFF});
    if (!DevExt->RirbBuffer) {
        DPRINT("HDA: Failed to allocate DMA buffer for RIRB\n");
        return STATUS_INSUFFICIENT_MEMORY;
    }
    DPRINT("Setup RIRB for HDA done\n");
    RtlZeroMemory(DevExt->RirbBuffer, 2048);

    PhysAddr = MmGetPhysicalAddress(DevExt->RirbBuffer);
    DevExt->RirbPhysicalAddress = PhysAddr;

    // Stop RIRB DMA Engine dulu (RIRBCTL @ 0x5C)
    WRITE_REGISTER_UCHAR(DevExt->HdaMMIOBase + 0x5C, 0);

    // Program RIRB Base Address (Lower & Upper 32-bit @ 0x50 & 0x54)
    WRITE_REGISTER_ULONG((PULONG)(DevExt->HdaMMIOBase + 0x50), PhysAddr.LowPart);
    WRITE_REGISTER_ULONG((PULONG)(DevExt->HdaMMIOBase + 0x54), PhysAddr.HighPart);

    // Set RIRB Size ke 256 entries (RIRBSIZE @ 0x5E: Bit 0..1 = 10b / 0x02)
    WRITE_REGISTER_UCHAR(DevExt->HdaMMIOBase + 0x5E, 0x02);

    // Reset RIRB Write Pointer (RIRBWP @ 0x58: Set Bit 15)
    WRITE_REGISTER_USHORT((PUSHORT)(DevExt->HdaMMIOBase + 0x58), (1 << 15));

    /* Response interrupt count = 1 (RINTCNT @ 0x5A) */
    WRITE_REGISTER_USHORT((PUSHORT)(DevExt->HdaMMIOBase + 0x5A), 1);

    // Enable RIRB DMA Engine & Response Interrupt (RIRBCTL @ 0x5C: Bit 1 = RUN, Bit 0 = INTCTL)
    WRITE_REGISTER_UCHAR(DevExt->HdaMMIOBase + 0x5C, (1 << 1) | (1 << 0));

    DPRINT("HDA: CORB/RIRB Ring Buffers initialized successfully!\n");
    DPRINT("HDA: CORB virt=%p phys=%08X, RIRB virt=%p phys=%08X\n",
        DevExt->CorbBuffer, DevExt->CorbPhysicalAddress.LowPart,
        DevExt->RirbBuffer, DevExt->RirbPhysicalAddress.LowPart);
    return STATUS_SUCCESS;
}

ULONG
VEAPI
HDASendCommand(
    IN PHDA_DEVICE_EXTENSION DeviceExtension,
    IN ULONG Command
)
{
    USHORT CorbWp;
    USHORT RirbWp;
    ULONG Timeout;
    PHDA_RIRB_ENTRY RirbEntry;
    ULONG Response; 

    /* Ring buffer */
    CorbWp = READ_REGISTER_USHORT((PUSHORT)(DeviceExtension->HdaMMIOBase + 0x48)) & 0xFF;
    CorbWp = (CorbWp + 1) % 256;

    DeviceExtension->CorbBuffer[CorbWp] = Command;

    WRITE_REGISTER_USHORT((PUSHORT)(DeviceExtension->HdaMMIOBase + 0x48), CorbWp);

    Timeout = 10000;
    while (TRUE)
    {
        RirbWp = READ_REGISTER_USHORT((PUSHORT)(DeviceExtension->HdaMMIOBase + 0x58)) & 0xFF;
        if (RirbWp != DeviceExtension->RirbReadPtr) {
            DeviceExtension->RirbReadPtr = RirbWp;
            break;
        }

        if (--Timeout == 0) {
            DPRINT("HDA: ERR - Timeout waiting for RIRB Response (CAD:%u NID:0x%02X)!\n", (Command >> 28) & 0x0F, (Command >> 20) & 0xFF);
            DPRINT("HDA: CORBWP=%04X CORBRP=%04X CORBCTL=%02X CORBSTS=%02X RIRBWP=%04X RIRBCTL=%02X RIRBSTS=%02X\n",
                READ_REGISTER_USHORT((PUSHORT)(DeviceExtension->HdaMMIOBase + 0x48)),
                READ_REGISTER_USHORT((PUSHORT)(DeviceExtension->HdaMMIOBase + 0x4A)),
                READ_REGISTER_UCHAR(DeviceExtension->HdaMMIOBase + 0x4C),
                READ_REGISTER_UCHAR(DeviceExtension->HdaMMIOBase + 0x4D),
                READ_REGISTER_USHORT((PUSHORT)(DeviceExtension->HdaMMIOBase + 0x58)),
                READ_REGISTER_UCHAR(DeviceExtension->HdaMMIOBase + 0x5C),
                READ_REGISTER_UCHAR(DeviceExtension->HdaMMIOBase + 0x5D));
            return 0xFFFFFFFF; // Error response
        }
        
        KsStallExecutionProcessor(10);
    }

    RirbEntry = (PHDA_RIRB_ENTRY)DeviceExtension->RirbBuffer;
    Response = RirbEntry[DeviceExtension->RirbReadPtr].Response;
    WRITE_REGISTER_UCHAR(DeviceExtension->HdaMMIOBase + 0x5D, 0x05);

    return Response;
}

ULONG
VEAPI
HDASendVerb(
    IN PHDA_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR Cad,
    IN UCHAR Nid,
    IN ULONG Verb12,
    IN ULONG Parameter
)
{
    return HDASendCommand(DeviceExtension,
                          COMMAND_HDA_MAKE12(Cad, Nid, Verb12, Parameter));
}

VEASTATUS
VEAPI
HDAProbeCodecs(IN PHDA_DEVICE_EXTENSION DevExt)
{
    ULONG VendorResponse;
    ULONG NodeCount;
    ULONG GroupType;
    UCHAR Cad;
    UCHAR StartNid;
    UCHAR Count;
    UCHAR Nid;
    UCHAR AfgNid;

    DPRINT("HDA: Probing Codecs...\n");

    for (Cad = 0; Cad < 15; Cad++)
    {
        if (!(DevExt->CodecMask & (1 << Cad))) {
            continue;
        }

        /* Vendor ID & Device ID dari Root Node (NID 0) */
        VendorResponse = HDASendVerb(DevExt, Cad, 0x00,
                                     COMMAND_HDA_VERB_GET_PARAMETER,
                                     COMMAND_HDA_PARAM_VENDOR_ID);
        if (VendorResponse == 0xFFFFFFFF || VendorResponse == 0) {
            DPRINT("HDA: Codec CAD %u failed to respond to Vendor ID probe!\n", Cad);
            continue;
        }

        DevExt->PrimaryCodecAddress = Cad;
        DevExt->VendorId = (USHORT)(VendorResponse >> 16);
        DevExt->DeviceId = (USHORT)(VendorResponse & 0xFFFF);

        DPRINT("HDA: Found Codec CAD %u -> Vendor ID: 0x%04X, Device ID: 0x%04X\n",
               Cad, DevExt->VendorId, DevExt->DeviceId);

        /* Cari Audio Function Group (AFG) di bawah root */
        NodeCount = HDASendVerb(DevExt, Cad, 0x00,
                                COMMAND_HDA_VERB_GET_PARAMETER,
                                COMMAND_HDA_PARAM_SUB_NODE_COUNT);
        StartNid = (UCHAR)((NodeCount >> 16) & 0xFF);
        Count    = (UCHAR)(NodeCount & 0xFF);
        AfgNid   = 0x01;    /* fallback kalau tidak ketemu */

        for (Nid = StartNid; Nid < (UCHAR)(StartNid + Count); Nid++)
        {
            GroupType = HDASendVerb(DevExt, Cad, Nid,
                                    COMMAND_HDA_VERB_GET_PARAMETER,
                                    COMMAND_HDA_PARAM_FUNC_GROUP_TYPE);
            if ((GroupType & 0xFF) == 0x01) {   /* 0x01 = Audio Function Group */
                AfgNid = Nid;
                break;
            }
        }

        DPRINT("HDA: AFG NID = 0x%02X\n", AfgNid);

        DevExt->AfgNid = AfgNid;

        /* Power state D0 dikirim ke AFG, bukan root node */
        HDASendVerb(DevExt, Cad, AfgNid,
                    COMMAND_HDA_VERB_SET_POWER_STATE, 0x00 /* D0 */);

        /* MVP: cukup codec pertama yang menjawab */
        break;
    }

    return (DevExt->VendorId != 0) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

static PHDA_WIDGET
HDAGetWidget(IN PHDA_DEVICE_EXTENSION DevExt, IN UCHAR Nid)
{
    if ((ULONG)Nid < DevExt->WidgetStart ||
        (ULONG)Nid >= (ULONG)DevExt->WidgetStart + DevExt->WidgetCount) {
        return NULL;
    }
    return &DevExt->Widgets[Nid - DevExt->WidgetStart];
}

static ULONG
HDAGetConnList(
    IN PHDA_DEVICE_EXTENSION DevExt,
    IN UCHAR Nid,
    OUT PUCHAR List,
    IN ULONG MaxCount
)
{
    UCHAR Cad = DevExt->PrimaryCodecAddress;
    ULONG LenResp, Len, LongForm, Per, i, k, Entry, Range;
    ULONG Resp = 0, Count = 0;
    UCHAR Prev = 0;

    LenResp = HDASendVerb(DevExt, Cad, Nid, COMMAND_HDA_VERB_GET_PARAMETER,
                          COMMAND_HDA_PARAM_CONN_LIST_LEN);
    if (LenResp == 0xFFFFFFFF) return 0;

    Len      = LenResp & 0x7F;
    LongForm = (LenResp >> 7) & 1;
    Per      = LongForm ? 2 : 4;     /* entry per respons */

    for (i = 0; i < Len; i++)
    {
        if ((i % Per) == 0) {
            Resp = HDASendVerb(DevExt, Cad, Nid, COMMAND_HDA_VERB_GET_CONN_LIST, i);
        }

        if (LongForm) {
            Entry = (Resp >> ((i % 2) * 16)) & 0xFFFF;
            Range = Entry & 0x8000;
            Entry &= 0x7FFF;
        } else {
            Entry = (Resp >> ((i % 4) * 8)) & 0xFF;
            Range = Entry & 0x80;
            Entry &= 0x7F;
        }

        if (Range && Count > 0) {
            /* entry ini = ujung range; Prev = awal range */
            for (k = (ULONG)Prev + 1; k <= Entry && Count < MaxCount; k++) {
                List[Count++] = (UCHAR)k;
            }
        } else if (Count < MaxCount) {
            List[Count++] = (UCHAR)Entry;
        }
        Prev = (UCHAR)Entry;
    }
    return Count;
}

VEASTATUS
VEAPI
HDAEnumerateWidgets(IN PHDA_DEVICE_EXTENSION DevExt)
{
    UCHAR Cad = DevExt->PrimaryCodecAddress;
    ULONG Resp, i;
    UCHAR Start, Count;
    PHDA_WIDGET W;

    Resp = HDASendVerb(DevExt, Cad, DevExt->AfgNid,
                       COMMAND_HDA_VERB_GET_PARAMETER,
                       COMMAND_HDA_PARAM_SUB_NODE_COUNT);
    if (Resp == 0xFFFFFFFF) return STATUS_UNSUCCESSFUL;

    Start = (UCHAR)((Resp >> 16) & 0xFF);
    Count = (UCHAR)(Resp & 0xFF);
    if (Count > HDA_MAX_WIDGETS) Count = HDA_MAX_WIDGETS;

    DevExt->WidgetStart = Start;
    DevExt->WidgetCount = Count;

    for (i = 0; i < Count; i++)
    {
        W = &DevExt->Widgets[i];
        RtlZeroMemory(W, sizeof(HDA_WIDGET));

        W->Nid  = (UCHAR)(Start + i);
        W->Caps = HDASendVerb(DevExt, Cad, W->Nid,
                              COMMAND_HDA_VERB_GET_PARAMETER,
                              COMMAND_HDA_PARAM_WIDGET_CAPS);
        W->Type = (UCHAR)((W->Caps >> 20) & 0x0F);

        if (W->Type == HDA_WTYPE_PIN) {
            W->PinCaps   = HDASendVerb(DevExt, Cad, W->Nid,
                                       COMMAND_HDA_VERB_GET_PARAMETER,
                                       COMMAND_HDA_PARAM_PIN_CAPS);
            W->PinConfig = HDASendVerb(DevExt, Cad, W->Nid,
                                       COMMAND_HDA_VERB_GET_CONFIG_DEFAULT, 0);
        }

        if (W->Caps & HDA_WCAP_CONN_LIST) {
            W->ConnCount = (UCHAR)HDAGetConnList(DevExt, W->Nid, W->Conn, HDA_MAX_CONN);
        }

        DPRINT("HDA: NID 0x%02X type=%u caps=%08X pincaps=%08X pincfg=%08X conns=%u\n",
               W->Nid, W->Type, W->Caps, W->PinCaps, W->PinConfig, W->ConnCount);
    }
    return STATUS_SUCCESS;
}

static BOOLEAN
HDASearchPath(
    IN PHDA_DEVICE_EXTENSION DevExt,
    IN UCHAR Nid,
    IN ULONG Depth,
    OUT PUCHAR Path,
    OUT PUCHAR Sel,
    OUT PULONG PathLen
)
{
    PHDA_WIDGET W = HDAGetWidget(DevExt, Nid);
    ULONG i;

    if (!W || Depth >= HDA_MAX_PATH) return FALSE;
    if (Depth > 0 && W->Type == HDA_WTYPE_PIN) return FALSE;   /* pin lain bukan jalur */

    Path[Depth] = Nid;

    if (W->Type == HDA_WTYPE_AUDIO_OUT) {
        *PathLen = Depth + 1;
        return TRUE;
    }

    for (i = 0; i < W->ConnCount; i++) {
        if (HDASearchPath(DevExt, W->Conn[i], Depth + 1, Path, Sel, PathLen)) {
            Sel[Depth] = (UCHAR)i;
            return TRUE;
        }
    }
    return FALSE;
}

VEASTATUS
VEAPI
HDAFindOutputPath(IN PHDA_DEVICE_EXTENSION DevExt)
{
    ULONG Pass, i, Conn, DefDev;
    PHDA_WIDGET W;

    /* Pass 0: pin line-out / speaker / headphone. Pass 1: pin output apa saja yang terhubung */
    for (Pass = 0; Pass < 2; Pass++)
    {
        for (i = 0; i < DevExt->WidgetCount; i++)
        {
            W = &DevExt->Widgets[i];
            if (W->Type != HDA_WTYPE_PIN) continue;
            if (!(W->PinCaps & HDA_PINCAP_OUTPUT)) continue;

            Conn = (W->PinConfig >> 30) & 0x3;
            if (Conn == 1) continue;                    /* 1 = tidak ada koneksi fisik */

            DefDev = (W->PinConfig >> 20) & 0x0F;       /* 0=line out, 1=speaker, 2=HP out */
            if (Pass == 0 && DefDev > 2) continue;

            if (HDASearchPath(DevExt, W->Nid, 0, DevExt->OutPath,
                              DevExt->OutPathSel, &DevExt->OutPathLen))
            {
                DevExt->PinWidgetNID   = W->Nid;
                DevExt->AudioOutputNID = DevExt->OutPath[DevExt->OutPathLen - 1];

                DPRINT("HDA: Output path found: pin 0x%02X (dev=%u) -> DAC 0x%02X, %u node\n",
                       DevExt->PinWidgetNID, DefDev, DevExt->AudioOutputNID,
                       DevExt->OutPathLen);
                return STATUS_SUCCESS;
            }
        }
    }

    DPRINT("HDA: No output path found!\n");
    return STATUS_UNSUCCESSFUL;
}

static ULONG
HDAGetAmpOffset(IN PHDA_DEVICE_EXTENSION DevExt, IN PHDA_WIDGET W, IN BOOLEAN Output)
{
    UCHAR Nid = (W->Caps & HDA_WCAP_AMP_OVERRIDE) ? W->Nid : DevExt->AfgNid;
    ULONG Caps = HDASendVerb(DevExt, DevExt->PrimaryCodecAddress, Nid,
                             COMMAND_HDA_VERB_GET_PARAMETER,
                             Output ? COMMAND_HDA_PARAM_OUTPUT_AMP_CAPS
                                    : COMMAND_HDA_PARAM_INPUT_AMP_CAPS);
    if (Caps == 0xFFFFFFFF) return 0;
    return Caps & 0x7F;
}

VEASTATUS
VEAPI
HDASetupOutputPath(IN PHDA_DEVICE_EXTENSION DevExt)
{
    UCHAR Cad = DevExt->PrimaryCodecAddress;
    ULONG d, Last, Offset, PinCtl, DefDev;
    PHDA_WIDGET W;

    if (DevExt->OutPathLen == 0) return STATUS_UNSUCCESSFUL;
    Last = DevExt->OutPathLen - 1;

    for (d = 0; d <= Last; d++)
    {
        W = HDAGetWidget(DevExt, DevExt->OutPath[d]);
        if (!W) continue;

        /* 1. Power D0 */
        if (W->Caps & HDA_WCAP_POWER_CTRL) {
            HDASendVerb(DevExt, Cad, W->Nid, COMMAND_HDA_VERB_SET_POWER_STATE, 0x00);
        }

        /* 2. Node non-DAC: pilih input yang menuju DAC + unmute input amp-nya */
        if (d < Last)
        {
            if (W->Type != HDA_WTYPE_MIXER && W->ConnCount > 1) {
                HDASendVerb(DevExt, Cad, W->Nid, COMMAND_HDA_VERB_SET_CONN_SELECT,
                            DevExt->OutPathSel[d]);
            }

            if (W->Caps & HDA_WCAP_IN_AMP) {
                Offset = HDAGetAmpOffset(DevExt, W, FALSE);
                HDASendCommand(DevExt, COMMAND_HDA_SET_AMP_GAIN(Cad, W->Nid,
                    COMMAND_HDA_AMP_SET_INPUT | COMMAND_HDA_AMP_SET_LEFT |
                    COMMAND_HDA_AMP_SET_RIGHT |
                    COMMAND_HDA_AMP_INDEX(DevExt->OutPathSel[d]) |
                    COMMAND_HDA_AMP_GAIN(Offset)));
            }
        }

        /* 3. Output amp (termasuk DAC): unmute, gain 0 dB */
        if (W->Caps & HDA_WCAP_OUT_AMP) {
            Offset = HDAGetAmpOffset(DevExt, W, TRUE);
            HDASendCommand(DevExt, COMMAND_HDA_SET_AMP_GAIN(Cad, W->Nid,
                COMMAND_HDA_AMP_SET_OUTPUT | COMMAND_HDA_AMP_SET_LEFT |
                COMMAND_HDA_AMP_SET_RIGHT | COMMAND_HDA_AMP_INDEX(0) |
                COMMAND_HDA_AMP_GAIN(Offset)));
        }

        /* 4. Pin: aktifkan output, EAPD kalau ada */
        if (W->Type == HDA_WTYPE_PIN)
        {
            DefDev = (W->PinConfig >> 20) & 0x0F;
            PinCtl = HDA_PINCTL_OUT_EN;
            if (DefDev == 2 && (W->PinCaps & HDA_PINCAP_HP_DRIVE)) {
                PinCtl |= HDA_PINCTL_HP_EN;
            }
            HDASendVerb(DevExt, Cad, W->Nid, COMMAND_HDA_VERB_SET_PIN_CTRL, PinCtl);

            if (W->PinCaps & HDA_PINCAP_EAPD) {
                HDASendVerb(DevExt, Cad, W->Nid, COMMAND_HDA_VERB_SET_EAPD, 0x02);
            }
        }
    }

    DPRINT("HDA: Output path enabled (pin 0x%02X -> DAC 0x%02X)\n",
           DevExt->PinWidgetNID, DevExt->AudioOutputNID);
    return STATUS_SUCCESS;
}

static VEASTATUS
HDAResetStream(IN PUCHAR Sd)
{
    ULONG Timeout;

    /* Stop DMA */
    WRITE_REGISTER_UCHAR(Sd + HDA_SD_CTL, 0);
    for (Timeout = 1000; Timeout; Timeout--) {
        if (!(READ_REGISTER_UCHAR(Sd + HDA_SD_CTL) & HDA_SD_CTL_RUN)) break;
        KsStallExecutionProcessor(10);
    }

    /* Masuk reset: tulis SRST, tunggu terbaca 1 */
    WRITE_REGISTER_UCHAR(Sd + HDA_SD_CTL, HDA_SD_CTL_SRST);
    for (Timeout = 1000; Timeout; Timeout--) {
        if (READ_REGISTER_UCHAR(Sd + HDA_SD_CTL) & HDA_SD_CTL_SRST) break;
        KsStallExecutionProcessor(10);
    }
    if (!Timeout) return STATUS_UNSUCCESSFUL;

    /* Keluar reset: tulis 0, tunggu terbaca 0 */
    WRITE_REGISTER_UCHAR(Sd + HDA_SD_CTL, 0);
    for (Timeout = 1000; Timeout; Timeout--) {
        if (!(READ_REGISTER_UCHAR(Sd + HDA_SD_CTL) & HDA_SD_CTL_SRST)) break;
        KsStallExecutionProcessor(10);
    }
    if (!Timeout) return STATUS_UNSUCCESSFUL;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
HDASetupOutputStream(IN PHDA_DEVICE_EXTENSION DevExt)
{
    UCHAR Cad = DevExt->PrimaryCodecAddress;
    PUCHAR Sd = DevExt->HdaMMIOBase + DevExt->StreamDescriptorOffset;  /* output stream #0 */
    PHDA_BDL_ENTRY Bdl;
    ULONG Chunk = HDA_STREAM_BUF_SIZE / HDA_BDL_ENTRIES;
    ULONG i, Ctl, Rates;
    VEASTATUS Status;

    if (DevExt->NumOutputStreams == 0) {
        DPRINT("HDA: Controller has no output stream!\n");
        return STATUS_UNSUCCESSFUL;
    }

    DevExt->StreamTag        = 1;
    DevExt->StreamFormat     = HDA_FMT_48K_16BIT_STEREO;
    DevExt->StreamBufferSize = HDA_STREAM_BUF_SIZE;

    /* Cek DAC mendukung 48 kHz / 16-bit (rates bit 6 = 48 kHz, bit 17 = 16-bit) */
    Rates = HDASendVerb(DevExt, Cad, DevExt->AudioOutputNID,
                        COMMAND_HDA_VERB_GET_PARAMETER,
                        COMMAND_HDA_PARAM_PCM_SIZE_RATE);
    DPRINT("HDA: DAC PCM size/rate = %08X (bit6 = 48kHz, bit17 = 16-bit)\n", Rates);

    /* Buffer audio */
    DevExt->StreamBuffer = MmAllocateContiguousMemory(HDA_STREAM_BUF_SIZE,
                               (PHYSICAL_ADDRESS){.QuadPart = 0xFFFFFFFF});
    if (!DevExt->StreamBuffer) {
        DPRINT("HDA: Failed to allocate stream buffer\n");
        return STATUS_INSUFFICIENT_MEMORY;
    }
    RtlZeroMemory(DevExt->StreamBuffer, HDA_STREAM_BUF_SIZE);
    DevExt->StreamBufferPhysical = MmGetPhysicalAddress(DevExt->StreamBuffer);

    /* BDL (harus 128-byte aligned) */
    DevExt->BdlBuffer = MmAllocateContiguousMemory(HDA_BDL_ENTRIES * sizeof(HDA_BDL_ENTRY),
                            (PHYSICAL_ADDRESS){.QuadPart = 0xFFFFFFFF});
    if (!DevExt->BdlBuffer) {
        DPRINT("HDA: Failed to allocate BDL\n");
        return STATUS_INSUFFICIENT_MEMORY;
    }
    RtlZeroMemory(DevExt->BdlBuffer, HDA_BDL_ENTRIES * sizeof(HDA_BDL_ENTRY));
    DevExt->BdlPhysical = MmGetPhysicalAddress(DevExt->BdlBuffer);

    DPRINT("HDA: Stream buf virt=%p phys=%08X, BDL virt=%p phys=%08X\n",
           DevExt->StreamBuffer, DevExt->StreamBufferPhysical.LowPart,
           DevExt->BdlBuffer, DevExt->BdlPhysical.LowPart);

    if (DevExt->BdlPhysical.LowPart & 0x7F) {
        DPRINT("HDA: BDL not 128-byte aligned!\n");
        return STATUS_UNSUCCESSFUL;
    }

    /* Isi BDL: 4 entry x 4 KB */
    Bdl = (PHDA_BDL_ENTRY)DevExt->BdlBuffer;
    for (i = 0; i < HDA_BDL_ENTRIES; i++) {
        Bdl[i].AddrLow  = DevExt->StreamBufferPhysical.LowPart + (i * Chunk);
        Bdl[i].AddrHigh = DevExt->StreamBufferPhysical.HighPart;
        Bdl[i].Length   = Chunk;
        Bdl[i].Flags    = HDA_BDL_IOC;
    }

    /* Reset stream descriptor */
    Status = HDAResetStream(Sd);
    if (!VEA_SUCCESS(Status)) {
        DPRINT("HDA: Stream reset timeout!\n");
        return Status;
    }
    WRITE_REGISTER_UCHAR(Sd + HDA_SD_STS, 0x1C);    /* clear BCIS | FIFOE | DESE */

    /* Stream tag di SDnCTL bit 23:20 (status byte 31:24 dimasker supaya tidak ikut ter-clear) */
    Ctl = READ_REGISTER_ULONG((PULONG)(Sd + HDA_SD_CTL));
    Ctl = (Ctl & 0x000FFFFF) | ((ULONG)DevExt->StreamTag << 20);
    WRITE_REGISTER_ULONG((PULONG)(Sd + HDA_SD_CTL), Ctl);

    WRITE_REGISTER_ULONG ((PULONG) (Sd + HDA_SD_CBL),  HDA_STREAM_BUF_SIZE);
    WRITE_REGISTER_USHORT((PUSHORT)(Sd + HDA_SD_LVI),  HDA_BDL_ENTRIES - 1);
    WRITE_REGISTER_USHORT((PUSHORT)(Sd + HDA_SD_FMT),  DevExt->StreamFormat);
    WRITE_REGISTER_ULONG ((PULONG) (Sd + HDA_SD_BDPL), DevExt->BdlPhysical.LowPart);
    WRITE_REGISTER_ULONG ((PULONG) (Sd + HDA_SD_BDPU), DevExt->BdlPhysical.HighPart);

    /* Sisi codec: format DAC + stream tag (channel 0) */
    HDASendCommand(DevExt, COMMAND_HDA_SET_CONV_FORMAT(Cad, DevExt->AudioOutputNID,
                                                       DevExt->StreamFormat));
    HDASendCommand(DevExt, COMMAND_HDA_SET_STREAM_CHAN(Cad, DevExt->AudioOutputNID,
                                                       DevExt->StreamTag, 0));

    DPRINT("HDA: Output stream configured (tag %u, fmt 0x%04X, DAC 0x%02X)\n",
           DevExt->StreamTag, DevExt->StreamFormat, DevExt->AudioOutputNID);
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
HDAStartOutputStream(IN PHDA_DEVICE_EXTENSION DevExt)
{
    PUCHAR Sd = DevExt->HdaMMIOBase + DevExt->StreamDescriptorOffset;
    UCHAR Ctl;

    WRITE_REGISTER_UCHAR(Sd + HDA_SD_CTL,
        (UCHAR)(READ_REGISTER_UCHAR(Sd + HDA_SD_CTL) | HDA_SD_CTL_RUN | HDA_SD_CTL_IOCE));
    DevExt->StreamRunning = TRUE;
    return STATUS_SUCCESS;
}

VOID
VEAPI
HDAStopOutputStream(IN PHDA_DEVICE_EXTENSION DevExt)
{
    PUCHAR Sd = DevExt->HdaMMIOBase + DevExt->StreamDescriptorOffset;
    UCHAR Ctl;

    Ctl = READ_REGISTER_UCHAR(Sd + HDA_SD_CTL);
    Ctl &= ~(HDA_SD_CTL_RUN | HDA_SD_CTL_IOCE);
    WRITE_REGISTER_UCHAR(Sd + HDA_SD_CTL, Ctl);
    DevExt->StreamRunning = FALSE;
}

static const USHORT HdaNotes[8] = { 262, 294, 330, 392, 440, 523, 587, 659 };  /* pentatonik-ish */

VOID
VEAPI
HDAGenerateAudio(
    IN PHDA_DEVICE_EXTENSION DevExt,
    IN ULONG ByteOffset,
    IN ULONG ByteLength
)
{
    short *Out = (short *)((PUCHAR)DevExt->StreamBuffer + ByteOffset);
    ULONG Frames = ByteLength / 4;      /* 16-bit stereo = 4 byte/frame */
    ULONG i, x;
    short S;

    for (i = 0; i < Frames; i++)
    {
        /* Ganti nada acak tiap 8192 frame (~170 ms) */
        if (DevExt->ToneFramesLeft == 0) {
            DevExt->Rng ^= DevExt->Rng << 13;       /* xorshift32 */
            DevExt->Rng ^= DevExt->Rng >> 17;
            DevExt->Rng ^= DevExt->Rng << 5;
            DevExt->ToneStep = HdaNotes[(DevExt->Rng >> 8) & 7] * 89478UL;  /* 2^32 / 48000 */
            DevExt->ToneFramesLeft = 8192;
        }
        DevExt->ToneFramesLeft--;

        DevExt->TonePhase += DevExt->ToneStep;      /* wrap alami di 2^32 */

        /* Gelombang segitiga dari phase 32-bit */
        x = DevExt->TonePhase >> 16;                /* 0..65535 */
        if (x >= 32768) x = 65535 - x;              /* 0..32767 */
        S = (short)(((long)x - 16384) / 2);         /* kira-kira +-8192 */

        Out[i * 2]     = S;     /* L */
        Out[i * 2 + 1] = S;     /* R */
    }
}

VOID
VEAPI
HDAFillTestTone(IN PHDA_DEVICE_EXTENSION DevExt)
{
    DevExt->Rng            = 0x1234ABCDUL;  /* seed xorshift, tidak boleh 0 */
    DevExt->TonePhase      = 0;
    DevExt->ToneStep       = 0;
    DevExt->ToneFramesLeft = 0;
    DevExt->FillChunk      = 0;

    HDAGenerateAudio(DevExt, 0, HDA_STREAM_BUF_SIZE);   /* prefill 4 chunk */
}