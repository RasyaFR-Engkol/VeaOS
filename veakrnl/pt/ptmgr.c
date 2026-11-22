/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : ptmgr.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Process Thread Manager
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 02-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file ptmgr.c
 * --------------------------------------------------------------------- */

ANSI_STRING PtVeaSoName = RTL_CONSTANT_ANSI_STRING("/SystemRoot/SystemReserved/veaso.so");

PVOID PtpSystemSoBase;
PVOID PtpSystemSoSection;
PVOID PtpSystemSoEntryPoint = NULL;
PVOID KsUserApcDispatcher = NULL;
PVOID KsUserExceptionDispatcher = NULL;
PVOID KsUserCallbackDispatcher = NULL;
PVOID KsRaiseUserExceptionDispatcher = NULL;

const ULONG PtpPriorityToWeight[32] = {
    /* 0 - 3 (Lowest / Idle) */     15,     20,     26,     33,
    /* 4 - 7 (Below Normal) */      42,     54,    335,    526,
    /* 8 - 11 (NORMAL) */         1024,   1277,   1586,   1991, // Priority 8 (Normal) = 1024
    /* 12 - 15 (Above Normal/High)*/2501,   3121,   3906,   4883,
    /* 16 - 19 (High/Realtime) */ 6103,   7629,   9548,  11935,
    /* 20 - 23 */                14918,  18648,  23310,  29137,
    /* 24 - 27 (Realtime Class)*/ 36421,  45527,  56909,  71137,
    /* 28 - 31 */                88761, 110951, 138689, 173361
};

VEASTATUS
VEAPI
PtpLookupSystemSoEntryPoint(
    IN PCSTR SymbolName,
    OUT PVOID *EntryPoint
)
{
    if (!PtpSystemSoBase || !SymbolName || !EntryPoint)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PELF32_EHDR Header = (PELF32_EHDR)PtpSystemSoBase;
    PELF32_SYM Sym = RtlFindElfSymbolByName(Header, SymbolName);

    if (!Sym)
    {
        DPRINT("[MM/ELF] Entry point symbol '%s' NOT FOUND in veaso.so!\n", SymbolName);
        return STATUS_PROCEDURE_NOT_FOUND;
    }

    /* Pakai RtlGetElfSymbolAddress agar aman baik untuk st_value relatif maupun ber-image-base */
    ULONG_PTR ResolvedAddr = RtlGetElfSymbolAddress(Header, (ULONG_PTR)PtpSystemSoBase, SymbolName);
    *EntryPoint = (PVOID)ResolvedAddr;

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
PtpMapSystemSo(
    IN PEPROCESS Process,
    IN PVOID *DllBase,
    IN BOOLEAN UseLargePages
)
{
    VEASTATUS Status;
    LARGE_INTEGER Offset = {{0,0}};
    SIZE_T ViewSize = 0;
    PVOID ImageBase = 0;

    Status = MmMapViewOfSection(
        PtpSystemSoSection,
        Process,
        (PVOID*)&ImageBase,
        0,
        0,
        &Offset,
        &ViewSize,
        ViewShare,
        0,
        PAGE_READWRITE
    );
    if (!VEA_SUCCESS(Status))
    {
        Status = STATUS_CONFLICTING_ADDRESSES;
    }

    if (DllBase)
    {
        *DllBase = ImageBase;
    }

    return Status;
}


VEASTATUS
VEAPI
PtLocateSystemSo(VOID)
{
    OBJECT_ATTRIBUTES Attributes;
    HANDLE FileHandle, SectionHandle;
    VEASTATUS Status;
    OIP_INFORMATION InformationBlock;

    InitializeObjectAttributes(
        &Attributes,
        &PtVeaSoName,
        0,
        NULL,
        NULL
    );
    
    /* Open VeaSo file */
    Status = VeaOpenFile(
        &FileHandle,
        FILE_READ_ACCESS,
        &Attributes,
        &InformationBlock,
        FILE_SHARE_READ,
        0
    );
    if (!VEA_SUCCESS(Status))
    {
        /* Is that mean no File? */
        KsBugCheckEx(PROCESSTHREAD_INITIALIZATION_FAILED, Status, 0, 0, 0);
    }

    Status = MmCheckSystemImage(FileHandle);
    if (Status == STATUS_IMAGE_CHECKSUM_MISMATCH || Status == STATUS_INVALID_IMAGE_FORMAT || Status == STATUS_INVALID_IMAGE_PROTECT)
    {
        KsBugCheckEx(PROCESSTHREAD_INITIALIZATION_FAILED, Status, 1, 0, 0);
    }

    Status = VeaCreateSection(
        &SectionHandle,
        SECTION_ALL_ACCESS,
        NULL,
        NULL,
        PAGE_EXECUTE,
        SEC_IMAGE,
        FileHandle
    );
    VeaClose(FileHandle);
    if (!VEA_SUCCESS(Status))
    {
        KsBugCheckEx(PROCESSTHREAD_INITIALIZATION_FAILED, Status, 2, 0, 0);
    }

    Status = ObReferenceObjectByHandle(
        SectionHandle,
        SECTION_ALL_ACCESS,
        MmSectionObjectType,
        KernelMode,
        (PVOID*)&PtpSystemSoSection
    );
    VeaClose(SectionHandle);
    if (!VEA_SUCCESS(Status))
    {
        KsBugCheckEx(PROCESSTHREAD_INITIALIZATION_FAILED, Status, 3, 0, 0);
    }

    Status = PtpMapSystemSo((PEPROCESS)PtGetCurrentProcess(), &PtpSystemSoBase, FALSE);
    if (!VEA_SUCCESS(Status))
    {
        KsBugCheckEx(PROCESSTHREAD_INITIALIZATION_FAILED, Status, 4, 0, 0);
    }

    return Status;
}

VEASTATUS
VEAPI
VeaContinue(
    IN PKTRAP_FRAME ContextFrame,
    IN BOOLEAN TestAlert
)
{
    PKTHREAD Thread;
    PKTRAP_FRAME TrapFrame;
    KTRAP_FRAME LocalContext;
    KPROCESSOR_MODE PreviousMode;

    Thread = PtGetCurrentThread();
    if (!Thread || !Thread->TrapFrame)
    {
        return STATUS_INVALID_PARAMETER;
    }

    TrapFrame = Thread->TrapFrame;
    PreviousMode = (KPROCESSOR_MODE)TrapFrame->PreviousPreviousMode;

    if (!ContextFrame)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // 1. Validasi & salin ContextFrame dari User Space ke buffer lokal kernel
    //    (Mencegah TOCTOU race condition & page fault saat memodifikasi TrapFrame asli)
    if (PreviousMode == UserMode)
    {
        if ((ULONG_PTR)ContextFrame >= (ULONG_PTR)MM_HIGHEST_USER_ADDRESS ||
            ((ULONG_PTR)ContextFrame + sizeof(KTRAP_FRAME)) > (ULONG_PTR)MM_HIGHEST_USER_ADDRESS)
        {
            return STATUS_ACCESS_VIOLATION;
        }
    }

    _VEA_TRY
    {
        RtlCopyMemory(&LocalContext, ContextFrame, sizeof(KTRAP_FRAME));
    }
    _VEA_EXCEPT(VEA_EXCEPTION_EXECUTE_HANDLER)
    {
        return STATUS_ACCESS_VIOLATION;
    }
    _VEA_END;

    TrapFrame->Eax = LocalContext.Eax;
    TrapFrame->Ebx = LocalContext.Ebx;
    TrapFrame->Ecx = LocalContext.Ecx;
    TrapFrame->Edx = LocalContext.Edx;
    TrapFrame->Esi = LocalContext.Esi;
    TrapFrame->Edi = LocalContext.Edi;
    TrapFrame->Ebp = LocalContext.Ebp;
    TrapFrame->Eip = LocalContext.Eip;
    TrapFrame->HardwareEsp = LocalContext.HardwareEsp; // Sesuaikan dengan nama field ESP di KTRAP_FRAME lo

    if (PreviousMode == UserMode)
    {
        // Kalau caller di Ring 3 mengisi selector != 0, paksa RPL = 3 (| 3).
        // Kalau di-zero (0), pertahankan selector Ring 3 bawaan TrapFrame agar tidak #GP saat iretd!
        if (LocalContext.SegCs != 0) TrapFrame->SegCs = (LocalContext.SegCs | 3);
        if (LocalContext.HardwareSegSs != 0) TrapFrame->HardwareSegSs = (LocalContext.HardwareSegSs | 3);
        if (LocalContext.SegDs != 0) TrapFrame->SegDs = (LocalContext.SegDs | 3);
        if (LocalContext.SegEs != 0) TrapFrame->SegEs = (LocalContext.SegEs | 3);
        if (LocalContext.SegFs != 0) TrapFrame->SegFs = (LocalContext.SegFs | 3);

        // Sanitasi EFlags: izinkan hanya flag aritmatika/status user (CF, PF, AF, ZF, SF, DF, OF)
        // dan paksa Interrupt Flag (0x200) tetap aktif agar Ring 3 tidak bisa mematikan IRQ!
        if (LocalContext.EFlags != 0)
        {
            TrapFrame->EFlags = (LocalContext.EFlags & 0x00000DD5) | 0x202;
        }
        else
        {
            TrapFrame->EFlags |= 0x202;
        }
    }
    else
    {
        TrapFrame->SegCs = LocalContext.SegCs;
        TrapFrame->EFlags = LocalContext.EFlags;
    }

    if (TestAlert && PreviousMode == UserMode)
    {
        
    }

    // 6. Langsung keluar lewat RiServiceExit tanpa melewati epilog syscall normal!
    RiServiceExit(TrapFrame);

    // Unreachable (karena RiServiceExit adalah NORET)
    return STATUS_SUCCESS;
}

/* EOF */