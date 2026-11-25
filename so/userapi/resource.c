/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : resource.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : Resource finding
 ----- Effective since 11-2026 til FOREVER ------------------------------- */

#include <userapi.h>
#include "userapi_priv.h"

/* Revision History ------------------------------------------------------
 * DATE       : 24-11-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file resource.c
 * --------------------------------------------------------------------- */

static CONST UCHAR *
UserGetModuleRsrcBase(
    IN HMODULE hModule
)
{
    if (!hModule || hModule == gUserApiModule)
    {
        return VeaResourceStart;
    }

    return (CONST UCHAR *)GetProcAddress(hModule, "VeaResourceStart");
}

static BOOLEAN
UserMatchResourceEntry(
    IN CONST UCHAR                    *RsrcBase,
    IN CONST RESOURCE_DIRECTORY_ENTRY *Entry,
    IN CONST CHAR                     *NameOrId
)
{
    if (IS_INTRESOURCE(NameOrId))
    {
        return (!Entry->NameIsString && Entry->Id == (USHORT)(ULONG_PTR)NameOrId);
    }

    if (!Entry->NameIsString)
    {
        return FALSE;
    }

    CONST RESOURCE_DIR_STRING_U *DirStr =
        (CONST RESOURCE_DIR_STRING_U *)(RsrcBase + Entry->NameOffset);

    for (USHORT i = 0; i < DirStr->Length; i++)
    {
        CHAR c1 = NameOrId[i];
        CHAR c2 = (CHAR)(DirStr->NameString[i] & 0xFF);
        if (c1 == '\0') return FALSE;
        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
        if (c1 != c2) return FALSE;
    }

    return (NameOrId[DirStr->Length] == '\0');
}

static CONST RESOURCE_DIRECTORY_ENTRY *
UserScanResourceDir(
    IN CONST UCHAR              *RsrcBase,
    IN CONST RESOURCE_DIRECTORY *Dir,
    IN CONST CHAR               *NameOrId
)
{
    CONST RESOURCE_DIRECTORY_ENTRY *Entries =
        (CONST RESOURCE_DIRECTORY_ENTRY *)(Dir + 1);

    ULONG Total = (ULONG)Dir->NumberOfNamedEntries + (ULONG)Dir->NumberOfIdEntries;
    ULONG Start = IS_INTRESOURCE(NameOrId) ? Dir->NumberOfNamedEntries : 0;
    ULONG End   = IS_INTRESOURCE(NameOrId) ? Total : Dir->NumberOfNamedEntries;

    for (ULONG i = Start; i < End; i++)
    {
        if (UserMatchResourceEntry(RsrcBase, &Entries[i], NameOrId))
        {
            return &Entries[i];
        }
    }

    return NULL;
}

CONST RESOURCE_DATA_ENTRY *
VEAPI
UserFindResourceEx(
    IN HMODULE     hModule,
    IN CONST CHAR *lpType,
    IN CONST CHAR *lpName,
    IN USHORT      wLanguage
)
{
    CONST UCHAR *RsrcBase = UserGetModuleRsrcBase(hModule);
    if (!RsrcBase)
    {
        return NULL;
    }

    CONST RESOURCE_DIRECTORY *RootDir = (CONST RESOURCE_DIRECTORY *)RsrcBase;
    CONST RESOURCE_DIRECTORY_ENTRY *TypeEntry = UserScanResourceDir(RsrcBase, RootDir, lpType);
    if (!TypeEntry || !TypeEntry->DataIsDirectory)
    {
        return NULL;
    }

    CONST RESOURCE_DIRECTORY *NameDir =
        (CONST RESOURCE_DIRECTORY *)(RsrcBase + TypeEntry->OffsetToDirectory);
    CONST RESOURCE_DIRECTORY_ENTRY *NameEntry = UserScanResourceDir(RsrcBase, NameDir, lpName);
    if (!NameEntry || !NameEntry->DataIsDirectory)
    {
        return NULL;
    }

    CONST RESOURCE_DIRECTORY *LangDir =
        (CONST RESOURCE_DIRECTORY *)(RsrcBase + NameEntry->OffsetToDirectory);
    CONST RESOURCE_DIRECTORY_ENTRY *LangEntry =
        UserScanResourceDir(RsrcBase, LangDir, MAKEINTRESOURCE(wLanguage));

    if (!LangEntry && wLanguage != LANG_NEUTRAL)
    {
        LangEntry = UserScanResourceDir(RsrcBase, LangDir, MAKEINTRESOURCE(LANG_NEUTRAL));
    }

    if (!LangEntry && (LangDir->NumberOfNamedEntries + LangDir->NumberOfIdEntries) > 0)
    {
        LangEntry = (CONST RESOURCE_DIRECTORY_ENTRY *)(LangDir + 1);
    }

    if (!LangEntry || LangEntry->DataIsDirectory)
    {
        return NULL;
    }

    return (CONST RESOURCE_DATA_ENTRY *)(RsrcBase + LangEntry->OffsetToData);
}

CONST VOID *
VEAPI
UserLockResource(
    IN HMODULE                    hModule,
    IN CONST RESOURCE_DATA_ENTRY *pResData
)
{
    CONST UCHAR *RsrcBase = UserGetModuleRsrcBase(hModule);
    if (!RsrcBase || !pResData)
    {
        return NULL;
    }
    return (CONST VOID *)(RsrcBase + pResData->OffsetToData);
}

ULONG
VEAPI
UserSizeofResource(
    IN HMODULE                    hModule,
    IN CONST RESOURCE_DATA_ENTRY *pResData
)
{
    (VOID)hModule;
    return pResData ? pResData->Size : 0;
}

FARPROC
VEAPI
GetProcAddress(
    IN HMODULE hModule,
    IN PCSTR   lpProcName
)
{
    if (!hModule)
    {
        /* Default ke image base executable utama (0x00400000 di VeaUserModules.cmake) */
        hModule = (HMODULE)0x00400000UL;
    }

    return (FARPROC)LdrGetProcedureAddress((PVOID)hModule, lpProcName);
}

HRSRC
VEAPI
FindResourceExA(
    IN HMODULE     hModule,
    IN CONST CHAR *lpType,
    IN CONST CHAR *lpName,
    IN USHORT      wLanguage
)
{
    return (HRSRC)UserFindResourceEx(hModule, lpType, lpName, wLanguage);
}

HRSRC
VEAPI
FindResourceA(
    IN HMODULE     hModule,
    IN CONST CHAR *lpName,
    IN CONST CHAR *lpType
)
{
    /* Perhatikan urutan parameter Win32: FindResource(hMod, lpName, lpType) */
    return (HRSRC)UserFindResourceEx(hModule, lpType, lpName, LANG_NEUTRAL);
}

HGLOBAL
VEAPI
LoadResource(
    IN HMODULE hModule,
    IN HRSRC   hResInfo
)
{
    return (HGLOBAL)UserLockResource(hModule, (CONST RESOURCE_DATA_ENTRY *)hResInfo);
}

PVOID
VEAPI
LockResource(
    IN HGLOBAL hResData
)
{
    return (PVOID)hResData;
}

ULONG
VEAPI
SizeofResource(
    IN HMODULE hModule,
    IN HRSRC   hResInfo
)
{
    return UserSizeofResource(hModule, (CONST RESOURCE_DATA_ENTRY *)hResInfo);
}

INT
VEAPI
LoadStringA(
    IN  HINSTANCE hInstance,
    IN  UINT      uID,
    OUT PSTR     lpBuffer,
    IN  INT       cchBufferMax
)
{
    if (!lpBuffer || cchBufferMax <= 0)
    {
        return 0;
    }

    lpBuffer[0] = '\0';

    USHORT BlockId = (USHORT)((uID >> 4) + 1);
    UINT   Index   = uID & 0x0F;

    HRSRC hRes = FindResourceExA(
        (HMODULE)hInstance,
        MAKEINTRESOURCE(RT_STRING),
        MAKEINTRESOURCE(BlockId),
        LANG_NEUTRAL
    );
    if (!hRes)
    {
        return 0;
    }

    ULONG BlockSize = SizeofResource((HMODULE)hInstance, hRes);
    CONST UCHAR *Ptr = (CONST UCHAR *)LockResource(LoadResource((HMODULE)hInstance, hRes));
    if (!Ptr || BlockSize < sizeof(USHORT))
    {
        return 0;
    }

    CONST UCHAR *End = Ptr + BlockSize;
    for (UINT i = 0; i <= Index; i++)
    {
        if (Ptr + sizeof(USHORT) > End)
        {
            return 0;
        }

        USHORT StrLen = *(CONST USHORT *)Ptr;
        Ptr += sizeof(USHORT);

        if (Ptr + (SIZE_T)StrLen * sizeof(USHORT) > End)
        {
            return 0;
        }

        if (i == Index)
        {
            CONST USHORT *WStr = (CONST USHORT *)Ptr;
            INT CopyLen = (INT)StrLen;
            if (CopyLen >= cchBufferMax)
            {
                CopyLen = cchBufferMax - 1;
            }

            for (INT c = 0; c < CopyLen; c++)
            {
                lpBuffer[c] = (WStr[c] < 0x80) ? (CHAR)WStr[c] : '?';
            }
            lpBuffer[CopyLen] = '\0';
            return CopyLen;
        }

        Ptr += (SIZE_T)StrLen * sizeof(USHORT);
    }

    return 0;
}

/* EOF */