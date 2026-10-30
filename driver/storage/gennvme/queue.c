/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : queue.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : NVMe Queue
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include "nvmeheader.h"

/* Revision History ------------------------------------------------------
 * DATE       : 07-10-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file queue.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
NVMeInitializeQueueState(
    IN PNVME_QUEUE Queue,
    IN USHORT QueueId,
    IN USHORT Depth
)
{
    RtlZeroMemory(Queue, sizeof(NVME_QUEUE));
 
    Queue->QueueId = QueueId;
    Queue->Depth = Depth;
    Queue->Phase = 1;           /* phase awal yang diharapkan di CQ */
    KsInitializeSpinlock(&Queue->Lock);
}
