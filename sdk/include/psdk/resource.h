#pragma once

#include <procbind.h>

#define RESOURCE_NAME_IS_STRING        0x80000000UL
#define RESOURCE_DATA_IS_DIRECTORY     0x80000000UL
#define RESOURCE_OFFSET_MASK           0x7FFFFFFFUL

#define RT_CURSOR          1
#define RT_BITMAP          2
#define RT_ICON            3
#define RT_MENU            4
#define RT_DIALOG          5
#define RT_STRING          6
#define RT_FONTDIR         7
#define RT_FONT            8
#define RT_ACCELERATOR     9
#define RT_RCDATA          10
#define RT_MESSAGETABLE    11
#define RT_GROUP_CURSOR    12
#define RT_GROUP_ICON      14
#define RT_VERSION         16
#define RT_MANIFEST        24
#define RT_THEME           256

#define MAKELANGID(Primary, Sub) \
    ((USHORT)((((USHORT)(Sub)) << 10) | (USHORT)(Primary)))

#define PRIMARYLANGID(LangId) \
    ((USHORT)((LangId) & 0x03FF))

#define SUBLANGID(LangId) \
    ((USHORT)((LangId) >> 10))

/* Some Language */
#define LANG_INDONESIAN    0x0421
#define LANG_JAPANESE      0x0411
#define LANG_NEUTRAL       0x0000
#define LANG_ENGLISH_US    0x0409

#define IS_INTRESOURCE(_r) ((((ULONG_PTR)(_r)) >> 16) == 0)
#define MAKEINTRESOURCE(i) ((CONST CHAR *)((ULONG_PTR)((USHORT)(i))))

#pragma pack(push, 1)

typedef struct _RESOURCE_DIRECTORY {
    ULONG  Characteristics;
    ULONG  TimeDateStamp;
    USHORT MajorVersion;
    USHORT MinorVersion;
    USHORT NumberOfNamedEntries;
    USHORT NumberOfIdEntries;
} RESOURCE_DIRECTORY, *PRESOURCE_DIRECTORY;

typedef struct _RESOURCE_DIRECTORY_ENTRY {
    union {
        struct {
            ULONG NameOffset   : 31;
            ULONG NameIsString : 1;
        };
        ULONG  Name;
        USHORT Id;
    };
    union {
        ULONG OffsetToData;
        struct {
            ULONG OffsetToDirectory : 31;
            ULONG DataIsDirectory   : 1;
        };
    };
} RESOURCE_DIRECTORY_ENTRY, *PRESOURCE_DIRECTORY_ENTRY;

typedef struct _RESOURCE_DIR_STRING_U {
    USHORT Length;        
    USHORT NameString[1];   
} RESOURCE_DIR_STRING_U, *PRESOURCE_DIR_STRING_U;

typedef struct _RESOURCE_DATA_ENTRY {
    ULONG OffsetToData;     /* Offset byte relatif dari __vea_rsrc_start (atau RVA module) */
    ULONG Size;             /* Ukuran data resource dalam byte */
    ULONG CodePage;         /* Misal 65001 (UTF-8) atau 1200 (UTF-16LE) */
    ULONG Reserved;         /* Harus 0 */
} RESOURCE_DATA_ENTRY, *PRESOURCE_DATA_ENTRY;

#pragma pack(pop)

/* EOF */