#pragma once

/* kernel essential */
#include <procbind.h> // IWYU pragma: export
#include <ldrtypes.h> // IWYU pragma: export
#include <ul.h> // IWYU pragma: export
#include <kdp.h> // IWYU pragma: export
#include <mmtype.h>
#include <mm.h>
#include <rtl.h>
#include <ks.h>
#include <hct.h>
#include <obtype.h>
#include <obfunc.h>
#include <veastatus.h>
#include <tag.h>
#include <sftypes.h>
#include <sffunc.h>
#include <pttypes.h>
#include <ptfunc.h>
#include <vktypes.h>
#include <vkfunc.h>

/* hardware essential */
#include <../asm/intrin.h> // IWYU pragma: export

/* internal */
#include "internal/veakrnl.h" // IWYU pragma: export
#include "internal/x86.h"
#include "internal/hcttype.h"
#include "internal/hctfunc.h"
