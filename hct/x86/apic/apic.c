/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : apic.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Included with APIC Function
 ----- Effective since 07-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>
#include "apic.h"

/* Revision History ------------------------------------------------------
 * DATE       : 25-07-2026
 * NAME       : Rasya Fadhillah R
 * REVISION   : Creating file apic.c
 * --------------------------------------------------------------------- */

// FUNCTION

VOID
VEAPI
ApicEndSystemInterrupt(VOID)
{
    ApicWrite(APIC_EOI, 0);
}

VOID
VEAPI
ApicSendSelfIpi(UCHAR Vector)
{
    //
    // Konfigurasi LAPIC ICR:
    // Destination Shorthand = Self (0x01 << 18 = 0x40000)
    // Level = Assert (1 << 14 = 0x4000)
    // Delivery Mode = Fixed (0)
    // Trigger Mode = Edge (0)
    //
    ULONG IcrValue = 0x44000 | Vector;

    // Tulis ke ICR Low (Otomatis menembakkan Self-IPI)
    ApicWrite(APIC_REG_ICR_LOW, IcrValue);
}

VOID
VEAPI
ApicRequestSoftwareInterrupt(IN UCHAR Vector)
{
    ULONG IcrLow;

    /*
     * Konfigurasi ICR (Interrupt Command Register) untuk Self-IPI:
     * - Vector                : Bit 0-7  (Vector target)
     * - Delivery Mode         : Bit 8-10 (000b = Fixed)
     * - Level                 : Bit 14   (1b   = Assert)
     * - Destination Shorthand : Bit 18-19(01b  = Self)
     */
    IcrLow = APIC_ICR_SHORTHAND_SELF | APIC_ICR_LEVEL_ASSERT | (ULONG)Vector;

    // Tulis register ICR High terlebih dahulu (Destination = 0 untuk Shorthand Self)
    ApicWrite(APIC_REG_ICR_HIGH, 0);

    // Tulis register ICR Low untuk memicu Self-IPI di LAPIC saat ini
    ApicWrite(APIC_REG_ICR_LOW, IcrLow);
}

