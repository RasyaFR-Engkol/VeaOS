#include <veakrnl.h>

UCHAR UlpInitializationPhase = 0;

BOOLEAN
VEAPI
UlpInitSystemPhase0(VOID)
{
    /* TODO: Initialize ERESOURCE */

    UlpInitLookasideList();

    return TRUE;
}

BOOLEAN 
VEAPI
UlpInitSystemPhase1(VOID)
{
    /* Initialize Worker */
    UlpInitializeWorkerThreads();

    return TRUE;
}

BOOLEAN
VEAPI
UlInitSystem(VOID)
{
    switch (UlpInitializationPhase)
    {
        case 0:
        {
            return UlpInitSystemPhase0();
        }

        case 1:
        {
            return UlpInitSystemPhase1();
        }

        default:
        {
            KsBugCheck(UNEXPECTED_INITIALIZATION_CALL);
            return FALSE;
        }
    }
}

void ul_initialize_layer(PBLOCK_BOOT_2 block_boot)
{
    /* ini yang harus di inisialisasi (in chronological order)
    1. mm
    2. ob
    3. pt
    4. kpc
    5. ul
    6. hct
    simple */

    UlInitPoolLookasidePointers();

    if(!MmInitialize(block_boot))
    {
        KsBugCheck(MEMORY_INITIALIZATION_FAILURE);
    }

    if(!SfInitSystem(0))
    {
        KsBugCheck(SECURITY_INITIALIZATION_FAILED);
    }

    if(!ObInitSystem())
    {
        KsBugCheck(OBJECT0_INITIALIZATION_FAILED);
    }

    if(!PtInitSystem())
    {
        KsBugCheck(PROCESSTHREAD_INITIALIZATION_FAILED);
    }

    /*if(!kpc_initialize())
    {
        //ks_bug_check();
    }*/

    if(!HctInitSystem(0))
    {
        KsBugCheck(HAL0_INITIALIZATION_FAILED);
    }

    if(!UlInitSystem())
    {
        KsBugCheck(PHASE0_INITIALIZATION_FAILED);
    }
}

VOID
VEAPI
UlParseBootArgument(PSTR BootArgument)
{
    
}

VOID
VEAPI
TestSectionMaking(VOID)
{
    PVOID SectionObject = NULL;
    HANDLE SectionHandle = NULL;
    VEASTATUS Status;
    LARGE_INTEGER SectionSize;
    OBJECT_ATTRIBUTES ObjAttr;
    ANSI_STRING SectionName;

    // Variable pendukung untuk MmMapViewOfSection
    PVOID BaseAddress = NULL;
    SIZE_T ViewSize = 0; // Diset 0 untuk uji coba Auto-Size Bounds calculation!

    DPRINT("=== [VeaOS MM Test] Starting Section Full Lifecycle Test ===\n\r");

    _VEA_TRY
    {
        SectionSize.QuadPart = 64 * 1024; // 64 KB

        RtlInitAnsiString(&SectionName, "/Device/MyTestSection");

        InitializeObjectAttributes(
            &ObjAttr,
            &SectionName,
            OBJ_CASE_INSENSITIVE,
            NULL,
            NULL
        );

        // 1. Create Section (PAGE_READWRITE)
        Status = MmCreateSection(
            &SectionObject,
            SECTION_ALL_ACCESS,
            &ObjAttr,
            &SectionSize,
            PAGE_READWRITE,
            SEC_COMMIT,
            NULL,
            NULL
        );

        if (VEA_SUCCESS(Status) && SectionObject != NULL)
        {
            DPRINT("[PASS] Named Section created at 0x%08X\n\r", (ULONG)SectionObject);

            // 2. Insert Object ke Object Tree
            Status = ObInsertObject(
                SectionObject,
                NULL,
                SECTION_ALL_ACCESS,
                0,
                NULL,
                &SectionHandle
            );

            if (VEA_SUCCESS(Status))
            {
                DPRINT("[PASS] Section inserted into Object Tree! Handle: 0x%08X\n\r", (ULONG)SectionHandle);
            }

            // -----------------------------------------------------------------
            // 3. MAP SECTION
            // -----------------------------------------------------------------
            Status = MmMapViewOfSection(
                SectionObject,
                (PEPROCESS)PtGetCurrentProcess(),
                &BaseAddress,
                0,
                0,
                NULL,          // Offset NULL -> Mulai dari byte 0
                &ViewSize,     // ViewSize 0 -> Harus otomatis terisi 65536 bytes
                ViewShare,
                0,
                PAGE_READWRITE
            );

            if (VEA_SUCCESS(Status) && BaseAddress != NULL)
            {
                DPRINT("[PASS] MmMapViewOfSection Succeeded!\n\r");
                DPRINT("       Mapped Base VA : 0x%08X\n\r", (ULONG)BaseAddress);
                DPRINT("       Auto View Size : %u bytes\n\r", (ULONG)ViewSize);

                // -------------------------------------------------------------
                // 4. TEST R/W SEBELUM UNMAP (HARUS SUKSES VIA DEMAND PAGING)
                // -------------------------------------------------------------
                PULONG TestPtr = (PULONG)BaseAddress;
                *TestPtr = 0xCAFEBAB3;

                if (*TestPtr == 0xCAFEBAB3)
                {
                    DPRINT("[PASS] Memory R/W Verified! Value 0xCAFEBAB3 read at VA 0x%08X\n\r", (ULONG)BaseAddress);
                }

                // -------------------------------------------------------------
                // 5. UNMAP SECTION
                // -------------------------------------------------------------
                Status = MmUnmapViewOfSection((PEPROCESS)PtGetCurrentProcess(), BaseAddress);

                if (VEA_SUCCESS(Status))
                {
                    DPRINT("[PASS] MmUnmapViewOfSection Succeeded!\n\r");

                    // ---------------------------------------------------------
                    // 6. TEST R/W SETELAH UNMAP (HARUS FAULT & DITANGKAP PSEH)
                    // ---------------------------------------------------------
                    DPRINT("[TEST] Accessing unmapped VA 0x%08X (Expecting Fault)...\n\r", (ULONG)BaseAddress);

                    _VEA_TRY
                    {
                        // Akses ini harus memicu #PF.
                        // Karena VAD sudah dihapus dan PTE di-invalidasi, MmAccessFault bakal return FALSE
                        // dan exception ditangkap oleh _VEA_EXCEPT di bawah!
                        *TestPtr = 0xDEADBEEF;

                        DPRINT("[FAIL] Access to unmapped memory succeeded unexpectedly! Value: 0x%08X\n\r", *TestPtr);
                    }
                    _VEA_EXCEPT(1)
                    {
                        DPRINT("[PASS] Page Fault caught by PSEH as expected on unmapped memory 0x%08X!\n\r", (ULONG)BaseAddress);
                    }
                    _VEA_END
                }
                else
                {
                    DPRINT("[FAIL] MmUnmapViewOfSection failed! Status: 0x%08X\n\r", Status);
                }
            }
            else
            {
                DPRINT("[FAIL] MmMapViewOfSection failed! Status: 0x%08X\n\r", Status);
            }
        }
    }
    _VEA_EXCEPT(1)
    {
        DPRINT("[EXCEPT] Outer Fault caught during test!\n\r");
    }
    _VEA_END

    DPRINT("=== [VeaOS MM Test] Test Completed ===\n\r");
}

VOID
VEAPI
UlPhase1(VOID)
{
    PCHAR Buffer;
    BOOLEAN MiniVeaMode = FALSE, DebugMode = FALSE, VerboseDebugMode = FALSE;
    BOOLEAN TextBootMode = FALSE;
    PSTR BootArg = KsLoaderBlock->VeaBootArgument;
    VEASTATUS Status;

    KdPrintf("PHASE1\n\r");

    /* Set to Phase 1 */
    UlpInitializationPhase = 1;

    /* Allocate Pool for processing in Phase1*/
    Buffer = (PCHAR)UlAllocatePoolWithTag(NonPagedPool, 0x2000, 'Ex1o');
    if(!Buffer)
    {
        /* Bugcheck */
        KsBugCheckEx(PHASE1_INITIALIZATION_FAILED, 0x1, (ULONG_PTR)Buffer, 0, 0);
    }

    /* Make sure LoaderBlock is fine */
    if(!KsLoaderBlock)
    {
        /* Bugcheck again */
        KsBugCheck(PHASE1_INITIALIZATION_FAILED);
    }
    
    /* Initialize our BvDriver, acquire it then */
    BvInitializeDriver(KsLoaderBlock);
    BvAcquireDisplayState(TRUE);

    /* Parse our argument*/
    if((RtlCheckBootFlag(BootArg, "/CLIMODE")))
    {
        TextBootMode = TRUE;
    }

    /* MiniVeaMode = VeaInstaller or VEAPE mode */
    if((RtlCheckBootFlag(BootArg, "/MINIVEA")))
    {
        MiniVeaMode = TRUE;
    }

    /* Debugging purpose */
    if(RtlCheckBootFlag(BootArg, "/DEBUG"))
    {
        DebugMode = TRUE;

        /* /VERBOSELOG must include /DEBUG flag */
        if(RtlCheckBootFlag(BootArg, "/VERBOSELOG"))
        {
            VerboseDebugMode = TRUE;
        }
    }

    /* For another argument boot option (eg. /MAXMEM=VALUE, /BURNMEMORY=VALUE,
     /REPAIRING, /BAUDRATE=VALUE, /SERIAL=COM1, etc), throw to this function */
    UlParseBootArgument(BootArg);

    if(!TextBootMode)
    {
        /* Initialize Logo Bitmap booting */
        BvDisplayBootLogoFadeIn();
        // BvEnableStringDisplay(FALSE);
    }
    else
    {
        /* Clean our logo hehe */
        // BvEnableStringDisplay(TRUE);
        // BvShowHeaderFooterTextMode();
    }

    if (!ObInitSystem())
    {
        KsBugCheckEx(OBJECT0_INITIALIZATION_FAILED, 1, 0, 0, 0);
    }

    if(!VkInitSystem1(KsLoaderBlock))
    {
        KsBugCheck(VKEY0_INITIALIZATION_FAILED);
    }

    /* Call UlInitSystem1 */
    if (!UlInitSystem())
    {
        /* Failed */
        KsBugCheckEx(PHASE1_INITIALIZATION_FAILED, 1, 0, 0, 0);
    }

    /* Call MmInitialize */
    if (!MmInitialize(KsLoaderBlock))
    {
        KsBugCheckEx(MEMORY_MANAGEMENT_ERROR, 0x2, 0, 0, 0);
    }

    /* Call Vk phase 1*/
    if(!VkInitSystemPhase1())
    {
        /* Bugcheck */
        KsBugCheckEx(VKEY1_INITIALIZATION_FAILED, 1, 0, 0, 0);
    }

    /* Initialize our Io Subsystem*/
    if(!ObioInitSystem1())
    {
        /* Bugcheck */
        KsBugCheckEx(IO1_INITIALIZATION_FAILED, 1, 0, 0, 0);
    }

    TestSectionMaking();

    ObDumpObjectTree();
    ObioDumpDeviceTree();

    DPRINT("ObioInitSystem1 finished\n");

    Loop:
    PtIdleLoop();
}

