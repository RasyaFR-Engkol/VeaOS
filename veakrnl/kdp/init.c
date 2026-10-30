/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : init.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Serial Initialization and Handling
 ----- Effective since 09-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : ??-??-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file init.c
 * --------------------------------------------------------------------- */

/* Pending message only for assertation now */
CHAR KdpPendingMessage[MAXIMUM_PENDING_MESSAGE_DEBUGGER];
BOOLEAN KdpTheresPendingMesssage = FALSE;
ULONG KdpPendingMessageIndex = 0;
BOOLEAN KdpPresent = FALSE;

VOID
VEAPI
kdp_activate_serial(void)
{
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x01); 
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 4, 0x0B);

    KdpPresent = TRUE;
}

VOID 
VEAPI
kdp_serial_putchar(CHAR c) 
{
    // Tunggu sampai Transmitter Holding Register kosong (bit 5 nyala)
    while ((inb(COM1_PORT + 5) & 0x20) == 0) {
        // CPU nunggu (bisa dikasih instruksi 'pause' di x86 buat efisiensi)
        __asm__ volatile("pause");
    }
    // Tembak karakternya ke port data!
    outb(COM1_PORT, c);
}

INT 
VEAPI
KdpSerialReceived(VOID)
{
    return inb(COM1_PORT + 5) & 1;
}

CHAR
VEAPI
KdpReadSerial(VOID)
{
    while (KdpSerialReceived() == 0)
    {
        __asm__ __volatile__ ("pause");
    }
    return inb(COM1_PORT + 0);
}

VOID
VEAPI
kdp_print(char *string)
{
    while(*string != '\0')
    {
        if (*string == '\n') 
        {
            kdp_serial_putchar('\r');
        }
        
        kdp_serial_putchar(*string);
        string++;
    }
}

VOID
KdPrintf(const char *Format, ...)
{
    static char Buffer[512];
    va_list args;

    if (Format == NULL) {
        return;
    }

    va_start(args, Format);
    RtlStringCbPrintfAImpl(Buffer, sizeof(Buffer), Format, args);
    va_end(args);
    kdp_print(Buffer);
}

BOOLEAN
VEAPI
KdpHandleDebug(PKREGISTER_FRAME RegisterFrame)
{
    KdPrintf("\n\n");
    KdPrintf("===============================================\n"
                     "              VeaOS Debugger v1.0              \n"
                     "===============================================\n"
                     "\n"
                     " This is VeaOS Debugger ver 1.0. If you're\n"
                     " thrown here accidentaly, just type ok then\n"
                     " press ENTER.\n\n");

    /* If there is a delay message box */
    if (KdpTheresPendingMesssage)
    {
        /* Print it first */
        KdPrintf(KdpPendingMessage);

        /* Reset all buffer message */
        RtlZeroMemory(KdpPendingMessage, MAXIMUM_PENDING_MESSAGE_DEBUGGER);
        KdpPendingMessageIndex = 0;
    }

    /* KdpHandleDebug only accepting input from serial */
    while (TRUE)
    {
        CHAR DebuggerInput[MAXIMUM_INPUT_DEBUGGER];
        ULONG DebuggerInputIndex = 0;

        /* Zero out every stack memory trash */
        RtlZeroMemory(DebuggerInput, MAXIMUM_INPUT_DEBUGGER);

        /* Print terminal debugger */
        KdPrintf("VeaKDProtocol > ");

        /* Echo every character to serial, and then add input from user
           to DebuggerInput */
        while (TRUE)
        {
            CHAR Input = KdpReadSerial();

            /* Check if it's ENTER */
            if (Input == '\n' || Input == '\r')
            {
                /* End it */
                DebuggerInput[DebuggerInputIndex] = '\0';
                KdPrintf("\n\n");
                break;
            }

            /* Check if it's backspace */
            if (Input == '\b' || Input == 0x7F)
            {
                if (DebuggerInputIndex > 0)
                {
                    DebuggerInputIndex--;
                    KdPrintf("\b \b");
                }
                continue;
            }

            /* Make sure isn't buffer overflow */
            if (DebuggerInputIndex < (MAXIMUM_INPUT_DEBUGGER - 1))
            {
                DebuggerInput[DebuggerInputIndex++] = Input;
                kdp_serial_putchar(Input);
            }
        }

        /* Make sure there is command */
        if (DebuggerInputIndex > 0)
        {
            BOOLEAN ShouldResume = FALSE;
            ShouldResume = KdpProcessInput(DebuggerInput, RegisterFrame);
            KdPrintf("\n");

            if (ShouldResume)
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}