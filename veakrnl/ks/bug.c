#include <veakrnl.h>

VOID 
VEAPI
KsDrawBlueScreen(
    ULONG BugCheckCode,
    ULONG_PTR BugCheckParameter1,
    ULONG_PTR BugCheckParameter2,
    ULONG_PTR BugCheckParameter3,
    ULONG_PTR BugCheckParameter4
)
{
    if(!BvDoWeHaveOwnership()) BvAcquireDisplayState(TRUE);

    BvAcquireDisplayState(TRUE);
    BvSetColorText(0xFFFFFFFF);
    BvSetColorBackground(0xFF000080);
    BvClearScreen();

    BvPrintLog(
        "\nAn Error has occured and VeaOS must be stopped to prevent"
        " a damage to your computer.\n\n"
    );

    /* TODO: Get an error code. Try with resource table */

    BvPrintLog(
        "If this is your PC, make sure your newly installed device isn't"
        " conflicting with your OS. Also make sure BIOS option doesn't conflict"
        " with OS configuration. Such as XMP or profiling\n\n"
    );

    BvPrintLog(
        "Mitigation steps to fix the error:\n"
        "1. Restore to last checkpoint where VeaOS run smoothly.\n"
        "2. Disable or revert any last changed settings in BIOS.\n"
        "3. Make sure your OS and BIOS up-to-date.\n"
        "4. Get VeaOS Disk Installer and choose Repair option.\n\n"
    );
    
    CHAR ErrorCode[128];
    RtlZeroMemory(ErrorCode, sizeof(ErrorCode));

    RtlStringCbPrintfA(
        ErrorCode, 
        sizeof(ErrorCode), 
        "*** STOP: 0x%x (0x%x 0x%x 0x%x 0x%x)",
        BugCheckCode,
        BugCheckParameter1,
        BugCheckParameter2,
        BugCheckParameter3,
        BugCheckParameter4
    );

    BvPrintLog(ErrorCode);
}

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

    KsDrawBlueScreen(
        BugCheckCode,
        BugCheckParameter1,
        BugCheckParameter2,
        BugCheckParameter3,
        BugCheckParameter4
    );

    __asm__ volatile("cli"); // Matikan interupsi
    
    while (1) {
        __asm__ volatile("hlt"); // Tidurkan prosesor dalam infinite loop
    }
}

VOID
VEAPI
KsBugCheck(
    ULONG BugCheckCode
)
{
    // Teruskan langsung ke versi Ex
    KsBugCheckEx(
        BugCheckCode, 
        0,
        0,
        0,
        0
    );
}