#include <veakrnl.h>

#if defined(KIRQL_DEBUG)

KIRQL
VEAPI
KeRaiseIrqlInternal(
    KIRQL NewIrql,
    PCHAR Function,
    INT Line,
    PCHAR File
)
{
    DPRINT("KeRaiseIrql: %s:%s:%d\n", File, Function, Line);
    return HctRaiseIrql(NewIrql);
}

VOID
VEAPI
KeLowerIrqlInternal(
    KIRQL NewIrql,
    PCHAR Function,
    INT Line,
    PCHAR File
)
{
    DPRINT("KeLowerIrql: %s:%s:%d\n", File, Function, Line);
    return HctLowerIrql(NewIrql);
}

#else

extern KIRQL KiCurrentIrql;
KIRQL
VEAPI
KeRaiseIrql(KIRQL NewIrql)
{

    return HctRaiseIrql(NewIrql);
}

VOID
VEAPI
KeLowerIrql(KIRQL NewIrql)
{
    if (NewIrql > KiCurrentIrql)
    {
        DPRINT("Caller %p try to call KeLowerIrql but NewIrql is big than current irql\n", __GetReturnAddress());
    }
    HctLowerIrql(NewIrql);
}

#endif

VOID
VEAPI
KsMemoryBarrierWithoutFence(VOID)
{
    HctMemoryBarrierWithoutFence();
}