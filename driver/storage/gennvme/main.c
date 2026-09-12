#include <veaddk.h>

VOID DriverEntry(VOID)
{
    ObioCreateDriver("WOW", NULL, NULL);
}