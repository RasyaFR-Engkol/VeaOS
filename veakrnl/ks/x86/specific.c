#include <veakrnl.h>

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
    HctLowerIrql(NewIrql);
}