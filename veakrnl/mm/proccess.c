/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : proccess.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Process supporting on Mm thing
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 05-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file proccess.c
 * --------------------------------------------------------------------- */

LONG 
VEAPI
MmpCompareVad(PRTL_BALANCED_NODE NodeA, PRTL_BALANCED_NODE NodeB);

VEASTATUS
VEAPI
MmCreateTeb(
    IN PEPROCESS Process,
    IN PCLIENT_ID ClientId,
    IN PINITIAL_TEB InitialTeb OPTIONAL,
    OUT PTEB *TebBase
)
{
    PTEB Teb = NULL;
    ULONG TebVa = 0;
    ULONG PhysPage = 0;
    BOOLEAN IsAttached = FALSE;
    PMMPDE Pde;
    PMMPTE Pte;
    MMPTE TempPte;
 
    if (!Process || !TebBase)
        return STATUS_INVALID_PARAMETER;
 
    // 1. Switch ke CR3/Context proses target jika belum di dalamnya
    if ((PEPROCESS)PtGetCurrentProcess() != Process)
    {
        PtAttachProcess((PKPROCESS)Process);
        IsAttached = TRUE;
    }
 
    // 2. Cari slot User Virtual Address yang kosong di kisaran 0x7FFDE000 (Standar NT TEB Range)
    //    Slot dianggap kosong kalau PDE belum ada, atau PTE-nya benar-benar 0.
    //    (PTE non-valid tapi tidak nol = PTE software, mis. demand-zero: sudah terpakai.)
    //    Operator || memastikan PTE tidak dibaca kalau PDE belum ada.
    for (ULONG CandidateVa = 0x7FFDE000; CandidateVa >= 0x7FF00000; CandidateVa -= PAGE_SIZE)
    {
        if (!MiAddressToPde(CandidateVa)->Hard.Valid ||
            MiAddressToPte(CandidateVa)->Long == 0)
        {
            TebVa = CandidateVa;
            break;
        }
    }
 
    if (TebVa == 0)
    {
        DPRINT("[MM/TEB] ERROR: Out of User VA space for TEB!\n");
        if (IsAttached) PtDetachProcess();
        return STATUS_INSUFFICIENT_RESOURCES;
    }
 
    // 3. Alokasikan 1 Physical RAM Page untuk TEB
    PhysPage = MmAllocatePhysicalPage();
    if (PhysPage == 0)
    {
        DPRINT("[MM/TEB] ERROR: Out of Physical Memory for TEB!\n");
        if (IsAttached) PtDetachProcess();
        return STATUS_INSUFFICIENT_RESOURCES;
    }
 
    // 4. Pastikan Page Table (PDE) untuk TebVa ada. MmpEnsurePageTable memasang
    //    PDE dengan bit user untuk VA user, jadi tidak perlu set manual.
    if (!MmpEnsurePageTable(TebVa))
    {
        MmFreePhysicalPage(PhysPage);
        if (IsAttached) PtDetachProcess();
        return STATUS_INSUFFICIENT_RESOURCES;
    }
 
    // PDE lama yang mungkin dibuat tanpa bit user: pastikan Owner (U/S) = 1
    Pde = MiAddressToPde(TebVa);
    if (!Pde->Hard.Owner)
    {
        Pde->Hard.Owner = 1;
    }
 
    // 5. Setup Hardware PTE: user + read/write (ditulis sekali)
    //    User Mode (Ring 3) bisa baca & tulis FS:[0x18]
    Pte = MiAddressToPte(TebVa);
    MI_MAKE_VALID_PTE(TempPte, PhysPage >> PAGE_SHIFT, 1, 1);
    MI_WRITE_VALID_PTE(Pte, TempPte);
 
    MMI_INVALIDATE_TLB(TebVa);
 
    // 6. Zeroing isi memori TEB 4KB (Kernel bisa langsung nulis ke TebVa karena sedang Attach)
    RtlZeroMemory((PVOID)TebVa, PAGE_SIZE);
 
    Teb = (PTEB)TebVa;
    Teb->NtTib.ExceptionList = (PVOID)(ULONG_PTR)-1; // End of SEH Chain (0xFFFFFFFF)
 
    if (InitialTeb)
    {
        Teb->NtTib.StackBase  = InitialTeb->StackBase;
        Teb->NtTib.StackLimit = InitialTeb->StackLimit;
    }
 
    // FS:[0x18] menunjuk ke User Virtual Address TEB itu sendiri
    Teb->NtTib.Self = (PVOID)Teb;
 
    if (ClientId)
    {
        Teb->ClientId = *ClientId;
    }
 
    Teb->ProcessEnvironmentBlock = Process->Peb;
 
    if (IsAttached)
    {
        PtDetachProcess();
    }
 
    *TebBase = Teb;
    return STATUS_SUCCESS;
}


VOID
VEAPI
MmDeletePeb(
    IN PEPROCESS Process,
    IN PPEB PebBase
)
{
    BOOLEAN IsAttached = FALSE;
    ULONG PebVa = (ULONG)PebBase;
 
    if (!Process || !PebBase)
        return;
 
    if ((PEPROCESS)PtGetCurrentProcess() != Process)
    {
        PtAttachProcess((PKPROCESS)Process);
        IsAttached = TRUE;
    }
 
    /* PDE dulu: membaca PTE lewat recursive mapping saat PDE tidak ada akan fault */
    if (MiAddressToPde(PebVa)->Hard.Valid)
    {
        PMMPTE Pte = MiAddressToPte(PebVa);
 
        if (Pte->Hard.Valid)
        {
            ULONG PhysPage = Pte->Hard.PageFrameNumber << PAGE_SHIFT;
 
            // Invalidate PTE & TLB
            MI_ERASE_PTE(Pte);
            MMI_INVALIDATE_TLB(PebVa);
 
            // Bebaskan physical RAM page ke PIP Database Free List
            MmFreePhysicalPage(PhysPage);
        }
    }
 
    Process->Peb = NULL;
 
    if (IsAttached)
    {
        PtDetachProcess();
    }
 
    DPRINT("[MM/PEB] MmDeletePeb SUCCESS for Process '%s'\n", Process->ImageFileName);
}


VEASTATUS
VEAPI
MmCreatePeb(
    IN PEPROCESS Process,
    IN PINITIAL_PEB InitialPeb OPTIONAL,
    OUT PPEB *PebBase
)
{
    PPEB Peb = NULL;
    ULONG PebVa = MM_PEB_BASE_ADDRESS;
    ULONG PhysPage = 0;
    BOOLEAN IsAttached = FALSE;
    PMMPDE Pde;
    PMMPTE Pte;
    MMPTE TempPte;
 
    if (!Process || !PebBase)
        return STATUS_INVALID_PARAMETER;
 
    // 1. Switch ke CR3/Address Space proses target jika dipanggil dari luar konteks
    if ((PEPROCESS)PtGetCurrentProcess() != Process)
    {
        PtAttachProcess((PKPROCESS)Process);
        IsAttached = TRUE;
    }
 
    // 2. Alamat PEB tidak boleh sudah terpakai. Versi lama menimpa PTE begitu saja,
    //    sehingga frame lama bocor. PDE dicek dulu sebelum membaca PTE.
    if (MiAddressToPde(PebVa)->Hard.Valid &&
        MiAddressToPte(PebVa)->Long != 0)
    {
        DPRINT("[MM/PEB] ERROR: PEB address 0x%08X already in use!\n", PebVa);
        if (IsAttached) PtDetachProcess();
        return STATUS_CONFLICTING_ADDRESSES;
    }
 
    // 3. Alokasikan 1 Physical RAM Page untuk PEB
    PhysPage = MmAllocatePhysicalPage();
    if (PhysPage == 0)
    {
        DPRINT("[MM/PEB] ERROR: Out of Physical Memory for PEB!\n");
        if (IsAttached) PtDetachProcess();
        return STATUS_INSUFFICIENT_RESOURCES;
    }
 
    // 4. Pastikan Page Table (PDE) untuk 0x7FFDF000 ada. MmpEnsurePageTable
    //    memasang PDE dengan bit user (U/S = 1) untuk VA user secara atomik.
    if (!MmpEnsurePageTable(PebVa))
    {
        MmFreePhysicalPage(PhysPage);
        if (IsAttached) PtDetachProcess();
        return STATUS_INSUFFICIENT_RESOURCES;
    }
 
    // PDE lama yang mungkin dibuat tanpa bit user: pastikan Owner (U/S) = 1
    Pde = MiAddressToPde(PebVa);
    if (!Pde->Hard.Owner)
    {
        Pde->Hard.Owner = 1;
    }
 
    // 5. Hardware PTE: user + read/write, ditulis sekali
    //    User Mode (Ring 3) bisa membaca & menulis PEB
    Pte = MiAddressToPte(PebVa);
    MI_MAKE_VALID_PTE(TempPte, PhysPage >> PAGE_SHIFT, 1, 1);
    MI_WRITE_VALID_PTE(Pte, TempPte);
 
    MMI_INVALIDATE_TLB(PebVa);
 
    // 6. Zeroing isi memori 4KB PEB di User Space
    RtlZeroMemory((PVOID)PebVa, PAGE_SIZE);
 
    Peb = (PPEB)PebVa;
 
    // 7. Inisialisasi Field Dasar PEB
    Peb->InheritedAddressSpace = FALSE;
    Peb->ReadImageFileExecOptions = FALSE;
    Peb->BeingDebugged = FALSE;
    Peb->BitField = 0;
 
    if (InitialPeb)
    {
        Peb->Mutant = InitialPeb->Mutant;
    }
    else
    {
        Peb->Mutant = (HANDLE)-1;
    }
 
    // Informasi Versi OS (Proses User Mode / C Runtime sering mengecek field ini)
    Peb->OSMajorVersion = 5;
    Peb->OSMinorVersion = 2;
    Peb->OSBuildNumber = 3790;
    Peb->OSPlatformId = 2; // VER_PLATFORM_WIN32_NT
    Peb->NumberOfProcessors = 1; // Atau isi variabel jumlah CPU VeaOS
 
    // Tautkan ImageBaseAddress jika EPROCESS memiliki SectionObject
    if (Process->SectionObject)
    {
        PSECTION Section = (PSECTION)Process->SectionObject;
        if (Section->Segment)
        {
            PSEGMENT Segment = (PSEGMENT)Section->Segment;
            Peb->ImageBaseAddress = Segment->BasedAddress;
        }
    }
 
    // Simpan pointer PEB ke struktur EPROCESS
    Process->Peb = Peb;
 
    if (IsAttached)
    {
        PtDetachProcess();
    }
 
    *PebBase = Peb;
    return STATUS_SUCCESS;
}

VOID
VEAPI
MmDeleteTeb(
    IN PEPROCESS Process,
    IN PTEB TebBase
)
{
    BOOLEAN IsAttached = FALSE;
    ULONG TebVa = (ULONG)TebBase;
 
    if (!Process || !TebBase)
        return;
 
    if ((PEPROCESS)PtGetCurrentProcess() != Process)
    {
        PtAttachProcess((PKPROCESS)Process);
        IsAttached = TRUE;
    }
 
    /* PDE dulu: membaca PTE lewat recursive mapping saat PDE tidak ada akan fault */
    if (MiAddressToPde(TebVa)->Hard.Valid)
    {
        PMMPTE Pte = MiAddressToPte(TebVa);
 
        if (Pte->Hard.Valid)
        {
            ULONG PhysPage = Pte->Hard.PageFrameNumber << PAGE_SHIFT;
 
            // Invalidate PTE & TLB
            MI_ERASE_PTE(Pte);
            MMI_INVALIDATE_TLB(TebVa);
 
            // Bebaskan physical RAM page ke PIP Database Free List
            MmFreePhysicalPage(PhysPage);
        }
    }
 
    if (IsAttached)
    {
        PtDetachProcess();
    }
 
    DPRINT("[MM/TEB] MmDeleteTeb SUCCESS: TebVa=0x%08X\n", TebVa);
}


VEASTATUS
VEAPI
MiInitializeHandBuiltProcess(
    IN PEPROCESS Process,
    OUT PULONG_PTR DirectoryTableBase
)
{
    ULONG CurrentCr3;

    if (!Process || !DirectoryTableBase)
    {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // TANGKAP ALAMAT FISIK CR3 AKTIF DARI BOOTLOADER
    //    Memanfaatkan helper MiGetSystemPageDirectoryTableBase() di mapping.c
    //

    CurrentCr3 = (ULONG)MiGetSystemPageDirectoryTableBase();

    if (CurrentCr3 == 0)
    {
        DPRINT("[MM/BOOT] ERROR: Failed to read current CR3 from hardware!\n");
        return STATUS_UNSUCCESSFUL;
    }

    //
    // ISI ARRAY DIRECTORY TABLE BASE & KPROCESS PCB
    //    DirectoryTableBase[0] = Alamat Fisik CR3 Bootloader
    //    DirectoryTableBase[1] = Hyperspace Base (Disamakan atau 0 di Boot Process)
    //

    DirectoryTableBase[0] = CurrentCr3;
    DirectoryTableBase[1] = 0;

    Process->Pcb.DirectoryTableBase = CurrentCr3;

    //
    // VERIFIKASI SELF-REFERENCING PDE (INDEX 1023)
    //    Memastikan PDE index 1023 menunjuk ke CurrentCr3 agar macro MM_GET_PD() 
    //    & MM_GET_PTE() di kernel VeaOS berjalan sempurna
    //

    PULONG PageDirVa = (PULONG)MM_GET_PD();
    ULONG Flags = PageDirVa[1023] & 0xFFF;
    if (Flags == 0) Flags = 0x07; // Present | Read/Write | User

    if ((PageDirVa[1023] & ~0xFFF) != CurrentCr3)
    {
        PageDirVa[1023] = CurrentCr3 | Flags;
        MMI_INVALIDATE_TLB((ULONG)&PageDirVa[1023]);
    }

    //
    // INISIALISASI VAD AVL TREE PROSES BOOT (PT SYSTEM PROCESS)
    //

    Process->VadRoot.Root = NULL;
    Process->VadRoot.CompareRoutine = MmpCompareVad;

    DPRINT("[MM/BOOT] Handbuilt process '%s' linked to Bootloader CR3: 0x%08X\n",
           Process->ImageFileName[0] ? Process->ImageFileName : "PtSystemProcess",
           CurrentCr3);

    return STATUS_SUCCESS;
}

BOOLEAN
VEAPI
MmCreateProcessAddressSpace(
    IN PEPROCESS Process,
    OUT PULONG_PTR DirectoryTableBase
)
{
    ULONG PageDirPhys;
    ULONG HyperPhys;
    PULONG PageDirVa;
    PULONG SystemPageDir;
    ULONG Flags;
    ULONG i;

    PageDirPhys = MmAllocatePhysicalPage();
    if (PageDirPhys == 0)
    {
        DPRINT("[MM/SPACE] ERROR: Failed to allocate physical page for Page Directory!\n");
        return FALSE;
    }

    HyperPhys = MmAllocatePhysicalPage();
    if (HyperPhys == 0)
    {
        DPRINT("[MM/SPACE] ERROR: Failed to allocate physical page for Hyperspace!\n");
        MmFreePhysicalPage(PageDirPhys);
        return FALSE;
    }

    PageDirVa = (PULONG)MmMapIoSpace(PageDirPhys, PAGE_SIZE, MmCached);
    if (!PageDirVa)
    {
        DPRINT("[MM/SPACE] ERROR: Failed to map Page Directory physical page!\n");
        MmFreePhysicalPage(HyperPhys);
        MmFreePhysicalPage(PageDirPhys);
        return FALSE;
    }

    RtlZeroMemory(PageDirVa, PAGE_SIZE);

    SystemPageDir = (PULONG)MM_GET_PD();
    for (i = 768; i < 1024; i++)
    {
        PageDirVa[i] = SystemPageDir[i];
    }

    Flags = SystemPageDir[1023] & 0xFFF;
    if (Flags == 0) Flags = 0x07;
    PageDirVa[1023] = PageDirPhys | Flags;

    MmUnmapIoSpace(PageDirVa, PAGE_SIZE);

    DirectoryTableBase[0] = PageDirPhys; 
    DirectoryTableBase[1] = HyperPhys;
    Process->Pcb.DirectoryTableBase = PageDirPhys;
    Process->VadRoot.Root = NULL;
    Process->VadRoot.CompareRoutine = MmpCompareVad;

    return TRUE;
}

VEASTATUS
VEAPI
MmInitializeProcessAddressSpace(
    IN PEPROCESS Process,
    IN PVOID SectionObject OPTIONAL,
    OUT PCHAR ImageFileName OPTIONAL
)
{
    VEASTATUS Status = STATUS_SUCCESS;
    BOOLEAN IsAttached = FALSE;
    PSECTION Section = (PSECTION)SectionObject;
    PVOID ImageBase = NULL;
    SIZE_T ViewSize = 0;

    if (!Process)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (!Process->VadRoot.CompareRoutine)
    {
        MmpInitializeProcessVadTree(Process);
    }

    //
    // 1. ATTACH KE CONTEXT PROSES TARGET (SWITCH CR3)
    //
    if ((PEPROCESS)PtGetCurrentProcess() != Process)
    {
        PtAttachProcess((PKPROCESS)Process);
        IsAttached = TRUE;
    }

    //
    // 2. TANDAI STATUS ALAMAT MEMORI SAMA SEPERTI REACTOS (PHASE 2)
    //
    Process->AddressSpaceInitialized = TRUE;

    // Pastikan VAD AVL Tree User Space milik proses sudah siap
    if (!Process->VadRoot.CompareRoutine)
    {
        Process->VadRoot.Root = NULL;
        Process->VadRoot.CompareRoutine = MmpCompareVad;
    }

    //
    // 3. JIKA TERIKAT DENGAN SECTION OBJECT (USER MODE EXECUTABLE IMAGE)
    //
    if (Section)
    {
        //
        // A. EKSTRAK NAMA FILE EXECUTABLE DARI FILE OBJECT KE EPROCESS->ImageFileName
        //
        if (Section->Segment && Section->Segment->ControlArea && Section->Segment->ControlArea->FilePointer)
        {
            PFILE_OBJECT FileObject = Section->Segment->ControlArea->FilePointer;
            
            if (FileObject->FileName.Buffer && FileObject->FileName.Length > 0)
            {
                PCHAR Buffer = FileObject->FileName.Buffer;
                USHORT Length = FileObject->FileName.Length;
                PCHAR NameStart = Buffer + Length;

                // Cari karakter pemisah path ('/' atau '\') dari belakang
                while (NameStart > Buffer)
                {
                    if (*(NameStart - 1) == '/' || *(NameStart - 1) == '\\')
                    {
                        break;
                    }
                    NameStart--;
                }

                // Salin maksimal 15 karakter ke EPROCESS->ImageFileName
                RtlCopyRawStringN(Process->ImageFileName, NameStart, 15);
                Process->ImageFileName[15] = '\0';
            }
        }

        // Salin ke parameter ImageFileName jika caller (seperti PtpCreateProcess) memintanya
        if (ImageFileName && Process->ImageFileName[0] != '\0')
        {
            RtlCopyRawStringN(ImageFileName, Process->ImageFileName, 15);
        }

        //
        // B. MAP SECTION EXECUTABLE (PE/ELF) KE VIRTUAL ADDRESS SPACE RING 3
        //
        Status = MmMapViewOfSection(
            Section,
            Process,
            &ImageBase,
            0,
            0,
            NULL,
            &ViewSize,
            ViewUnmap,
            0,
            PAGE_READWRITE
        );

        if (VEA_SUCCESS(Status))
        {
            // Simpan alamat Base Executable ke EPROCESS (misal 0x08048000 / 0x00400000)
            Process->SectionBaseAddress = ImageBase;
        }
        else
        {
            DPRINT("[MM/INIT] ERROR: Failed to map Section for process '%s'! Status: 0x%08X\n",
                   Process->ImageFileName, Status);
        }
    }

    //
    // 4. DETACH DARI CONTEXT PROSES (KEMBALI KE CONTEXT PEMANGGIL)
    //
    if (IsAttached)
    {
        PtDetachProcess();
    }

    return Status;
}

/* EOF */