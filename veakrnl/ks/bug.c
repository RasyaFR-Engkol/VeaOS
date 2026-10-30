#include <veakrnl.h>

VOID
VEAPI
KsGetAndPrintBugMessage(IN ULONG BugCheckCode)
{
    PCHAR BugMessage = NULL;

    switch (BugCheckCode)
    {
    case IRQL_NOT_GREATER_OR_EQUAL:
        BugMessage = "IRQL_NOT_GREATER_OR_EQUAL";
        break;

    case IRQL_NOT_LESS_OR_EQUAL:
        BugMessage = "IRQL_NOT_LESS_OR_EQUAL";
        break;

    case SPINLOCK_IS_ALREADY_OWNED:
        BugMessage = "SPINLOCK_IS_ALREADY_OWNED";
        break;

    case SPINLOCK_IS_NOT_OWNED:
        BugMessage = "SPINLOCK_IS_NOT_OWNED";
        break;

    case BAD_POOL_HEADER:
        BugMessage = "BAD_POOL_HEADER";
        break;

    case MEMORY_MANAGEMENT_ERROR:
        BugMessage = "MEMORY_MANAGEMENT_ERROR";
        break;

    case KMODE_EXCEPTION_NOT_HANDLED:
        BugMessage = "KMODE_EXCEPTION_NOT_HANDLED";
        break;

    case FAT_FILE_SYSTEM:
        BugMessage = "FAT_FILE_SYSTEM";
        break;

    case NTFS_FILE_SYSTEM:
        BugMessage = "NTFS_FILE_SYSTEM";
        break;

    case DATA_BUS_ERROR:
        BugMessage = "DATA_BUS_ERROR";
        break;

    case PHASE0_INITIALIZATION_FAILED:
        BugMessage = "PHASE0_INITIALIZATION_FAILED";
        break;

    case PHASE1_INITIALIZATION_FAILED:
        BugMessage = "PHASE1_INITIALIZATION_FAILED";
        break;

    case OIP_MULTIPLE_COMPLETE_REQUESTS:
        BugMessage = "OIP_MULTIPLE_COMPLETE_REQUESTS";
        break;

    case OIP_STACK_CORRUPTION:
        BugMessage = "OIP_STACK_CORRUPTION";
        break;

    case PAGE_FAULT_IN_NONPAGED_AREA:
        BugMessage = "PAGE_FAULT_IN_NONPAGED_AREA";
        break;

    case HAL0_INITIALIZATION_FAILED:
        BugMessage = "HAL0_INITIALIZATION_FAILED";
        break;

    case CONFIG_INITIALIZATION_FAILED: // (0x67) Juga VKEY0_INITIALIZATION_FAILED
        BugMessage = "CONFIG_INITIALIZATION_FAILED / VKEY0_INITIALIZATION_FAILED";
        break;

    case VKEY_MASTER_FAILED:
        BugMessage = "VKEY_MASTER_FAILED";
        break;

    case PROCESSTHREAD_INITIALIZATION_FAILED:
        BugMessage = "PROCESSTHREAD_INITIALIZATION_FAILED";
        break;

    case CONFIG_LIST_FAILED: // (0x73) Juga VKEY_CONFIG_CORRUPTED
        BugMessage = "CONFIG_LIST_FAILED / VKEY_CONFIG_CORRUPTED";
        break;

    case BOOT_DEVICE_HANDLE_MISSING:
        BugMessage = "BOOT_DEVICE_HANDLE_MISSING";
        break;

    case SYSTEM_THREAD_EXCEPTION_NOT_HANDLED:
        BugMessage = "SYSTEM_THREAD_EXCEPTION_NOT_HANDLED";
        break;

    case UNEXPECTED_KERNEL_MODE_TRAP:
        BugMessage = "UNEXPECTED_KERNEL_MODE_TRAP";
        break;

    case OBJECT0_INITIALIZATION_FAILED:
        BugMessage = "OBJECT0_INITIALIZATION_FAILED";
        break;

    case SECURITY_INITIALIZATION_FAILED:
        BugMessage = "SECURITY_INITIALIZATION_FAILED";
        break;

    case MEMORY_INITIALIZATION_FAILURE:
        BugMessage = "MEMORY_INITIALIZATION_FAILURE";
        break;

    case UNEXPECTED_INITIALIZATION_CALL:
        BugMessage = "UNEXPECTED_INITIALIZATION_CALL";
        break;

    case IO1_INITIALIZATION_FAILED:
        BugMessage = "IO1_INITIALIZATION_FAILED";
        break;

    case DRIVER_POWER_STATE_FAILURE: // (0x9F) Juga VKEY1_INITIALIZATION_FAILED
        BugMessage = "DRIVER_POWER_STATE_FAILURE / VKEY1_INITIALIZATION_FAILED";
        break;

    case HARDWARE_NO_ACPI_AVAILABLE: // (0xA5) Juga PC_ACPI_UNAVAILABLE
        BugMessage = "HARDWARE_NO_ACPI_AVAILABLE";
        break;

    case ATTEMPTED_WRITE_TO_READONLY_MEMORY:
        BugMessage = "ATTEMPTED_WRITE_TO_READONLY_MEMORY";
        break;

    case BAD_POOL_CALLER:
        BugMessage = "BAD_POOL_CALLER";
        break;

    case DRIVER_VERIFIER_DETECTED_VIOLATION:
        BugMessage = "DRIVER_VERIFIER_DETECTED_VIOLATION";
        break;

    case DRIVER_CORRUPTED_EXPOOL:
        BugMessage = "DRIVER_CORRUPTED_EXPOOL";
        break;

    case PNP_DETECTED_FATAL_ERROR:
        BugMessage = "PNP_DETECTED_FATAL_ERROR";
        break;

    case DRIVER_IRQL_NOT_LESS_OR_EQUAL:
        BugMessage = "DRIVER_IRQL_NOT_LESS_OR_EQUAL";
        break;

    case WORKER_BACK_AT_NON_PASSIVE_IRQL:
        BugMessage = "WORKER_BACK_AT_NON_PASSIVE_IRQL";
        break;

    case MANUALLY_INITIATED_CRASH:
        BugMessage = "MANUALLY_INITIATED_CRASH";
        break;

    case CRITICAL_PROCESS_DIED:
        BugMessage = "CRITICAL_PROCESS_DIED";
        break;

    case INVALID_OBJECT_DETACH:
        BugMessage = "INVALID_OBJECT_DETACH";
        break;

    case INVALID_OBJECT_STATE:
        BugMessage = "INVALID_OBJECT_STATE";
        break;

    case INVALID_OBJECT_ATTACH:
        BugMessage = "INVALID_OBJECT_ATTACH";
        break;

    case OIP_IS_IN_UNSTABLE_STATE:
        BugMessage = "OIP_IS_IN_UNSTABLE_STATE";
        break;

    case KERNEL_SECURITY_CHECK_FAILURE:
        BugMessage = "KERNEL_SECURITY_CHECK_FAILURE";
        break;

    default:
        BugMessage = "UNKNOWN_BUGCHECK_CODE";
        break;
    }

    BvPrintLog(BugMessage);
    BvPrintLog("\n\n");
}

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
    BvClearScreen();
    BvSetColorText(0xFFFFFFFF);
    BvSetColorBackground(0xFF000080);
    BvClearScreen();

    BvPrintLog(
        "\nAn Error has occured and VeaOS must be stopped to prevent"
        " a damage to your computer.\n\n"
    );

    KsGetAndPrintBugMessage(BugCheckCode);

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

    __Stop();
    KeRaiseIrql(HIGH_LEVEL);
    
    // Pastikan buffer bersih sebelum diformat
    RtlZeroMemory(Buffer, sizeof(Buffer));
    
    // Format pesan ala BSOD (Blue Screen of Death)
    // Pakai %08X untuk mencetak angka/pointer dalam format Hexadecimal 32-bit
    RtlStringCbPrintfA(Buffer, sizeof(Buffer), 
        "\n\r\n\r"
        "===============================================================\n\r"
        "*** FATAL SYSTEM ERROR ***\n\r"
        "A critical error has occurred and the system has been halted.\n\r"
        "\n\r"
        "***STOP: 0x%X (0x%X, 0x%X, 0x%X, 0x%X)\n\r"
        "===============================================================\n\r",
        BugCheckCode,
        BugCheckParameter1,
        BugCheckParameter2,
        BugCheckParameter3,
        BugCheckParameter4
    );

    /* Send to debugger */
    if ((BugCheckCode != MANUALLY_INITIATED_CRASH) && KdpPresent)
    {
        KdPrintf(Buffer);

        // KdpDebugBreak(DEBUG_BREAK_BUG_CHECK);
    }

    KsDrawBlueScreen(
        BugCheckCode,
        BugCheckParameter1,
        BugCheckParameter2,
        BugCheckParameter3,
        BugCheckParameter4
    );

    
    while (1) {
        __Halt();
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