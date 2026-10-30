// VEANALYZE 

#pragma once
#include <procbind.h>

// Support CLANG only
#if defined(__clang__)

/* Lock Annotations */
#define __Analyze_Lock_Acquire(Lock)    __attribute__((annotate("vea_lock_acquire:" #Lock)))
#define __Analyze_Lock_Release(Lock)    __attribute__((annotate("vea_lock_release:" #Lock)))
#define __Analyze_Lock_Held(Lock)       __attribute__((annotate("vea_lock_held:" #Lock)))

/* IRQL Annotations */
#define __Analyze_IRQL_Requires(KIRQL)  __attribute__((annotate("vea_irql_req:" #KIRQL)))
#define __Analyze_IRQL_Maximum(KIRQL)   __attribute__((annotate("vea_irql_max:" #KIRQL)))
#define __Analyze_IRQL_At_Return(KIRQL) __attribute__((annotate("vea_irql_ret:" #KIRQL)))

/* IRQL Transition Annotations */
#define __Analyze_Irql_Raise(NewIrql)   __attribute__((annotate("vea_irql_raise:" #NewIrql)))
#define __Analyze_Irql_Restore(OldIrql) __attribute__((annotate("vea_irql_restore:" #OldIrql)))

/* Memory Safety Annotations */
#define __Analyze_Paged_Code()          __attribute__((annotate("vea_paged_code")))

#endif