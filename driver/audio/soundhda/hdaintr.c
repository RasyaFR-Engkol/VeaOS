/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : hdaintr.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : HDA Interrupt
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veaddk.h>
#include "hdaheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 03-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file hdaintr.c
 * --------------------------------------------------------------------- */

BOOLEAN
VEAPI
HDAInterruptHandler(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
)
{
    PHDA_DEVICE_EXTENSION DevExt = (PHDA_DEVICE_EXTENSION)ServiceContext;
    PUCHAR Base;
    PUCHAR Sd;
    ULONG IntSts, i, Streams;
    UCHAR SdSts;

    if (!DevExt || !DevExt->HdaMMIOBase) return FALSE;
    Base = DevExt->HdaMMIOBase;

    DevExt->IrqTotal++;

    IntSts = READ_REGISTER_ULONG((PULONG)(Base + HDA_REG_INTSTS));
    DevExt->LastIntSts = IntSts;
    if (!(IntSts & HDA_INT_GIS)) {
        DevExt->IrqSpurious++;
        return FALSE;                       /* bukan HDA yang menaikkan IRQ ini */
    }

    /* Clear status SEMUA stream yang nyala, tidak bergantung StreamRunning */
    Streams = DevExt->NumInputStreams + DevExt->NumOutputStreams + DevExt->NumBidirStreams;
    for (i = 0; i < Streams && i < 30; i++) {
        if (!(IntSts & (1UL << i))) continue;

        Sd = Base + 0x80 + (i * 0x20);
        SdSts = READ_REGISTER_UCHAR(Sd + HDA_SD_STS);
        WRITE_REGISTER_UCHAR(Sd + HDA_SD_STS, (UCHAR)(SdSts & 0x1C));

        if (i == DevExt->NumInputStreams && (SdSts & HDA_SD_STS_BCIS)) {
            DevExt->IrqCount++;
            
            if (DevExt->BufferedEvent)
            {
                KsSetEvent(
                    DevExt->BufferedEvent,
                    FALSE
                );
            }
        }
    }

    if (IntSts & HDA_INT_CIS) {
        WRITE_REGISTER_UCHAR(Base + 0x5D, 0x05);    /* RIRBSTS */
        WRITE_REGISTER_UCHAR(Base + 0x4D, 0x01);    /* CORBSTS */
    }

    /* Pengaman: kalau GIS masih nyala setelah semua di-clear, matikan interrupt HDA */
    if (READ_REGISTER_ULONG((PULONG)(Base + HDA_REG_INTSTS)) & HDA_INT_GIS) {
        if (++DevExt->IrqStuck > 100) {
            WRITE_REGISTER_ULONG((PULONG)(Base + HDA_REG_INTCTL), 0);
        }
    } else {
        DevExt->IrqStuck = 0;
    }

    return TRUE;
}

VEASTATUS
VEAPI
HDAConnectInterrupt(IN PHDA_DEVICE_EXTENSION DevExt)
{
    if (DevExt->InterruptVector == 0) {
        DPRINT("HDA: No interrupt vector, staying in polling mode\n");
        return STATUS_UNSUCCESSFUL;
    }

    DevExt->InterruptIrql = HctConvertVectorToIrql(
        (UCHAR)HctIrqVectorToIsrVector(DevExt->InterruptVector));

    return ObioConnectInterrupt(
        &DevExt->InterruptObject,
        HDAInterruptHandler,
        DevExt,
        NULL,                       /* SpinLock, sama seperti di PS/2 */
        DevExt->InterruptVector,
        DevExt->InterruptIrql,
        DevExt->InterruptIrql,
        LevelSensitive,             /* PCI INTx level-triggered (PS/2 pakai Latched) */
        0);
}

VOID
VEAPI
HDAEnableInterrupts(IN PHDA_DEVICE_EXTENSION DevExt)
{
    /* GIE + stream output #0. CIE sengaja tidak diaktifkan karena CORB/RIRB masih polling. */
    WRITE_REGISTER_ULONG((PULONG)(DevExt->HdaMMIOBase + HDA_REG_INTCTL),
                         HDA_INT_GIE | (1UL << DevExt->NumInputStreams));
}

VOID
VEAPI
HDADisableInterrupts(IN PHDA_DEVICE_EXTENSION DevExt)
{
    WRITE_REGISTER_ULONG((PULONG)(DevExt->HdaMMIOBase + HDA_REG_INTCTL), 0);
}