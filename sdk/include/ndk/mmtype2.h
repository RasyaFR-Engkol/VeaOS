#pragma once

#include <procbind.h>

typedef struct _SECTION_IMAGE_INFORMATION
{
    PVOID TransferAddress;
    ULONG ZeroBits;
    SIZE_T MaximumStackSize;
    SIZE_T CommittedStackSize;
    ULONG SubSystemType;
    union
    {
        struct
        {
            USHORT SubSystemMinorVersion;
            USHORT SubSystemMajorVersion;
        };
        ULONG SubSystemVersion;
    };
    ULONG GpValue;
    USHORT ImageCharacteristics;
    USHORT DllCharacteristics;
    USHORT Machine;
    BOOLEAN ImageContainsCode;
    union
    {
        struct
        {
            UCHAR ComPlusNativeReady:1;
            UCHAR ComPlusILOnly:1;
            UCHAR ImageDynamicallyRelocated:1;
            UCHAR ImageMappedFlat:1;
            UCHAR Reserved:4;
        };
        UCHAR ImageFlags;
    };
    ULONG LoaderFlags;
    ULONG ImageFileSize;
    ULONG CheckSum;
} SECTION_IMAGE_INFORMATION, *PSECTION_IMAGE_INFORMATION;

#define MM_IMAGE_ELF_REL_OBJECT   0x00000001  /* ET_REL (.ko): map linear 1:1 berikut header & reloc */
#define MM_IMAGE_ELF_PROGRAM_HDR  0x00000002  /* ET_EXEC / ET_DYN: map per-PT_LOAD & zero .bss */

#define PAGE_SIZE 0x1000