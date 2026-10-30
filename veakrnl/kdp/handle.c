/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : handle.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Handling Serial command task
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 12-09-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file handle.c
 * --------------------------------------------------------------------- */

VOID
VEAPI
KdpDumpRegisters(PKREGISTER_FRAME Frame)
{
    if (!Frame)
    {
        KdPrintf("[KDP] Register frame not available.\n");
        return;
    }

    KdPrintf("\n=== CPU REGISTER STATE ===\n");
    // Format 64-bit (sesuaikan tipe data di PKREGISTER_FRAME VeaOS)
    KdPrintf(" EAX: 0x%X  EBX: 0x%X\n", Frame->Eax, Frame->Ebx);
    KdPrintf(" ECX: 0x%X  EDX: 0x%X\n", Frame->Ecx, Frame->Edx);
    KdPrintf(" ESI: 0x%X  EDI: 0x%X\n", Frame->Esi, Frame->Edi);
    KdPrintf(" EBP: 0x%X  ESP: 0x%X\n", Frame->Ebp, Frame->Esp);
    KdPrintf(" EIP: 0x%X  EFLAGS: 0x%08X\n", Frame->Eip, Frame->Eflags);
    KdPrintf(" CS : 0x%04X  SS: 0x%04X  DS: 0x%04X  ES: 0x%04X\n", 
             Frame->Cs, Frame->HardwareSs, Frame->Ds, Frame->Es);
    KdPrintf("==========================\n\n");
}

BOOLEAN
VEAPI
KdpProcessInput(
    PCHAR String,
    PKREGISTER_FRAME Frame
)
{
    /* Continue */
    if (RtlRawStringCompareN(String, "ok", 2) == 0 || 
        RtlRawStringCompareN(String, "c", 1) == 0)
    {
        if (KdpLastStatus == DEBUG_BREAK_BUG_CHECK)
        {
            KdPrintf("Debugger error: Can't proceed after bug check.\n");
            return FALSE;
        }
        
        return TRUE; 
    }

    /* Regs */
    if (RtlRawStringCompareN(String, "regs", 4) == 0)
    {
        KdpDumpRegisters(Frame);
        return FALSE; 
    }

    /* Helpme */
    if (RtlRawStringCompareN(String, "helpme", 6) == 0)
    {
        KdPrintf("\nVeaKDProtocol Commands:\n");
        KdPrintf("  ok / c   - Resume execution\n");
        KdPrintf("  regs     - Dump CPU registers state\n");
        KdPrintf("  helpme   - Show this help message\n");
        KdPrintf("  reboot   - Cold reboot this machine\n");
        KdPrintf("  dump     - Dumping usage help\n");
        return FALSE;
    }

    /* Reboot (TODO: Put this at debugging power utility)*/
    if (RtlRawStringCompareN(String, "reboot", 6) == 0)
    {
        /* TODO: We should call HctPowerRoutine. Maybe we could back via ACPI? */
        outb(0x64, 0xFE);

        return FALSE;
    }

    /* Dump */
    if (RtlRawStringCompareN(String, "dump", 4) == 0)
    {
        KdPrintf("DumpInfo: \n");
        KdPrintf("Use dump to dump some info about Kernel Subsystem\n");
        KdPrintf("e.g \"Pt dump process\" or \"Ob dump lifecycle\"\n");
         
        return FALSE;
    }

    /* Lets put the rest to subsystem dumping */
    if (!KdpSubsystemDump(String))
    {
        goto done;
    }
    else
    {
        return FALSE;
    }
    
done:
    KdPrintf("VeaKDProtocol: Unknown command '%s'. Type 'helpme' for help.\n", String);
    return FALSE;
}

VOID
VEAPI
KdpMailboxDelayMessage(PCHAR String)
{
    ULONG StringLength;
    ULONG MustInsert;

    /* Make sure it isn't full */
    if (KdpPendingMessageIndex >= 256)
    {
        /* Its full. return */
        return;
    }

    /* Count on string length */
    StringLength = RtlRawStringLength(String);
    if (StringLength == 0)
    {
        return;
    }

    /* String is valid. Mark it */
    KdpTheresPendingMesssage = TRUE;

    /* Maybe can overflow index? */
    if (StringLength + KdpPendingMessageIndex > 250)
    {
        /* count how much we cap to insert */
        MustInsert = MAXIMUM_PENDING_MESSAGE_DEBUGGER - KdpPendingMessageIndex;
    }
    else
    {
        /* Insert all */
        MustInsert = StringLength;
    }
    
    /* Insert all char */
    RtlCopyMemory(KdpPendingMessage, String, MustInsert);
    KdpPendingMessageIndex += MustInsert;

    /* End It */
    KdpPendingMessage[KdpPendingMessageIndex] = '\0';
}

VOID
VEAPI
KdpAssertFailedDebug(
   const PCHAR Expression,
   const PCHAR File, 
   const PCHAR Function, 
   ULONG Line
)
{
   CHAR StackString[KDP_MAXIMUM];

   /* We can't use Memory after assertation because we can't trust it again */
   RtlZeroMemory(StackString, 256);

   /* RtlString */
   RtlStringCbPrintfA(
      StackString,
      KDP_MAXIMUM,
      " ** ASSERTATION FAILED ** \n"
              " EXPRESSION: %s \n\n"
              " ASSERTATION FAILED AT \n"
              " FILE: %s \n"
              " FUNCTION: %s \n"
              " LINE: %d \n\n"
              " There is a risk of system failing if you continue. You're here because"
              " of some reason.\n\n",
   Expression, File, Function, Line);

   /* Pass it to mailbox */
   KdpMailboxDelayMessage(StackString);

   /* TRIGGER IT NOW */
   __asm__ __volatile__ ("int $3");
}

VOID
KdpDebugMailboxPrint(
   PCHAR String,
   ...
)
{
    va_list args;
    va_start(args, String);

    CHAR StackString[KDP_MAXIMUM];

    /* We can't use Memory after assertation because we can't trust it again */
    RtlZeroMemory(StackString, 256);

    /* RtlString */
    RtlStringCbPrintfAImpl(
        StackString,
        KDP_MAXIMUM,
        String,
        args
    );

    va_end(args);

   /* Pass it to mailbox */
    KdpMailboxDelayMessage(StackString);

    /* TRIGGER IT NOW */
    __asm__ __volatile__ ("int $3");
}