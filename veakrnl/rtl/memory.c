#include <veakrnl.h>

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