/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : fcb.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Fcb 
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "veafat.h"

/* Revision History ------------------------------------------------------
 * DATE       : 17-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file fcb.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
FatReferenceFcb(
    IN PVEAFAT_FCB Fcb
)
{
    KIRQL OldIrql;
    KsAcquireSpinLock(&Fcb->Vcb->FcbListLock, &OldIrql);
    Fcb->RefCount++;
    KsReleaseSpinLock(&Fcb->Vcb->FcbListLock, OldIrql);
}

VOID
VEAPI
FatDereferenceFcb(
    IN PVEAFAT_FCB Fcb
)
{
    KIRQL OldIrql;
    PFAT32_DEVICE_EXTENSION Vcb = Fcb->Vcb;
    BOOLEAN ShouldFree = FALSE;

    KsAcquireSpinLock(&Vcb->FcbListLock, &OldIrql);
    Fcb->RefCount--;

    if (Fcb->RefCount == 0)
    {
        RemoveEntryList(&Fcb->FcbListEntry);
        ShouldFree = TRUE;
    }
    KsReleaseSpinLock(&Vcb->FcbListLock, OldIrql);

    if (ShouldFree)
    {
        if (Fcb->FileName.Buffer)
        {
            UlFreePoolWithTag(Fcb->FileName.Buffer, VEAFAT_POOL_TAG);
        }
        UlFreePoolWithTag(Fcb, VEAFAT_POOL_TAG);
    }
}