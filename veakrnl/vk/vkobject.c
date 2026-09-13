/* VeaOS SPDX License ----------------------------------------------------
   SPDX-License-Identifier: GPL-2.0-only
   LICENSE     : GNU General Public License v2.0
   FILE        : vkobject.c
   CREATOR     : RasyaFR-Engkol
   MAINTAINER  : VeaOS Team
   PURPOSE     : VeaKey Subsystem
 ----- Effective since 07-2026 til FOREVER ------------------------------- */

#include <veakrnl.h>

/* Revision History ------------------------------------------------------
 * DATE       : 30-07-2026
 * NAME       : RasyaFR-Engkol
 * REVISION   : Creating file vkobject.c
 * --------------------------------------------------------------------- 
 * DATE       : 01-08-2026
 * NAME       : RasyaFR-Engkol
 * REVISION:  : VkpParseKey supporting Symlink
 * --------------------------------------------------------------------- */

PVOID
VEAPI
VkpGetCellPointer(
    PVK_HIVE Hive,
    ULONG CellOffset
);

static PCHAR VkpGetNextToken(PCHAR Path, PCHAR* SavePtr) {
    PCHAR Str = Path ? Path : *SavePtr;
    if (!Str || *Str == '\0') return NULL;

    // Skip leading slashes '/'
    while (*Str == '/') Str++;
    if (*Str == '\0') return NULL;

    PCHAR TokenStart = Str;
    while (*Str != '\0' && *Str != '/') 
    {
        Str++;
    }

    if (*Str == '/') 
    {
        *Str = '\0';
        *SavePtr = Str + 1;
    } 
    else 
    {
        *SavePtr = Str;
    }

    return TokenStart;
}

VEASTATUS
VEAPI
VkpParseKeyObject(
    PVOID ParseObject,
    POB_LOOKUP_CONTEXT LookupContext,
    PVOID *ResolvedObject
)
{
    PVK_KEY_OBJECT ParentKeyObject = (PVK_KEY_OBJECT)ParseObject;
    PVK_HIVE Hive = ParentKeyObject->HiveBase;
    PKEY_DIRECTORY_NODE CurrentNode = ParentKeyObject->KeyNode;
    PUCHAR HiveBuffer = (PUCHAR)Hive->BaseAddress;

    // Make sure Lookup is OK
    if (!LookupContext || !ResolvedObject) {
        return STATUS_INVALID_PARAMETER;
    }

    if (LookupContext->CaseInsensitive) {
        return STATUS_INVALID_PARAMETER;
    }

    // Make sure it is valid access mask
    POBJECT_TYPE TargetObjectType = LookupContext->ExpectedType 
                                    ? LookupContext->ExpectedType 
                                    : VkKeyObjectType;

    if (TargetObjectType != NULL && TargetObjectType->ValidAccessMask != 0)
    {
        // Make sure the right bit is passed and not a wrong bit
        if ((LookupContext->DesiredAccess & ~TargetObjectType->ValidAccessMask) != 0)
        {
            KdPrintf("[Vk] ERROR | Access mask 0x%X contains invalid bits for ObjectType (Valid: 0x%X)\n",
                     LookupContext->DesiredAccess,
                     TargetObjectType->ValidAccessMask);
            return STATUS_ACCESS_DENIED; 
        }
    }

    // If no remaining names, that means the target is ROOT itself
    if (LookupContext->RemainingName.Length == 0 ||
        LookupContext->RemainingName.Buffer == NULL)
    {
        *ResolvedObject = ParentKeyObject;
        return STATUS_SUCCESS;
    }

    // The remainingname condition is ofc the key path to KEY_FILE, or 
    // KEY_DIRECTORY. we need to parse it 1 by 1
    //
    // FIXME: We must use flexible PathBuffer
    CHAR PathBuffer[256];
    ULONG CopyLen = (LookupContext->RemainingName.Length < 255) 
                    ? LookupContext->RemainingName.Length 
                    : 255;
    RtlCopyMemory(PathBuffer, LookupContext->RemainingName.Buffer, CopyLen);
    PathBuffer[CopyLen] = '\0';

    // Tokenize it      
    PCHAR SavePtr = NULL;
    PCHAR Token = VkpGetNextToken(PathBuffer, &SavePtr);

    PKEY_DIRECTORY_NODE FoundChild = NULL;
    ULONG FoundChildOffset = 0;

    // Traverse the object through or Linked List
    while (Token != NULL)
    {
        ULONG TokenLen = (ULONG)RtlRawStringLength(Token);
        
        // Panggil helper universal kita!
        FoundChild = VkpFindSubkey(Hive, CurrentNode, Token, TokenLen, &FoundChildOffset);

        if (!FoundChild)
        {
            // Handling Insert Mode (buat VkCreateKey)
            if (LookupContext->InsertMode) {
                PCHAR NextToken = VkpGetNextToken(NULL, &SavePtr);
                if (NextToken == NULL) {
                    RtlCopyMemory(LookupContext->ReparseBuffer, Token, TokenLen + 1);
                    RtlInitAnsiString(&LookupContext->RemainingName, LookupContext->ReparseBuffer);

                    *ResolvedObject = ParentKeyObject; 
                    return STATUS_SUCCESS; 
                }
            }

            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        if (FoundChild->Flags & KEY_DIR_SYMLINK)
        {
            ANSI_STRING SymValueName;
            RtlInitAnsiString(&SymValueName, "SymbolicLinkValue");

            PKEY_FILE_NODE LinkValue = VkpFindValueNode(Hive, FoundChild, &SymValueName, NULL);
            if (!LinkValue || LinkValue->Type != FILE_KEY_LINK)
            {
                return STATUS_OBJECT_NAME_INVALID;
            }

            PCHAR TargetPath = NULL;
            if ((LinkValue->Flags & KEY_FILE_INLINE) || LinkValue->DataLength <= sizeof(ULONG))
            {
                TargetPath = (PCHAR)&LinkValue->DataPOffset;
            }
            else
            {
                PUCHAR CellPtr = (PUCHAR)VkpGetCellPointer(Hive, LinkValue->DataPOffset);
                if (!CellPtr)
                {
                    return STATUS_OBJECT_NAME_INVALID;
                }
                TargetPath = (PCHAR)(CellPtr + sizeof(LONG)); // Lewati 4-byte CellSize!
            }

            ULONG TargetLen = (ULONG)RtlRawStringLength(TargetPath);

            // SavePtr sekarang nunjuk ke sisa path SETELAH token ini (belum diproses)
            // karena VkpGetNextToken sebelumnya udah mutilate PathBuffer & maju-in SavePtr
            PCHAR Remainder = SavePtr;
            ULONG RemainderLen = (Remainder && *Remainder) ? (ULONG)RtlRawStringLength(Remainder) : 0;

            ULONG TotalLen = TargetLen + (RemainderLen > 0 ? 1 + RemainderLen : 0);
            if (TotalLen >= sizeof(LookupContext->ReparseBuffer))
            {
                return STATUS_NAME_TOO_LONG;
            }

            // Gabung: "<TargetPath>" + "/" + "<sisa token yang belum diproses>"
            ULONG Pos = 0;
            RtlCopyMemory(LookupContext->ReparseBuffer, TargetPath, TargetLen);
            Pos = TargetLen;

            if (RemainderLen > 0)
            {
                LookupContext->ReparseBuffer[Pos++] = '/';
                RtlCopyMemory(LookupContext->ReparseBuffer + Pos, Remainder, RemainderLen);
                Pos += RemainderLen;
            }
            LookupContext->ReparseBuffer[Pos] = '\0';

            RtlInitAnsiString(&LookupContext->RemainingName, LookupContext->ReparseBuffer);

            // Balikin STATUS_REPARSE 
            return STATUS_REPARSE;
        }

        CurrentNode = FoundChild;
        Token = VkpGetNextToken(NULL, &SavePtr);
    }

    // Make new node
    PVK_KEY_OBJECT NewKeyObject = NULL;
    OBJECT_ATTRIBUTES ObjAttr;
    RtlZeroMemory(&ObjAttr, sizeof(OBJECT_ATTRIBUTES));

    VEASTATUS Status = ObCreateObject(
        KernelMode,
        LookupContext->ExpectedType ? LookupContext->ExpectedType : VkKeyObjectType,
        &ObjAttr,
        KernelMode,
        NULL,
        sizeof(VK_KEY_OBJECT),
        0,
        0,
        (PVOID)&NewKeyObject
    );

    if (!VEA_SUCCESS(Status)) {
        return Status;
    }

    // Set our new data
    RtlZeroMemory(NewKeyObject, sizeof(VK_KEY_OBJECT));
    NewKeyObject->HiveBase    = Hive;
    NewKeyObject->KeyNode     = FoundChild;
    NewKeyObject->KeyOffset   = FoundChildOffset;

    RtlZeroMemory(&LookupContext->RemainingName, sizeof(ANSI_STRING));

    *ResolvedObject = NewKeyObject;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VkpQueryValueKey(
    IN PVK_KEY_OBJECT KeyObject,
    IN PANSI_STRING ValueName,
    OUT PULONG Type,
    OUT PVOID Data,
    IN ULONG DataSize,
    OUT PULONG ResultLength
)
{
    // Basic checking
    if (!KeyObject || !KeyObject->HiveBase || !ValueName)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PVK_HIVE Hive = KeyObject->HiveBase;
    PKEY_DIRECTORY_NODE KeyDirNode = (PKEY_DIRECTORY_NODE)KeyObject->KeyNode;

    if (!KeyDirNode || KeyDirNode->Signature != KEY_DIR_SIG)
    {
        KdPrintf("[Vk] ERROR | Invalid Directory Node!\n");
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    // Use oour helper
    PKEY_FILE_NODE FileNode = VkpFindValueNode(Hive, KeyDirNode, ValueName, NULL);

    if (!FileNode)
    {
        // Value tidak ditemukan
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    // Metadata filling
    if (Type != NULL)
        *Type = FileNode->Type;
    
    if (ResultLength != NULL)
        *ResultLength = FileNode->DataLength;

    // Resolve Data Payload Pointer
    PVOID ValueData = NULL;

    if (FileNode->DataLength <= sizeof(ULONG))
    {
        // Inline Data 
        ValueData = &FileNode->DataPOffset;
    }
    else
    {
        // Out-of-line Data (Cell Offset)
        PUCHAR CellPtr = (PUCHAR)VkpGetCellPointer(Hive, FileNode->DataPOffset);
        if (CellPtr != NULL)
        {
            ValueData = (PVOID)(CellPtr + sizeof(LONG));
        }
    }

    if (!ValueData)
    {
        KdPrintf("[Vk] ERROR | Invalid DataPOffset 0x%X\n", FileNode->DataPOffset);
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    // Safe Buffer Copying
    ULONG BytesToCopy = FileNode->DataLength;
    BOOLEAN IsCallerBufferTooSmall = FALSE;

    if (DataSize < FileNode->DataLength)
    {
        BytesToCopy = DataSize;
        IsCallerBufferTooSmall = TRUE;
    }

    if (Data != NULL && BytesToCopy > 0)
    {
        RtlCopyMemory(Data, ValueData, BytesToCopy);
    }

    if (IsCallerBufferTooSmall)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
}

//
// Last TODO for nextday
//
// 1. Do volatile checking
// 2. VOLATILE
// 3. Create VeaCreateKey, and it's dispo. 
//    are these volatile or stable KEY
PVOID
VEAPI
VkpGetCellPointer(
    PVK_HIVE Hive,
    ULONG CellOffset
)
{
    if (!Hive || CellOffset == 0)
        return NULL;

    ULONG RawOffset = VkGetRawOffset(CellOffset);

    // Buka dari Volatile Pool
    if (VkIsVolatileOffset(CellOffset))
    {
        ULONG BinIndex    = RawOffset / VK_BIN_SIZE; 
        ULONG LocalOffset = RawOffset & VK_VOLATILE_BIN_MASK; 

        if (BinIndex >= Hive->VolatileBinCount || !Hive->VolatileBins[BinIndex])
        {
            KdPrintf("[Vk] ERROR | Invalid Volatile Bin Index: %u\n", BinIndex);
            return NULL;
        }

        return (PVOID)((PUCHAR)Hive->VolatileBins[BinIndex] + LocalOffset);
    }

    for(ULONG i = 0; i < Hive->StableBinCount; i++)
    {
        PVK_STABLE_BIN Bin = &Hive->StableBins[i];

        if (RawOffset >= Bin->BinBaseOffset &&
            RawOffset <  Bin->BinBaseOffset + Bin->BinSize)
        {
            ULONG LocalOffset = RawOffset - Bin->BinBaseOffset;
            return (PVOID)((PUCHAR)Bin->BinBase + LocalOffset);
        }
    }

    KdPrintf("[Vk] ERROR | Stable Offset 0x%X not found in any BIN\n", RawOffset);
    return (PVOID)((PUCHAR)Hive->BaseAddress + RawOffset);
}

//
// For allocating Volatile Cell
//
PVOID
VEAPI
VkpAllocateVolatileCell(
    IN PVK_HIVE Hive,
    IN ULONG Size,
    OUT PULONG RawVolatileOffset
)
{
    if (!Hive || Size == 0 || !RawVolatileOffset)
    {
        return NULL;
    }

    // Align them so cpu happy :). 4 is best option
    ULONG AllignedSize = VK_ALIGN_UP(Size, 4); 

    // Condition:
    // 1. No Volatile Bin
    // 2. CurrentBinFreeOffset is nearly full
    if(Hive->VolatileBinCount == 0 ||
       (Hive->CurrentBinFreeOffset + AllignedSize) > VK_BIN_SIZE)
    {
        VEASTATUS Status;
        
        // Allocate new Volatile Cell
        Status = HvAllocateVolatileCell(Hive);
        if(!VEA_SUCCESS(Status))
        {
            return NULL;
        }
    }

    // Get active bin information now
    ULONG ActiveBinIndex = Hive->VolatileBinCount - 1;
    PUCHAR BinBase = (PUCHAR)Hive->VolatileBins[ActiveBinIndex];
    
    *RawVolatileOffset = (ActiveBinIndex * VK_BIN_SIZE) + Hive->CurrentBinFreeOffset;

    PVOID AllocatedCell = (PVOID)(BinBase + Hive->CurrentBinFreeOffset);
    Hive->CurrentBinFreeOffset += AllignedSize;

    return AllocatedCell;
}

//
// Routine to attach Volatile Key
//
VEASTATUS
VEAPI
VkpAttachVolatileSubkey(
    IN PVK_HIVE Hive,
    IN ULONG ParentOffset,
    IN PANSI_STRING SubkeyName,
    OUT PULONG CreatedSubkeyOffset
)
{
    // Basic validation
    if (!Hive || !SubkeyName || !SubkeyName->Buffer || SubkeyName->Length == 0 || !CreatedSubkeyOffset)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Get Parent Node
    PKEY_DIRECTORY_NODE ParentNode = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, ParentOffset);
    if (!ParentNode || ParentNode->Signature != KEY_DIR_SIG)
    {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    // Cek Duplication
    ULONG ExistingOffset = 0;
    PKEY_DIRECTORY_NODE ExistingNode = VkpFindSubkey(
        Hive, 
        ParentNode, 
        SubkeyName->Buffer, 
        SubkeyName->Length, 
        &ExistingOffset
    );

    if (ExistingNode != NULL)
    {
        // Subkey dengan nama ini sudah ada!
        *CreatedSubkeyOffset = ExistingOffset;
        return STATUS_OBJECT_NAME_COLLISION; 
    }

    // Calculate size for this cell
    USHORT NameLen = SubkeyName->Length;
    ULONG  NodeSize = sizeof(KEY_DIRECTORY_NODE) + NameLen;

    ULONG RawVolatileOffset = 0;
    PKEY_DIRECTORY_NODE NewVolatileNode = (PKEY_DIRECTORY_NODE)VkpAllocateVolatileCell(
        Hive, 
        NodeSize, 
        &RawVolatileOffset
    );
    
    if (!NewVolatileNode)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    // Make Volatile Key Offset (Bit 0x80000000 SET)
    ULONG VolatileCellOffset = VkMakeVolatileOffset(RawVolatileOffset);

    // Fill Metadata
    RtlZeroMemory(NewVolatileNode, NodeSize);
    NewVolatileNode->CellSize           = -(LONG)NodeSize; // Negative = Used
    NewVolatileNode->Signature          = KEY_DIR_SIG;     // 'DN'
    NewVolatileNode->Flags              = KEY_FILE_VOLATILE;
    NewVolatileNode->ParentOffset       = ParentOffset;  
    NewVolatileNode->FirstSubKeyOffset  = 0;
    NewVolatileNode->NextSiblingOffset  = 0;
    NewVolatileNode->FirstValueOffset   = 0;
    NewVolatileNode->SubKeyCount        = 0;
    NewVolatileNode->NameLength         = NameLen;
    RtlCopyMemory(NewVolatileNode->Name, SubkeyName->Buffer, NameLen);

    // Link
    if (ParentNode->FirstSubKeyOffset == 0)
    {
        // Fast path: Parent doesn't have KEY_DN
        ParentNode->FirstSubKeyOffset = VolatileCellOffset;
    }
    else
    {
        // Slow path: Unfortunately, parent has KEY_DN attached
        // with another
        ULONG CurrSiblingOff = ParentNode->FirstSubKeyOffset;

        while (CurrSiblingOff != 0)
        {
            PKEY_DIRECTORY_NODE CurrSibling = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, CurrSiblingOff);

            if (!CurrSibling || CurrSibling->Signature != KEY_DIR_SIG)
            {
                // Sibling chain corrupted
                return STATUS_OBJECT_TYPE_MISMATCH;
            }

            if (CurrSibling->NextSiblingOffset == 0)
            {
                CurrSibling->NextSiblingOffset = VolatileCellOffset;
                break;
            }

            CurrSiblingOff = CurrSibling->NextSiblingOffset;
        }
    }

    // Update Parent Metadata
    ParentNode->SubKeyCount++;

    // Mark dirty the ParentOffset. go take a bath
    VkpMarkCellDirty(Hive, ParentOffset);

    *CreatedSubkeyOffset = VolatileCellOffset;
    return STATUS_SUCCESS;
}

//
// Routine to create new KEY and attach it ANYWHERE
//
VEASTATUS
VEAPI
VkpAttachVolatileKey(
    IN PVK_HIVE Hive,
    IN ULONG ParentOffset,
    IN PANSI_STRING ValueName,
    IN ULONG Type,
    IN PVOID Data,
    IN ULONG DataSize,
    OUT PULONG CreatedValueOffset
)
{
    // Condition
    // 1. No Hive
    // 2. No ValueName or ValueName invalid
    // 3. No CreateValueOffset
    if (!Hive || 
        !ValueName || 
        !ValueName->Buffer || 
        ValueName->Length == 0 || 
        !CreatedValueOffset)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Take our Parent Directory
    PKEY_DIRECTORY_NODE ParentNode = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, ParentOffset);
    if (!ParentNode || ParentNode->Signature != KEY_DIR_SIG)
    {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    // Check duplication
    PKEY_FILE_NODE ExistingValue = VkpFindValueNode(Hive, ParentNode, ValueName, NULL);
    if(ExistingValue)
    {
        return STATUS_OBJECT_NAME_COLLISION;
    }

    // Check wether the data should be inlined or external
    ULONG DataPOffset = 0;

    if (DataSize <= sizeof(ULONG))
    {
        if (Data != NULL && DataSize > 0)
        {
            RtlCopyMemory(&DataPOffset, Data, DataSize);
        }
    }
    else
    {
        // SlowPath haha
        if (Data == NULL)
        {
            return STATUS_INVALID_PARAMETER;
        }

        ULONG RawDataOff = 0;
        ULONG CellAllocSize = DataSize + sizeof(LONG);
        PVOID DataCell = VkpAllocateVolatileCell(Hive, CellAllocSize, &RawDataOff);
        if (!DataCell)
        {
            return STATUS_INSUFFICIENT_MEMORY;
        }

        // Copy out Payload instantly
        *(PLONG)DataCell = -(LONG)CellAllocSize;
        RtlCopyMemory((PUCHAR)DataCell + sizeof(LONG), Data, DataSize);

        DataPOffset = VkMakeVolatileOffset(RawDataOff);
    }

    // Allocate Volatile Cell for our KEY_FN
    USHORT NameLen = ValueName->Length;
    ULONG  NodeSize = sizeof(KEY_FILE_NODE) + NameLen;
    ULONG RawNodeOff = 0;
    PKEY_FILE_NODE NewFileNode = (PKEY_FILE_NODE)VkpAllocateVolatileCell(
        Hive, 
        NodeSize, 
        &RawNodeOff
    );

    if (!NewFileNode)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    // Fill out KEY_FN metadata
    ULONG VolatileValueOffset = VkMakeVolatileOffset(RawNodeOff);

    RtlZeroMemory(NewFileNode, NodeSize);
    NewFileNode->CellSize        = -(LONG)NodeSize; // Negatif = Cell Terpakai
    NewFileNode->Signature       = KEY_FILE_SIG;    // 'FN' (0x4E46)
    NewFileNode->NextValueOffset = 0;
    NewFileNode->NameLength      = NameLen;
    NewFileNode->DataPOffset     = DataPOffset;     // Nunjuk ke Inline Data atau Cell Volatile Payload
    NewFileNode->DataLength      = DataSize;
    NewFileNode->Type            = Type;            // REG_SZ, REG_DWORD, REG_BINARY, dll.
    NewFileNode->Flags           = KEY_FILE_VOLATILE;
    RtlCopyMemory(NewFileNode->Name, ValueName->Buffer, NameLen);

    // Link the key
    if (ParentNode->FirstValueOffset == 0)
    {
        // Fast Path
        ParentNode->FirstValueOffset = VolatileValueOffset;
    }
    else
    {
        // Slow Path
        ULONG CurrValOff = ParentNode->FirstValueOffset;
        while (CurrValOff != 0)
        {
            PKEY_FILE_NODE CurrVal = (PKEY_FILE_NODE)VkpGetCellPointer(Hive, CurrValOff);

            if (!CurrVal || CurrVal->Signature != KEY_FILE_SIG)
            {
                // Structural corruption check
                return STATUS_OBJECT_TYPE_MISMATCH;
            }

            if (CurrVal->NextValueOffset == 0)
            {
                CurrVal->NextValueOffset = VolatileValueOffset;
                break;
            }

            CurrValOff = CurrVal->NextValueOffset;
        }
    }

    // Parent is dirty and need to take a bath
    VkpMarkCellDirty(Hive, ParentOffset);

    *CreatedValueOffset = VolatileValueOffset;
    return STATUS_SUCCESS;
}

PVOID
VEAPI
VkpAllocateStableCell(
    IN PVK_HIVE Hive,
    IN ULONG Size,
    OUT PULONG OutStableOffset
)
{
    if (!Hive || Size == 0 || !OutStableOffset)
    {
        return NULL;
    }

    // Align it so CPU happy :)
    ULONG AlignedSize = VK_ALIGN_UP(Size, 4);

    // Condition:
    // 1. Hive Free Offset is zero
    // 2. Hive Free Offset + this AlignedSize more than Hive Length
    if (Hive->FreeOffset == 0 || (Hive->FreeOffset + AlignedSize) > Hive->Length)
    {
        HvAllocateStableCell(Hive);
    }

    // Lets update our free offset. Take data allocatedoffset
    // and allocatedcell
    ULONG AllocatedOffset = Hive->FreeOffset;
    PVOID AllocatedCell = VkpGetCellPointer(Hive, AllocatedOffset);
    if (!AllocatedCell)
        return NULL;

    Hive->FreeOffset += AlignedSize;
    VkpMarkCellDirty(Hive, AllocatedOffset);

    // Return our new AllocateCell
    *OutStableOffset = AllocatedOffset; 
    return AllocatedCell;
}

VEASTATUS
VEAPI
VkpAttachStableSubkey(
    IN PVK_HIVE Hive,
    IN ULONG ParentOffset,
    IN PANSI_STRING SubkeyName,
    OUT PULONG CreatedSubkeyOffset
)
{
    // Condition
    // 1. No Hive
    // 2. No ValueName or ValueName invalid
    // 3. No CreateValueOffset
    if (!Hive || !SubkeyName || !SubkeyName->Buffer || SubkeyName->Length == 0 || !CreatedSubkeyOffset)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Get parent node and cek duplication, return if there is a duplication
    PKEY_DIRECTORY_NODE ParentNode = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, ParentOffset);
    if (!ParentNode || ParentNode->Signature != KEY_DIR_SIG)
    {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    ULONG ExistingOffset = 0;
    PKEY_DIRECTORY_NODE ExistingNode = VkpFindSubkey(
        Hive, 
        ParentNode, 
        SubkeyName->Buffer, 
        SubkeyName->Length, 
        &ExistingOffset
    );

    if (ExistingNode != NULL)
    {
        *CreatedSubkeyOffset = ExistingOffset;
        return STATUS_OBJECT_NAME_COLLISION; 
    }

    // Calculate size for allocating stable cell
    USHORT NameLen = SubkeyName->Length;
    ULONG  NodeSize = sizeof(KEY_DIRECTORY_NODE) + NameLen;

    ULONG StableCellOffset = 0;
    PKEY_DIRECTORY_NODE NewStableNode = (PKEY_DIRECTORY_NODE)VkpAllocateStableCell(
        Hive, 
        NodeSize, 
        &StableCellOffset
    );

    if (!NewStableNode)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    ParentNode = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, ParentOffset);
    if (!ParentNode || ParentNode->Signature != KEY_DIR_SIG)
    {
        return STATUS_OBJECT_TYPE_MISMATCH; // seharusnya gak mungkin, tapi jaga-jaga
    }

    // Fill our metadata
    RtlZeroMemory(NewStableNode, NodeSize);
    NewStableNode->CellSize           = -(LONG)NodeSize; // Negative = Used
    NewStableNode->Signature          = KEY_DIR_SIG;     // 'DN'
    NewStableNode->Flags              = 0;               // 0 = Stable Key
    NewStableNode->ParentOffset       = ParentOffset;  
    NewStableNode->FirstSubKeyOffset  = 0;
    NewStableNode->NextSiblingOffset  = 0;
    NewStableNode->FirstValueOffset   = 0;
    NewStableNode->SubKeyCount        = 0;
    NewStableNode->NameLength         = NameLen;
    RtlCopyMemory(NewStableNode->Name, SubkeyName->Buffer, NameLen);

    // Link our Subkey to annother Subkey
    if (ParentNode->FirstSubKeyOffset == 0)
    {
        // Fast path
        ParentNode->FirstSubKeyOffset = StableCellOffset;
    }
    else
    {
        // Slow path
        ULONG CurrSiblingOff = ParentNode->FirstSubKeyOffset;

        while (CurrSiblingOff != 0)
        {
            PKEY_DIRECTORY_NODE CurrSibling = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, CurrSiblingOff);

            if (!CurrSibling || CurrSibling->Signature != KEY_DIR_SIG)
            {
                return STATUS_OBJECT_TYPE_MISMATCH;
            }

            if (CurrSibling->NextSiblingOffset == 0)
            {
                CurrSibling->NextSiblingOffset = StableCellOffset;
                
                // Tandai Sibling Terakhir ini Dirty karena NextSiblingOffset-nya diubah!
                VkpMarkCellDirty(Hive, CurrSiblingOff);
                break;
            }

            CurrSiblingOff = CurrSibling->NextSiblingOffset;
        }
    }

    // Update SubKey Count
    ParentNode->SubKeyCount++;
    VkpMarkCellDirty(Hive, ParentOffset);

    *CreatedSubkeyOffset = StableCellOffset;
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
VkpAttachStableValueKey(
    IN PVK_HIVE Hive,
    IN ULONG ParentOffset,
    IN PANSI_STRING ValueName,
    IN ULONG Type,
    IN PVOID Data,
    IN ULONG DataSize,
    OUT PULONG CreatedValueOffset
)
{
    if (!Hive || !ValueName || !ValueName->Buffer || ValueName->Length == 0 || !CreatedValueOffset)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // 1. Get Parent Node
    PKEY_DIRECTORY_NODE ParentNode = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, ParentOffset);
    if (!ParentNode || ParentNode->Signature != KEY_DIR_SIG)
    {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    // 2. CEK DUPLIKASI VALUE
    PKEY_FILE_NODE ExistingValue = VkpFindValueNode(Hive, ParentNode, ValueName, NULL);
    if (ExistingValue != NULL)
    {
        return STATUS_OBJECT_NAME_COLLISION;
    }

    // 3. ATUR PAYLOAD DATA (INLINE VS OUT-OF-LINE STABLE CELL)
    ULONG DataPOffset = 0;

    if (DataSize <= sizeof(ULONG))
    {
        // Inline Data: Copy langsung ke field DataPOffset
        if (Data != NULL && DataSize > 0)
        {
            RtlCopyMemory(&DataPOffset, Data, DataSize);
        }
    }
    else
    {
        // Out-of-line Data: Alokasi Cell Stable khusus untuk Payload Data
        if (Data == NULL)
        {
            return STATUS_INVALID_PARAMETER;
        }

        ULONG CellAllocSize = DataSize + sizeof(LONG);
        PVOID DataCell = VkpAllocateStableCell(Hive, CellAllocSize, &DataPOffset);
        if (!DataCell)
        {
            return STATUS_INSUFFICIENT_MEMORY;
        }

        *(PLONG)DataCell = -(LONG)CellAllocSize;
        RtlCopyMemory((PUCHAR)DataCell + sizeof(LONG), Data, DataSize);
    }

    // 4. ALOKASI STABLE CELL UNTUK KEY_FILE_NODE ('FN')
    USHORT NameLen = ValueName->Length;
    ULONG  NodeSize = sizeof(KEY_FILE_NODE) + NameLen;

    ULONG StableValueOffset = 0;
    PKEY_FILE_NODE NewFileNode = (PKEY_FILE_NODE)VkpAllocateStableCell(
        Hive, 
        NodeSize, 
        &StableValueOffset
    );

    if (!NewFileNode)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    // Refresh pointer
    ParentNode = (PKEY_DIRECTORY_NODE)VkpGetCellPointer(Hive, ParentOffset);
    if (!ParentNode || ParentNode->Signature != KEY_DIR_SIG)
    {
        return STATUS_OBJECT_TYPE_MISMATCH; // seharusnya gak mungkin, tapi jaga-jaga
    }

    // 5. ISI METADATA KEY_FILE_NODE
    RtlZeroMemory(NewFileNode, NodeSize);
    NewFileNode->CellSize        = -(LONG)NodeSize; // Negatif = Used Cell
    NewFileNode->Signature       = KEY_FILE_SIG;    // 'FN'
    NewFileNode->NextValueOffset = 0;
    NewFileNode->NameLength      = NameLen;
    NewFileNode->DataPOffset     = DataPOffset;     // Raw Stable Offset atau Inline Bytes
    NewFileNode->DataLength      = DataSize;
    NewFileNode->Type            = Type;
    NewFileNode->Flags           = 0;               // 0 = Stable File Node
    RtlCopyMemory(NewFileNode->Name, ValueName->Buffer, NameLen);

    // 6. TAUTKAN KE LINKED LIST PARENT
    if (ParentNode->FirstValueOffset == 0)
    {
        // Fast Path
        ParentNode->FirstValueOffset = StableValueOffset;
    }
    else
    {
        // Slow Path: Iterasi sampai Value Sibling paling ujung
        ULONG CurrValOff = ParentNode->FirstValueOffset;

        while (CurrValOff != 0)
        {
            PKEY_FILE_NODE CurrVal = (PKEY_FILE_NODE)VkpGetCellPointer(Hive, CurrValOff);

            if (!CurrVal || CurrVal->Signature != KEY_FILE_SIG)
            {
                return STATUS_OBJECT_TYPE_MISMATCH;
            }

            if (CurrVal->NextValueOffset == 0)
            {
                CurrVal->NextValueOffset = StableValueOffset;

                // Mark Sibling Lama Dirty
                VkpMarkCellDirty(Hive, CurrValOff);
                break;
            }

            CurrValOff = CurrVal->NextValueOffset;
        }
    }

    // Mark Parent Dirty
    VkpMarkCellDirty(Hive, ParentOffset);

    *CreatedValueOffset = StableValueOffset;
    return STATUS_SUCCESS;
}

