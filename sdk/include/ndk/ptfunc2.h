#pragma once

#include <procbind.h>

FORCEINLINE struct _TEB *NtCurrentTeb(VOID)
{
    return (struct _TEB *)__ReadFS32(0x18);
}

#define NtCurrentPeb() (NtCurrentTeb()->ProcessEnvironmentBlock)