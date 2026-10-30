#pragma once

#include <stdarg.h>
#include "procbind.h"
#include "veastatus.h"
#include "../psdk/veaelf.h"

/* TYPES */

// AVL Specific
typedef struct _RTL_BALANCED_NODE {
    struct _RTL_BALANCED_NODE *Left;
    struct _RTL_BALANCED_NODE *Right;
    struct _RTL_BALANCED_NODE* Parent;
    LONG Balance;
} RTL_BALANCED_NODE, *PRTL_BALANCED_NODE;

typedef LONG VEAPI (*PRTL_AVL_COMPARE_ROUTINE)(
    PRTL_BALANCED_NODE NodeA, 
    PRTL_BALANCED_NODE NodeB
);

typedef struct _RTL_AVL_TREE {
    PRTL_BALANCED_NODE Root;
    PRTL_AVL_COMPARE_ROUTINE CompareRoutine;
    PRTL_BALANCED_NODE Leftmost;
} RTL_AVL_TREE, *PRTL_AVL_TREE;

#define AVL_MAX(a, b)           ((a) > (b) ? (a) : (b))
#define AVL_GET_HEIGHT(node)    ((node) ? (node)->Balance : 0)

/* FUNCTION */

// Memory Specific

VOID RtlFillMemory(
    PVOID  Destination,
    SIZE_T Length,
    UCHAR  Fill
);

VOID RtlZeroMemory(
    PVOID  Destination,
    SIZE_T Length
);

VOID RtlCopyMemory(PVOID Dest, PVOID Src, SIZE_T N);
LONG RtlRawStringLength(const char *string);
PCHAR RtlCopyRawString(char *dst, const char *src);
LONG RtlRawStringCompareN(const char *string, const char *string2, int n);
BOOLEAN
VEAPI
RtlAppendRawString(
    OUT PCHAR Destination,
    IN ULONG DestinationSize,
    IN PCSTR Source
);

#define RTL_CONSTANT_ANSI_STRING(s) \
{ \
    sizeof(s) - sizeof((s)[0]), \
    sizeof(s), \
    (PCHAR)(s) \
}

PCHAR
VEAPI
RtlRawStringString(
    IN PCSTR Haystack,
    IN PCSTR Needle
);

BOOLEAN
VEAPI
RtlGetBootArgumentValue(
    IN PCSTR BootArgs,
    IN PCSTR Key,
    OUT PSTR Buffer,
    IN ULONG BufferSize
);

BOOLEAN
VEAPI
RtlGetBootArgumentULong(
    IN PCSTR BootArgs,
    IN PCSTR Key,
    OUT PULONG Value
);

BOOLEAN
VEAPI
RtlCheckBootFlag(
    IN PCSTR BootArgs,
    IN PCSTR Flag
);

// AVL Specific

VOID 
VEAPI 
RtlInsertElementAvl(PRTL_AVL_TREE Tree, PRTL_BALANCED_NODE NewNode);

VOID 
VEAPI 
RtlDeleteElementAvl(PRTL_AVL_TREE Tree, PRTL_BALANCED_NODE TargetNode);

PRTL_BALANCED_NODE
VEAPI
RtlLookupElementAvl(
    PRTL_AVL_TREE Tree, 
    PRTL_BALANCED_NODE TargetNode
);

PRTL_BALANCED_NODE 
VEAPI 
RtlGetNextNodeAvl(PRTL_BALANCED_NODE Node) ;

// Linked list specific

#define CONTAINING_RECORD(address, type, field) \
    ((type *)( (char *)(address) - offsetof(type, field) ))

#define InitializeListHead(ListHead) \
    ((ListHead)->Flink = (ListHead)->Blink = (ListHead))

#define InsertTailList(ListHead, Entry) do { \
    PLIST_ENTRY _ListHead = (ListHead);      \
    PLIST_ENTRY _Entry = (Entry);            \
    PLIST_ENTRY _OldBlink = _ListHead->Blink;\
    _Entry->Flink = _ListHead;               \
    _Entry->Blink = _OldBlink;               \
    _OldBlink->Flink = _Entry;               \
    _ListHead->Blink = _Entry;               \
} while(0)

#define InsertHeadList(ListHead, Entry) do { \
    PLIST_ENTRY _ListHead = (ListHead);      \
    PLIST_ENTRY _Entry = (Entry);            \
    PLIST_ENTRY _OldFlink = _ListHead->Flink;\
    _Entry->Flink = _OldFlink;               \
    _Entry->Blink = _ListHead;               \
    _OldFlink->Blink = _Entry;               \
    _ListHead->Flink = _Entry;               \
} while(0)

#define IsListEmpty(ListHead) \
    ((ListHead)->Flink == (ListHead))

#define RemoveHeadList(ListHead) (          \
    IsListEmpty(ListHead) ? NULL :          \
    (PLIST_ENTRY)(                          \
        {                                   \
            PLIST_ENTRY _First = (ListHead)->Flink;  \
            PLIST_ENTRY _Next  = _First->Flink;      \
            (ListHead)->Flink  = _Next;              \
            _Next->Blink       = (ListHead);         \
            _First->Flink      = NULL;               \
            _First->Blink      = NULL;               \
            _First;                                  \
        }                                   \
    )                                       \
)

#define RemoveTailList(ListHead) (          \
    IsListEmpty(ListHead) ? NULL :          \
    (PLIST_ENTRY)(                          \
        {                                   \
            PLIST_ENTRY _Last = (ListHead)->Blink;   \
            PLIST_ENTRY _Prev = _Last->Blink;        \
            (ListHead)->Blink = _Prev;               \
            _Prev->Flink      = (ListHead);          \
            _Last->Flink      = NULL;                \
            _Last->Blink      = NULL;                \
            _Last;                                   \
        }                                   \
    )                                       \
)

// Menghapus node mana saja dari list berdasarkan alamat elemen itu sendiri.
// Mengembalikan TRUE jika list menjadi kosong setelah elemen ini dihapus, dan FALSE jika masih ada sisa.
#define RemoveEntryList(Entry) (                     \
    (BOOLEAN)(                                       \
        {                                            \
            PLIST_ENTRY _Entry = (Entry);             \
            PLIST_ENTRY _Flink = _Entry->Flink;      \
            PLIST_ENTRY _Blink = _Entry->Blink;      \
            _Blink->Flink = _Flink;                  \
            _Flink->Blink = _Blink;                  \
            _Entry->Flink = NULL; /* Pengaman */     \
            _Entry->Blink = NULL;                    \
            (_Flink == _Blink);                      \
        }                                            \
    )                                                \
)

// String Utility
#define TAG_RTL_STRING 0x536C7452

ULONG
RtlStringCbPrintfA(
    char *Dest, unsigned int cbDest, const char* Format, ...
);

ULONG
RtlStringCbPrintfAImpl(
    char *Dest, unsigned int cbDest, const char* Format, va_list Args
);

VOID 
VEAPI
RtlInitAnsiString(
    PANSI_STRING DestinationString,
    const PCHAR SourceString
);

VOID
VEAPI
RtlInitUnicodeString(
    PUNICODE_STRING DestinationString,
    const PWCHAR SourceString
);


VOID
VEAPI
RtlCopyUnicodeString(PUNICODE_STRING DestinationString, 
                     const UNICODE_STRING *SourceString);

VOID
VEAPI 
RtlCopyString(PANSI_STRING DestinationString, 
              const ANSI_STRING *SourceString);

VEASTATUS
VEAPI
RtlAppendRawStringToString(
    PANSI_STRING DestinationString,
    PCHAR SourceString
);

VEASTATUS
VEAPI
RtlAppendStringToString(
    PANSI_STRING DestinationString,
    const ANSI_STRING *SourceString
);


BOOLEAN 
VEAPI
RtlEqualUnicodeString(const UNICODE_STRING *String1, 
                      const UNICODE_STRING *String2, 
                      BOOLEAN CaseInSensitive);
           
BOOLEAN 
VEAPI
RtlEqualString(const ANSI_STRING *String1, 
               const ANSI_STRING *String2, 
               BOOLEAN CaseInSensitive);

PCHAR RtlFindSubstringAscii(const ANSI_STRING *String, 
                            const ANSI_STRING *SubString, 
                            BOOLEAN CaseInSensitive);

BOOLEAN
VEAPI
RtlAnsiStringToUnicodeString(
    PUNICODE_STRING DestinationString,
    const ANSI_STRING *SourceString,
    BOOLEAN AllocateDestinationString
);

BOOLEAN
VEAPI
RtlUnicodeStringToAnsiString(
    PANSI_STRING DestinationString,
    const UNICODE_STRING *SourceString,
    BOOLEAN AllocateDestinationString
);

VOID
VEAPI
RtlFreeUnicodeString(PUNICODE_STRING UnicodeString);

VOID
VEAPI 
RtlFreeAnsiString(PANSI_STRING AnsiString);

PCHAR RtlCopyRawStringN(char *dst, const char *src, int n);

// RBT SPECIFIC
#define RTL_RB_BLACK 0
#define RTL_RB_RED   1

#define RTL_RB_COLOR(Node)           ((Node)->Balance)
#define RTL_RB_SET_COLOR(Node, C)    ((Node)->Balance = (C))

typedef LONG VEAPI (*PRTL_RB_COMPARE_ROUTINE)(
    PRTL_BALANCED_NODE NodeA, 
    PRTL_BALANCED_NODE NodeB
);

typedef struct _RTL_RB_TREE {
    PRTL_BALANCED_NODE Root;
    PRTL_RB_COMPARE_ROUTINE CompareRoutine;
    PRTL_BALANCED_NODE Leftmost; // Penting untuk CFS: O(1) akses vruntime terkecil
} RTL_RB_TREE, *PRTL_RB_TREE;

VOID 
VEAPI 
RtlInsertElementRb(PRTL_RB_TREE Tree, PRTL_BALANCED_NODE NewNode);

VOID 
VEAPI 
RtlDeleteElementRb(PRTL_RB_TREE Tree, PRTL_BALANCED_NODE TargetNode);

PRTL_BALANCED_NODE 
VEAPI 
RtlGetNextNodeRb(PRTL_BALANCED_NODE Node);

#define RtlGetLeftmostNodeRb(Tree) ((Tree)->Leftmost)

VOID 
VEAPI 
RtlInitializeRbTree(
    PRTL_RB_TREE Tree, 
    PRTL_RB_COMPARE_ROUTINE CompareRoutine
);

PRTL_BALANCED_NODE 
VEAPI 
RtlLookupElementRb(
    PRTL_RB_TREE Tree, 
    PRTL_BALANCED_NODE SearchNode
);

PRTL_BALANCED_NODE 
VEAPI 
RtlGetNextNodeRb(PRTL_BALANCED_NODE Node);

LONG
VEAPI
PtCfsCompareRoutine(
    PRTL_BALANCED_NODE NodeA, 
    PRTL_BALANCED_NODE NodeB
);

SIZE_T RtlCompareMemory(
    const PVOID Source1,
    const PVOID Source2,
    SIZE_T Length
);

VEASTATUS
VEAPI
RtlStringCchCopyA(
    OUT PCHAR pszDest,
    IN SIZE_T cchDest,
    IN PCSTR pszSrc
);

VEASTATUS
VEAPI
RtlStringFromGUID(
    IN const GUID *Guid,
    OUT PANSI_STRING GuidString
);

ULONGLONG
VEAPI
RtlDivideUnsigned64(
    IN ULONGLONG Dividend,
    IN ULONGLONG Divisor,
    OUT PULONGLONG Remainder OPTIONAL
);

CHAR
VEAPI
RtlUpperChar(
    CHAR Character
);

#define RTL_DUPLICATE_ANSI_STRING_NULL_TERMINATE 0x1
#define RTL_DUPLICATE_ANSI_STRING_ALLOCATE_NULL_STRING 0x2

VEASTATUS VEAPI RtlDuplicateAnsiString(
    ULONG Flags,
    const ANSI_STRING* SourceString,
    ANSI_STRING* DestinationString
);

LONGLONG
VEAPI
RtlDivideSigned64(
    IN LONGLONG Dividend,
    IN LONGLONG Divisor,
    OUT PLONGLONG Remainder OPTIONAL
);

//
// Bitmap Specific
//

typedef struct _RTL_BITMAP
{
    ULONG SizeOfBitMap;
    PULONG Buffer;
} RTL_BITMAP, *PRTL_BITMAP;

typedef struct _RTL_BITMAP_RUN
{
    ULONG StartingIndex;
    ULONG NumberOfBits;
} RTL_BITMAP_RUN, *PRTL_BITMAP_RUN;

VOID
VEAPI
RtlFillMemoryUlong(
    PVOID  Destination,
    SIZE_T Length,
    ULONG  Pattern
);

typedef ULONG BITMAP_INDEX, *PBITMAP_INDEX;
typedef ULONG BITMAP_BUFFER, *PBITMAP_BUFFER;

VOID
VEAPI
RtlInitializeBitmap(
    OUT PRTL_BITMAP BitmapHeader,
    IN PBITMAP_BUFFER BitmapBuffer,
    IN ULONG SizeOfBitmap
);

VOID
VEAPI
RtlClearAllBits(
    IN PRTL_BITMAP BitMapHeader
);

VOID
VEAPI
RtlSetAllBits(
    IN PRTL_BITMAP BitMapHeader
);

VOID
VEAPI
RtlClearBit(
    IN PRTL_BITMAP BitMapHeader,
    IN BITMAP_INDEX BitNumber
);

VOID
VEAPI
RtlSetBit(
    IN PRTL_BITMAP BitMapHeader,
    IN BITMAP_INDEX BitNumber
);

VOID
VEAPI
RtlClearBits(
    IN PRTL_BITMAP BitMapHeader,
    IN BITMAP_INDEX StartingIndex,
    IN BITMAP_INDEX NumberToClear
);

VOID
VEAPI
RtlSetBits(
    IN PRTL_BITMAP BitMapHeader,
    IN BITMAP_INDEX StartingIndex,
    IN BITMAP_INDEX NumberToSet
);

BOOLEAN
VEAPI
RtlTestBit(
    IN PRTL_BITMAP BitMapHeader,
    IN BITMAP_INDEX BitNumber
);

BOOLEAN
VEAPI
RtlAreBitsClear(
    IN PRTL_BITMAP BitMapHeader,
    IN BITMAP_INDEX StartingIndex,
    IN BITMAP_INDEX Length
);

BOOLEAN
VEAPI
RtlAreBitsSet(
    _In_ PRTL_BITMAP BitMapHeader,
    _In_ BITMAP_INDEX StartingIndex,
    _In_ BITMAP_INDEX Length
);

BITMAP_INDEX
VEAPI
RtlNumberOfSetBits(
    _In_ PRTL_BITMAP BitMapHeader
);

BITMAP_INDEX
VEAPI
RtlNumberOfClearBits(
    _In_ PRTL_BITMAP BitMapHeader
);

BITMAP_INDEX
VEAPI
RtlFindClearBits(
    _In_ PRTL_BITMAP BitMapHeader,
    _In_ BITMAP_INDEX NumberToFind,
    _In_ BITMAP_INDEX HintIndex
);

BITMAP_INDEX
VEAPI
RtlFindSetBits(
    _In_ PRTL_BITMAP BitMapHeader,
    _In_ BITMAP_INDEX NumberToFind,
    _In_ BITMAP_INDEX HintIndex
);

BITMAP_INDEX
VEAPI
RtlFindClearBitsAndSet(
    _In_ PRTL_BITMAP BitMapHeader,
    _In_ BITMAP_INDEX NumberToFind,
    _In_ BITMAP_INDEX HintIndex
);

BITMAP_INDEX
VEAPI
RtlFindSetBitsAndClear(
    _In_ PRTL_BITMAP BitMapHeader,
    _In_ BITMAP_INDEX NumberToFind,
    _In_ BITMAP_INDEX HintIndex
);

BITMAP_INDEX
VEAPI
RtlFindNextForwardRunClear(
    _In_ PRTL_BITMAP BitMapHeader,
    _In_ BITMAP_INDEX FromIndex,
    _Out_ PBITMAP_INDEX StartingRunIndex
);

BITMAP_INDEX
VEAPI
RtlFindNextForwardRunSet(
    _In_ PRTL_BITMAP BitMapHeader,
    _In_ BITMAP_INDEX FromIndex,
    _Out_ PBITMAP_INDEX StartingRunIndex
);

BITMAP_INDEX
VEAPI
RtlFindFirstRunClear(
    _In_ PRTL_BITMAP BitMapHeader,
    _Out_ PBITMAP_INDEX StartingIndex
);

BITMAP_INDEX
VEAPI
RtlFindLastBackwardRunClear(
    _In_ PRTL_BITMAP BitMapHeader,
    _In_ BITMAP_INDEX FromIndex,
    _Out_ PBITMAP_INDEX StartingRunIndex
);

ULONG
VEAPI
RtlFindClearRuns(
    _In_ PRTL_BITMAP BitMapHeader,
    _In_ PRTL_BITMAP_RUN RunArray,
    _In_ ULONG SizeOfRunArray,
    _In_ BOOLEAN LocateLongestRuns
);

BITMAP_INDEX
VEAPI
RtlFindLongestRunClear(
    IN PRTL_BITMAP BitMapHeader,
    IN PBITMAP_INDEX StartingIndex
);

BITMAP_INDEX
VEAPI
RtlFindLongestRunSet(
    IN PRTL_BITMAP BitMapHeader,
    IN PBITMAP_INDEX StartingIndex
);

typedef struct _VEA_EXCEPTION_FRAME {
    struct _VEA_EXCEPTION_FRAME* PreviousFrame;
    PVOID JumpBuffer[5];
    VEASTATUS FaultStatus;
} VEA_EXCEPTION_FRAME, *PVEA_EXCEPTION_FRAME;

//
// RTL Splay and Balanced Links structures
//
typedef struct _RTL_SPLAY_LINKS
{
    struct _RTL_SPLAY_LINKS *Parent;
    struct _RTL_SPLAY_LINKS *LeftChild;
    struct _RTL_SPLAY_LINKS *RightChild;
} RTL_SPLAY_LINKS, *PRTL_SPLAY_LINKS;

typedef struct _RTL_BALANCED_LINKS
{
    struct _RTL_BALANCED_LINKS *Parent;
    struct _RTL_BALANCED_LINKS *LeftChild;
    struct _RTL_BALANCED_LINKS *RightChild;
    CHAR Balance;
    UCHAR Reserved[3];
} RTL_BALANCED_LINKS, *PRTL_BALANCED_LINKS;

typedef enum _RTL_GENERIC_COMPARE_RESULTS
{
    GenericLessThan,
    GenericGreaterThan,
    GenericEqual
} RTL_GENERIC_COMPARE_RESULTS;

struct _RTL_GENERIC_TABLE;

typedef RTL_GENERIC_COMPARE_RESULTS
(VEAPI RTL_GENERIC_COMPARE_ROUTINE) (
    struct _RTL_GENERIC_TABLE *Table,
    PVOID FirstStruct,
    PVOID SecondStruct
);
typedef RTL_GENERIC_COMPARE_ROUTINE *PRTL_GENERIC_COMPARE_ROUTINE;

typedef PVOID
(VEAPI RTL_GENERIC_ALLOCATE_ROUTINE) (
    struct _RTL_GENERIC_TABLE *Table,
    LONG ByteSize
);
typedef RTL_GENERIC_ALLOCATE_ROUTINE *PRTL_GENERIC_ALLOCATE_ROUTINE;

typedef VOID
(VEAPI RTL_GENERIC_FREE_ROUTINE) (
    struct _RTL_GENERIC_TABLE *Table,
    PVOID Buffer
);
typedef RTL_GENERIC_FREE_ROUTINE *PRTL_GENERIC_FREE_ROUTINE;

typedef struct _RTL_GENERIC_TABLE
{
    PRTL_SPLAY_LINKS TableRoot;
    LIST_ENTRY InsertOrderList;
    PLIST_ENTRY OrderedPointer;
    ULONG WhichOrderedElement;
    ULONG NumberGenericTableElements;
    PRTL_GENERIC_COMPARE_ROUTINE CompareRoutine;
    PRTL_GENERIC_ALLOCATE_ROUTINE AllocateRoutine;
    PRTL_GENERIC_FREE_ROUTINE FreeRoutine;
    PVOID TableContext;
} RTL_GENERIC_TABLE, *PRTL_GENERIC_TABLE;

VOID
VEAPI
RtlInitializeGenericTable(IN PRTL_GENERIC_TABLE Table,
                          IN PRTL_GENERIC_COMPARE_ROUTINE CompareRoutine,
                          IN PRTL_GENERIC_ALLOCATE_ROUTINE AllocateRoutine,
                          IN PRTL_GENERIC_FREE_ROUTINE FreeRoutine,
                          IN PVOID TableContext);

BOOLEAN RtlEqualMemory(PVOID Source1, PVOID Source2, SIZE_T Length);

/* EOF */