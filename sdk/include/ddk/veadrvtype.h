#ifndef _VEADRVTYPE_H_
#define _VEADRVTYPE_H_

#include <procbind.h>
#include <ndk/obtype.h>

typedef UCHAR KIRQL, *PKIRQL;
typedef ULONG_PTR KSPIN_LOCK, *PKSPIN_LOCK;

typedef enum _POOL_TYPE {
    NonPagedPool,
    PagedPool,
    MaxPoolType
} POOL_TYPE;


#endif // _VEADRVTYPE_H_
