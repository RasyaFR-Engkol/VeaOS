#include <stdarg.h>
#include <veakrnl.h>

VOID
VEAPI
kdp_activate_serial(void)
{
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x01); // Divisor Latch Low Byte
    outb(COM1_PORT + 1, 0x00); // Divisor Latch High Byte
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 4, 0x0B);
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
    char Buffer[512];
    va_list args;

    if (Format == NULL) {
        return;
    }

    va_start(args, Format);
    RtlStringCbPrintfAImpl(Buffer, sizeof(Buffer), Format, args);
    va_end(args);
    kdp_print(Buffer);
}