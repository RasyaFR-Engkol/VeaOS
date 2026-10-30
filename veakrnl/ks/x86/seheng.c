/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   FILE        : seheng3.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Hardware Trap EIP Rewiring Exception Dispatcher
 ----- Effective since 10-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

BOOLEAN
VEAPI
KsiDispatchSehException(
    IN PKTRAP_FRAME TrapFrame,
    IN VEASTATUS ExceptionCode
)
{
    PVEA_SEH_REGISTRATION_FRAME Frame = VeaSehGetFrameHead();

    while (Frame != NULL && Frame != (PVOID)0xFFFFFFFF)
    {
        if (Frame->TargetEip != NULL)
        {
            Frame->ExceptionCode = ExceptionCode;
            Frame->AbnormalTermination = TRUE;

            if (TrapFrame != NULL)
            {
                // Simpan alamat instruksi CPU yang fault buat skenario CONTINUE_EXECUTION
                Frame->FaultEip = TrapFrame->Eip;

                // Unregister frame ini dari FS:[0] sebelum me-rewire EIP
                VeaSehSetFrameHead(Frame->Next);

                // Rewire Trap Frame EIP ke landing pad filter
                TrapFrame->Eip = (ULONG)Frame->TargetEip;
                return TRUE;
            }
            else
            {
                // Direct Jump Unwind dari CONTINUE_SEARCH atau FINALLY
                ULONG TargetEip = (ULONG)Frame->TargetEip;
                ULONG SavedEsp = Frame->SavedEsp;
                ULONG SavedEbp = Frame->SavedEbp;

                VeaSehSetFrameHead(Frame->Next);

                asm volatile (
                    "movl %0, %%esp\n\t"
                    "movl %1, %%ebp\n\t"
                    "jmp *%2\n\t"
                    :
                    : "r"(SavedEsp), "r"(SavedEbp), "r"(TargetEip)
                    : "memory"
                );
            }
        }

        Frame = Frame->Next;
    }

    if (TrapFrame == NULL)
    {
        KsBugCheckEx(KMODE_EXCEPTION_NOT_HANDLED, ExceptionCode, 0, 0, 0);
    }

    return FALSE;
}
/* EOF */