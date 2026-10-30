#pragma once

#include <procbind.h>
#include "rtl.h"

static FORCEINLINE
BOOLEAN
IsEqualGUID(
    IN const GUID *Guid1,
    IN const GUID *Guid2
    )
{
    // Kalau VeaOS punya RtlCompareMemory:
    return (RtlCompareMemory((PVOID)Guid1, (PVOID)Guid2, sizeof(GUID)) == sizeof(GUID));
    
    // Atau kalau lu mau pakai bitwise OR biar lebih cepet (opsional):
    // return (((PULONG)Guid1)[0] == ((PULONG)Guid2)[0] &&
    //         ((PULONG)Guid1)[1] == ((PULONG)Guid2)[1] &&
    //         ((PULONG)Guid1)[2] == ((PULONG)Guid2)[2] &&
    //         ((PULONG)Guid1)[3] == ((PULONG)Guid2)[3]);
}