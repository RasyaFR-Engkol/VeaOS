#pragma once

typedef struct _LDR_MODULE
{
    LIST_ENTRY InLoadOrderLinks;
    PVOID      Base;
    PVOID      EntryPoint;     /* e_entry yang sudah di-rebase (EXE) */
    PVOID      InitFunction;   /* DT_INIT, diisi di step berikutnya */
    LONG       LoadCount;      /* -1 = permanen (EXE, veaso.so) */
    ULONG      Flags;
    CHAR       Name[64];       /* "veaso.so", dst. */
} LDR_MODULE, *PLDR_MODULE;

#define LDRP_FLAG_EXE        0x00000001
#define LDRP_FLAG_PROCESSED  0x00000002   /* relokasi sudah dilakukan */
#define LDRP_FLAG_LOADING    0x00000004   /* untuk deteksi siklus nanti */
#define LDRP_FLAG_INITED     0x00000008   /* DT_INIT sudah dipanggil */

typedef struct _LDR_DATA
{
    LIST_ENTRY InLoadOrderModuleList;
    BOOLEAN    Initialized;
} LDR_DATA, *PLDR_DATA;