#include <veakrnl.h>

void ul_initialize_layer(PBLOCK_BOOT_1 block_boot)
{
    /* ini yang harus di inisialisasi (in chronological order)
    1. mm
    2. ob
    3. pt
    4. kpc
    5. ul
    6. hct
    simple */

    if(!MmInitialize(block_boot))
    {
        //ks_bug_check();
    }

    if(!ObInitSystem())
    {
        //ks_bug_check();
    }

    /*if(!PtInitSystem())
    {
        //ks_bug_check();
    }

    if(!kpc_initialize())
    {
        //ks_bug_check();
    }

    if(!ul_initialize())
    {
        //ks_bug_check();
    }*/
}