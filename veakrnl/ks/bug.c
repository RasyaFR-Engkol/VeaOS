#include <veakrnl.h>

// simple bugcheck

VOID
VEAPI
KsBugCheckEx(
    ULONG BugCheckCode,
    ULONG_PTR BugCheckParameter1,
    ULONG_PTR BugCheckParameter2,
    ULONG_PTR BugCheckParameter3,
    ULONG_PTR BugCheckParameter4
)
{
    CHAR Buffer[256];
    
    // Pastikan buffer bersih sebelum diformat
    RtlZeroMemory(Buffer, sizeof(Buffer));
    
    // Format pesan ala BSOD (Blue Screen of Death)
    // Pakai %08X untuk mencetak angka/pointer dalam format Hexadecimal 32-bit
    RtlStringCbPrintfA(Buffer, sizeof(Buffer), 
        "\n\r"
        "===============================================================\n\r"
        "*** FATAL SYSTEM ERROR ***\n\r"
        "A critical error has occurred and the system has been halted.\n\r"
        "\n\r"
        "STOP: 0x%X (0x%X, 0x%X, 0x%X, 0x%X)\n\r"
        "===============================================================\n\r",
        BugCheckCode,
        BugCheckParameter1,
        BugCheckParameter2,
        BugCheckParameter3,
        BugCheckParameter4
    );

    // Kirim ke debugger atau serial port
    kdp_print(Buffer);

    __asm__ volatile("cli"); // Matikan interupsi
    
    while (1) {
        __asm__ volatile("hlt"); // Tidurkan prosesor dalam infinite loop
    }
}

VOID
VEAPI
KsBugCheck(
    ULONG BugCheckCode,
    ULONG_PTR BugCheckParameter1,
    ULONG_PTR BugCheckParameter2,
    ULONG_PTR BugCheckParameter3,
    ULONG_PTR BugCheckParameter4
)
{
    // Teruskan langsung ke versi Ex
    KsBugCheckEx(
        BugCheckCode, 
        BugCheckParameter1, 
        BugCheckParameter2, 
        BugCheckParameter3, 
        BugCheckParameter4
    );
}