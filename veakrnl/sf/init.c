#include <veakrnl.h>

/* VALUE */

PVEA_SID SfEveryoneSid = NULL;
PVEA_SID SfSystemSid = NULL;
PVEA_SID SfAdministratorSid = NULL;
PVEA_TOKEN SfSystemToken = NULL;
ULONGLONG SfGlobalLuidCounter = 1000;

/* FUNCTION */

VOID
VEAPI
SfpDumpSid(VOID)
{
    // Dump only main SID
    KdPrintf("================== SID DUMP ================\n\r");

    KdPrintf("Revision: %c, Authority: %d, Flags: %u, MachineID: 0x%x, UserID: 0x%x.\n\r",
            SfEveryoneSid->Revision, SfEveryoneSid->Authority, SfEveryoneSid->Flags, SfEveryoneSid->MachineID,
        SfEveryoneSid->UserID);

    KdPrintf("Revision: %c, Authority: %d, Flags: %u, MachineID: 0x%x, UserID: 0x%x.\n\r",
            SfSystemSid->Revision, SfSystemSid->Authority, SfSystemSid->Flags, SfSystemSid->MachineID,
        SfSystemSid->UserID);

    KdPrintf("Revision: %c, Authority: %d, Flags: %u, MachineID: 0x%x, UserID: 0x%x.\n\r",
            SfAdministratorSid->Revision, SfAdministratorSid->Authority, SfAdministratorSid->Flags, SfAdministratorSid->MachineID,
        SfAdministratorSid->UserID);

    KdPrintf("============================================\n\r");
}

BOOLEAN
VEAPI
SfInitSystem(ULONG BootPhase)
{
    if(BootPhase == 0)
    {
        // inisialisasi value diatas
        SfEveryoneSid = SfAllocateAndInitializeSid(0x0, 0x0, 1);
        if(!SfEveryoneSid)
        {
            return FALSE;
        }

        SfSystemSid = SfAllocateAndInitializeSid(0x0, 0x1, 1);
        if(!SfSystemSid)
        {
            return FALSE;
        }

        SfAdministratorSid = SfAllocateAndInitializeSid(0x0, 0x2, 1);
        if(!SfAdministratorSid)
        {
            return FALSE;
        }

        SfSystemToken = SfCreateSystemGenericToken();
        if(!SfSystemToken)
        {
            return FALSE;
        }
    }
    else if(BootPhase == 1)
    {
        // Inisialisasi tipe objek token untuk formalitas
    }

    return TRUE;
}