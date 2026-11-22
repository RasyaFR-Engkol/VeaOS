#include "veapi.h"

#define SVC_ENTRY(Func, ArgCount) (ULONG_PTR)(Func),
ULONG_PTR VeapiSSDT[] = {
    SVC_ENTRY(VeaUserInitialize, 0)         /* Syscall ID: 0x1000 */
    SVC_ENTRY(VeaUserGetMessage, 4)         /* Syscall ID: 0x1001 */
    SVC_ENTRY(VeaUserGetAsyncKeyState, 1)   /* Syscall ID: 0x1002 */
};
#undef SVC_ENTRY

#define SVC_ENTRY(Func, ArgCount) ((ArgCount) * sizeof(ULONG_PTR)),
UCHAR VeapiSSPT[] = {
    SVC_ENTRY(VeaUserInitialize, 0)
    SVC_ENTRY(VeaUserGetMessage, 4)
    SVC_ENTRY(VeaUserGetAsyncKeyState, 1)
};
#undef SVC_ENTRY

ULONG VeapiNumberOfSysCalls = sizeof(VeapiSSDT) / sizeof(VeapiSSDT[0]);