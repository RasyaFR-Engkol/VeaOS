#define SVC_(_Name, _Argcount) (ULONG_PTR)Vea##_Name,
ULONG_PTR MainSSDT[] = {
    #include "sysfuncs.h"
};
#undef SVC_

#define SVC_(name, argcount) argcount * sizeof(void *),
UCHAR MainSSPT[] = {
    #include "sysfuncs.h"
};

#define MIN_SYSCALL_NUMBER    0
#define NUMBER_OF_SYSCALLS    (sizeof(MainSSPT) / sizeof(MainSSPT[0]))
#define MAX_SYSCALL_NUMBER    (NUMBER_OF_SYSCALLS - 1)
ULONG MainNumberOfSysCalls = NUMBER_OF_SYSCALLS;