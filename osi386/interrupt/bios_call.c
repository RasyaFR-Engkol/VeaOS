#include <osi386.h>
#include <stdint.h>

void bios_print_char(char c) {
    // AH = 0x0E (Teletype Output), AL = Karakter
    uint32_t eax_in = (0x0E << 8) | c;
    // BH = 0 (Page 0), BL = 0x07 (Warna Light Gray)
    uint32_t ebx_in = 0x0007; 
    
    _INT10_CALL(eax_in, ebx_in);
}

void bios_print_string(const char* str) {
    while (*str) {
        bios_print_char(*str++);
    }
}

void set_vesa_lfb(uint16_t mode) {
    // AX = 0x4F02 (Set VBE Mode)
    uint32_t eax_in = 0x4F02;
    
    // Bit 14 (0x4000) WAJIB di-set buat ngasih tau BIOS kita mau pakai Linear Framebuffer
    // Contoh mode: 0x0118 (1024x768x24) -> Jadinya: 0x4118
    uint32_t ebx_in = mode | 0x4000;
    
    uint32_t status = _INT10_CALL(eax_in, ebx_in);
    
    // VESA balikin status di register AX (16-bit bawah EAX).
    // Kalau sukses, nilainya wajib 0x004F!
    if ((status & 0xFFFF) != 0x004F) {
        bios_print_string("Gagal ganti mode VESA!\r\n");
    }
}

char wait_for_keypress(void)
{
    uint32_t status = _INT16_CALL(0x0000);
    return (char)(status & 0xFF);
}

SHORT
GetKeycode(VOID)
{
    ULONG Raw = _INT16_CALL(0x0000);
    UCHAR Ascii = (UCHAR)(Raw & 0xFF);
    UCHAR Scancode = (UCHAR)((Raw >> 8) & 0xFF);

    if(Ascii != 0x00 && Ascii != 0xE0)
    {
        return (USHORT)Ascii;
    }

    return (USHORT)(Scancode << 8);
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

void force_reboot() {
    outb(0x64, 0xFE);

    while(1);
}
