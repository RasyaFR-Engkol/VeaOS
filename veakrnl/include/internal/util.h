#pragma once

#include <procbind.h>
VOID ksi_switch_stack(ULONG_PTR NewStackTop, PVOID NextFunction, PVOID Argument);