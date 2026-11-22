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

BOOLEAN RtlEqualMemory(PVOID Source1, PVOID Source2, SIZE_T Length) {
    const unsigned char *p1 = (const unsigned char *)Source1;
    const unsigned char *p2 = (const unsigned char *)Source2;

    for (SIZE_T i = 0; i < Length; i++) {
        if (p1[i] != p2[i]) {
            return FALSE;
        }
    }

    return TRUE;
}

VOID
RtlMoveMemory(
    VOID *Destination,
    const VOID *Source,
    SIZE_T Length)
{
    unsigned char *pdst = (unsigned char *)Destination;
    const unsigned char *psrc = (const unsigned char *)Source;

    if (pdst == psrc || Length == 0) {
        return;
    }

    /* Tujuan SEBELUM sumber (atau tidak overlap): aman copy maju */
    if (pdst < psrc || pdst >= psrc + Length) {

        /* Copy per-ULONG jika kedua pointer selaras 4 byte */
        if ((((ULONG_PTR)pdst | (ULONG_PTR)psrc) & (sizeof(ULONG) - 1)) == 0) {
            ULONG *dw_dst = (ULONG *)pdst;
            const ULONG *dw_src = (const ULONG *)psrc;
            SIZE_T dwords = Length / sizeof(ULONG);

            while (dwords--) {
                *dw_dst++ = *dw_src++;
            }
            pdst = (unsigned char *)dw_dst;
            psrc = (const unsigned char *)dw_src;
            Length &= (sizeof(ULONG) - 1);
        }

        while (Length--) {
            *pdst++ = *psrc++;
        }
    }
    /* Tujuan SETELAH sumber dan overlap: wajib copy MUNDUR */
    else {
        pdst += Length;
        psrc += Length;

        if ((((ULONG_PTR)pdst | (ULONG_PTR)psrc) & (sizeof(ULONG) - 1)) == 0) {
            ULONG *dw_dst = (ULONG *)pdst;
            const ULONG *dw_src = (const ULONG *)psrc;
            SIZE_T dwords = Length / sizeof(ULONG);

            while (dwords--) {
                *--dw_dst = *--dw_src;
            }
            pdst = (unsigned char *)dw_dst;
            psrc = (const unsigned char *)dw_src;
            Length &= (sizeof(ULONG) - 1);
        }

        while (Length--) {
            *--pdst = *--psrc;
        }
    }
}

/* EOF */