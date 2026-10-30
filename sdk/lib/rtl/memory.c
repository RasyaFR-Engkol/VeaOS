#include <rtl.h>

VOID RtlFillMemory(
    PVOID  Destination,
    SIZE_T Length,
    UCHAR  Fill
) {
    if (Destination == NULL || Length == 0) {
        return;
    }

    UCHAR* BytePtr = (UCHAR*)Destination;
    for (SIZE_T i = 0; i < Length; i++) {
        BytePtr[i] = Fill;
    }
}

VOID
VEAPI
RtlFillMemoryUlong(
    PVOID  Destination,
    SIZE_T Length,
    ULONG  Pattern
)
{
    // Pastikan pointer valid dan Length minimal seukuran 1 ULONG (4 bytes)
    if (Destination == NULL || Length < sizeof(ULONG)) {
        return;
    }

    PULONG UlongPtr = (PULONG)Destination;
    SIZE_T Count = Length / sizeof(ULONG); // Hitung berapa banyak ULONG yang ditulis

    for (SIZE_T i = 0; i < Count; i++) {
        UlongPtr[i] = Pattern;
    }
}

VOID RtlZeroMemory(
    PVOID  Destination,
    SIZE_T Length
) {
    // Di Windows, RtlZeroMemory secara internal memanggil RtlFillMemory dengan nilai 0
    RtlFillMemory(Destination, Length, 0);
}

VOID RtlCopyMemory(PVOID Dest, PVOID Src, SIZE_T N)
{
    PCHAR pDest = (PCHAR)Dest;
    PCHAR pSrc = (PCHAR)Src;
    while(N != 0)
    {
        *pDest = *pSrc;
        pDest++; pSrc++;
        N--;
    };
}

SIZE_T RtlCompareMemory(
    const PVOID Source1,
    const PVOID Source2,
    SIZE_T Length
)
{
    const PUCHAR P1 = (const PUCHAR)Source1;
    const PUCHAR P2 = (const PUCHAR)Source2;
    SIZE_T Matched = 0;

    while (Matched < Length && P1[Matched] == P2[Matched])
    {
        Matched++;
    }

    return Matched;
}