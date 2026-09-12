/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : init.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Initialize Loader VK parser
 ----- Effective since 08-2026 til FOREVER ------------------------------- */

#include <osi386.h>
#include <../ndk/vktypes.h>

/* Revision History ------------------------------------------------------
 * DATE       : 04-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file init.c
 * --------------------------------------------------------------------- */

#define SERVICE_BOOT_START 0x00000000

BOOLEAN             LvkParserReady = FALSE;
PUCHAR              LvkHiveBase    = NULL;
ULONG               LvkHiveLength  = 0;
PKEY_BASE_BLOCK     RootBaseBlock  = NULL;
PKEY_DIRECTORY_NODE LvkRootNode    = NULL;

PVOID
VEAPI
LvkGetCellPointer(
    IN ULONG CellOffset
)
{
    if (!LvkHiveBase || CellOffset == 0 || CellOffset >= LvkHiveLength)
    {
        return NULL;
    }

    /* Di loader, Hive bersumber dari flat buffer di RAM */
    return (PVOID)(LvkHiveBase + CellOffset);
}

VOID 
VEAPI
LvkInitializeLoaderParser(IN PBLOCK_BOOT_2 BlockBoot2)
{
    PKEY_FILE_HEADER VkeyFileHeader;
    PKEY_BASE_BLOCK  BaseBlock;

    /* Make sure Hive is already loaded on BlockBoot */
    if (BlockBoot2->SystemHiveBase == NULL || BlockBoot2->SystemHiveLength == 0)
    {
        return;
    }

    LvkHiveBase   = (PUCHAR)BlockBoot2->SystemHiveBase;
    LvkHiveLength = BlockBoot2->SystemHiveLength;

    /* Cast ke Header 'KEY1' */
    VkeyFileHeader = (PKEY_FILE_HEADER)LvkHiveBase;

    /* Validasi Magic Signature KEY1 */
    if (VkeyFileHeader->Signature != KEY_HEADER_SIG)
    {
        LdrError(STATUS_VKEY_CORRUPTED);
        return;
    }

    /* Validasi Sequence Counter Primary vs Secondary */
    if (VkeyFileHeader->PrimarySequence != VkeyFileHeader->SecondarySequence)
    {
        LdrError(STATUS_VKEY_CORRUPTED);
        return;
    }

    /* Ambil BaseBlock ('BIN1') di offset 0x1000 */
    BaseBlock = (PKEY_BASE_BLOCK)(LvkHiveBase + sizeof(KEY_FILE_HEADER));

    /* Verify signature 'BIN1' */
    if (BaseBlock->Signature != KEY_BIN_SIG)
    {
        LdrError(STATUS_VKEY_CORRUPTED);
        return;
    }

    RootBaseBlock = BaseBlock;

    /* Ambil Root Directory Node ('DN') yang berada tepat setelah KEY_BASE_BLOCK */
    LvkRootNode = (PKEY_DIRECTORY_NODE)((PUCHAR)BaseBlock + sizeof(KEY_BASE_BLOCK));
    if (LvkRootNode->Signature != KEY_DIR_SIG)
    {
        LdrError(STATUS_VKEY_CORRUPTED);
        return;
    }

    LvkParserReady = TRUE;
}

PKEY_DIRECTORY_NODE
VEAPI
LvkFindSubkey(
    IN PKEY_DIRECTORY_NODE ParentNode,
    IN PCSTR SubkeyName
)
{
    if (!ParentNode || !SubkeyName) return NULL;

    ULONG NameLen = (ULONG)RtlRawStringLength(SubkeyName);
    ULONG CurrentOff = ParentNode->FirstSubKeyOffset;

    while (CurrentOff != 0)
    {
        PKEY_DIRECTORY_NODE Node = (PKEY_DIRECTORY_NODE)LvkGetCellPointer(CurrentOff);
        if (!Node || Node->Signature != KEY_DIR_SIG) break;

        if (Node->NameLength == NameLen &&
            RtlRawStringCompareN(Node->Name, SubkeyName, NameLen) == 0)
        {
            return Node;
        }

        CurrentOff = Node->NextSiblingOffset;
    }

    return NULL;
}

PKEY_FILE_NODE
VEAPI
LvkFindValueNode(
    IN PKEY_DIRECTORY_NODE DirNode,
    IN PCSTR ValueName
)
{
    if (!DirNode || !ValueName) return NULL;

    ULONG NameLen = (ULONG)RtlRawStringLength(ValueName);
    ULONG CurrentValOff = DirNode->FirstValueOffset;

    while (CurrentValOff != 0)
    {
        PKEY_FILE_NODE FileNode = (PKEY_FILE_NODE)LvkGetCellPointer(CurrentValOff);
        if (!FileNode || FileNode->Signature != KEY_FILE_SIG) break;

        if (FileNode->NameLength == NameLen &&
            RtlRawStringCompareN(FileNode->Name, ValueName, NameLen) == 0)
        {
            return FileNode;
        }

        CurrentValOff = FileNode->NextValueOffset;
    }

    return NULL;
}

PKEY_DIRECTORY_NODE
VEAPI
LvkOpenKey(
    IN PCSTR KeyPath
)
{
    if (!LvkParserReady || !KeyPath || !LvkRootNode) return NULL;

    PKEY_DIRECTORY_NODE CurrentNode = LvkRootNode;
    CHAR PathBuffer[256];
    ULONG PathLen = (ULONG)RtlRawStringLength(KeyPath);

    if (PathLen >= sizeof(PathBuffer)) return NULL;

    RtlCopyMemory(PathBuffer, (PVOID)KeyPath, PathLen);
    PathBuffer[PathLen] = '\0';

    PCHAR Token = PathBuffer;
    while (*Token == '/') Token++;

    while (*Token != '\0')
    {
        PCHAR TokenEnd = Token;
        while (*TokenEnd != '\0' && *TokenEnd != '/') TokenEnd++;

        CHAR SaveChar = *TokenEnd;
        *TokenEnd = '\0';

        if (RtlRawStringLength(Token) > 0)
        {
            CurrentNode = LvkFindSubkey(CurrentNode, Token);
            if (!CurrentNode) return NULL;
        }

        if (SaveChar == '\0') break;
        Token = TokenEnd + 1;
        while (*Token == '/') Token++;
    }

    return CurrentNode;
}

VEASTATUS
VEAPI
LvkQueryValueKey(
    IN PKEY_DIRECTORY_NODE KeyNode,
    IN PCSTR ValueName,
    OUT PULONG Type OPTIONAL,
    OUT PVOID Data OPTIONAL,
    IN ULONG DataSize,
    OUT PULONG ResultLength OPTIONAL
)
{
    if (!LvkParserReady || !KeyNode || !ValueName)
        return STATUS_INVALID_PARAMETER;

    PKEY_FILE_NODE FileNode = LvkFindValueNode(KeyNode, ValueName);
    if (!FileNode) return STATUS_OBJECT_NAME_NOT_FOUND;

    if (Type) *Type = FileNode->Type;
    if (ResultLength) *ResultLength = FileNode->DataLength;

    PVOID ValueData = NULL;

    /* Handle Inline vs Out-of-line Data Cell Offset */
    if ((FileNode->Flags & KEY_FILE_INLINE) || FileNode->DataLength <= sizeof(ULONG))
    {
        /* Inline Data: Data tersimpan di field DataPOffset */
        ValueData = &FileNode->DataPOffset;
    }
    else
    {
        /* Out-of-line Data: DataPOffset menunjuk Cell Data */
        PUCHAR CellPtr = (PUCHAR)LvkGetCellPointer(FileNode->DataPOffset);
        if (!CellPtr) return STATUS_OBJECT_TYPE_MISMATCH;

        /* Lewati Header CellSize 4 Byte (sizeof(LONG))! */
        ValueData = (PVOID)(CellPtr + sizeof(LONG));
    }

    if (Data && DataSize > 0)
    {
        ULONG BytesToCopy = (DataSize < FileNode->DataLength) ? DataSize : FileNode->DataLength;
        RtlCopyMemory(Data, ValueData, BytesToCopy);

        if (DataSize < FileNode->DataLength)
            return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
}

VOID
VEAPI
LvkNormalizeDriverPath(
    IN PCSTR RawPath,
    OUT PCHAR OutPath,
    IN ULONG MaxLen
)
{
    if (!RawPath || !OutPath || MaxLen == 0) return;

    CHAR TempBuf[256];
    ULONG RawLen = (ULONG)RtlRawStringLength(RawPath);
    if (RawLen >= sizeof(TempBuf)) RawLen = sizeof(TempBuf) - 1;

    RtlCopyMemory(TempBuf, (PVOID)RawPath, RawLen);
    TempBuf[RawLen] = '\0';

    /* 1. Ubah backslash '\' menjadi slash '/' */
    for (ULONG i = 0; i < RawLen; i++)
    {
        if (TempBuf[i] == '\\') TempBuf[i] = '/';
    }

    PCHAR Cursor = TempBuf;

    /* 2. Deteksi & lewati prefix %SYSTEMROOT% / %SystemRoot% */
    if (RtlRawStringCompareN(Cursor, "%SYSTEMROOT%", 12) == 0 ||
        RtlRawStringCompareN(Cursor, "%SystemRoot%", 12) == 0)
    {
        Cursor += 12;
    }

    /* Skip leading slash jika ada */
    while (*Cursor == '/') Cursor++;

    /* 3. Rakit path baru dengan prefix /VeaOS/ */
    ULONG CursorLen = (ULONG)RtlRawStringLength(Cursor);
    
    /* Prepend /VeaOS/ ke target path */
    const CHAR Prefix[] = "/VeaOS/";
    ULONG PrefixLen = sizeof(Prefix) - 1;

    if (PrefixLen + CursorLen >= MaxLen) return;

    RtlCopyMemory(OutPath, (PVOID)Prefix, PrefixLen);
    RtlCopyMemory(OutPath + PrefixLen, Cursor, CursorLen);
    OutPath[PrefixLen + CursorLen] = '\0';
}

BOOLEAN
VEAPI
LvkLoadBootDrivers(VOID)
{
    if (!LvkParserReady) return FALSE;
    BvClearScreen();
    BvRerenderLayout();

    /* Get ControlSet001 */
    PKEY_DIRECTORY_NODE ServicesNode = LvkOpenKey("/ControlSet001/Services");
    if (!ServicesNode)
    {
        return FALSE;
    }

    ULONG CurrentSubkeyOff = ServicesNode->FirstSubKeyOffset;

    while(CurrentSubkeyOff != 0)
    {
        PKEY_DIRECTORY_NODE ServiceNode = (PKEY_DIRECTORY_NODE)LvkGetCellPointer(CurrentSubkeyOff);
        if (!ServiceNode || ServiceNode->Signature != KEY_DIR_SIG)
        {
            LdrWarning("CORRUPTED CELL.\n");
        }

        CHAR ServiceName[128];
        ULONG NameLen = (ServiceNode->NameLength < 127) ? ServiceNode->NameLength : 127;
        RtlCopyMemory(ServiceName, ServiceNode->Name, NameLen);
        ServiceName[NameLen] = '\0';

        ULONG StartType = 0xFFFFFFFF;
        VEASTATUS Status = LvkQueryValueKey(
            ServiceNode, 
            "Start", 
            NULL, 
            &StartType, 
            sizeof(StartType), 
            NULL
        );

        if(VEA_SUCCESS(Status) && StartType == SERVICE_BOOT_START)
        {
            CHAR RawImagePath[256];
            ULONG ResultLen = 0;

            Status = LvkQueryValueKey(
                ServiceNode,
                "ImagePath",
                NULL,
                RawImagePath,
                sizeof(RawImagePath) - 1,
                &ResultLen
            );

            if (VEA_SUCCESS(Status) && ResultLen > 0)
            {
                RawImagePath[ResultLen] = '\0';
                
                /* Normalisasi Path */
                CHAR ResolvedPath[256];
                LvkNormalizeDriverPath(RawImagePath, ResolvedPath, sizeof(ResolvedPath));

                /* Panggil Loader File System untuk memuat driver binary */
                Status = LdrInitLoadBootDriver(ServiceName, ResolvedPath);
                if(VEA_SUCCESS(Status))
                {
                    BvPrintLog("Loading: ");
                    BvPrintLog(RawImagePath);
                    BvPrintLog("\n");
                }
            }
            else
            {
                /* Fallback jika ImagePath tidak terdefinisi:
                 * Default ke /VeaOS/drivers/<ServiceName>.sys atau .ko */
            }
        }

        CurrentSubkeyOff = ServiceNode->NextSiblingOffset;
    }

    return STATUS_SUCCESS;
}