// X

#pragma once
#include <procbind.h>

#define KsEnterGuardedRegionThread(_Thread)                                 \
{                                                                           \
                                                                            \
    /* Disable Special APCs */                                              \
    (_Thread)->SpecialApcDisable--;                                         \
}

#define KsEnterGuardedRegion()                                              \
{                                                                           \
    PKTHREAD _Thread = PtGetCurrentThread();                                \
    KsEnterGuardedRegionThread(_Thread);                                    \
}

#define KsLeaveGuardedRegionThread(_Thread)                                 \
{                                                                           \
    /* Sanity checks */                                                     \
                                                                            \
    /* Leave region and check if APCs are OK now */                         \
    if (!(++(_Thread)->SpecialApcDisable))                                  \
    {                                                                       \
        /* Check for Kernel APCs on the list */                             \
        if (!IsListEmpty(&(_Thread)->ApcState.                              \
                         ApcListHead[KernelMode]))                          \
        {                                                                   \
            /* Check for APC Delivery */                                    \
            KsCheckForKernelApcDelivery();                                  \
        }                                                                   \
    }                                                                       \
}

#define KsLeaveGuardedRegion()                                              \
{                                                                           \
    PKTHREAD _Thread = PtGetCurrentThread();                                \
    KsLeaveGuardedRegionThread(_Thread);                                    \
}

#define KsEnterCriticalRegionThread(_Thread)                                \
{                                                                           \
                                                                            \
    /* Disable Kernel APCs */                                               \
    (_Thread)->KernelApcDisable--;                                          \
}

#define KsEnterCriticalRegion()                                             \
{                                                                           \
    PKTHREAD _Thread = PtGetCurrentThread();                                \
    KsEnterCriticalRegionThread(_Thread);                                   \
}

#define KsLeaveCriticalRegionThread(_Thread)                                \
{                                                                           \
                                                                            \
    /* Enable Kernel APCs */                                                \
    (_Thread)->KernelApcDisable++;                                          \
                                                                            \
    /* Check if Kernel APCs are now enabled */                              \
    if (!((_Thread)->KernelApcDisable))                                     \
    {                                                                       \
        /* Check if we need to request an APC Delivery */                   \
        if (!(IsListEmpty(&(_Thread)->ApcState.ApcListHead[KernelMode])) && \
            !((_Thread)->SpecialApcDisable))                                \
        {                                                                   \
            /* Check for the right environment */                           \
            KsCheckForKernelApcDelivery();                                  \
        }                                                                   \
    }                                                                       \
}

#define KsLeaveCriticalRegion()                                             \
{                                                                           \
    PKTHREAD _Thread = PtGetCurrentThread();                                \
    KsLeaveCriticalRegionThread(_Thread);                                   \
}

