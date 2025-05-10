#include "libstd_internal/init.h"

#include "../platform/platform.h"
#include "constraint_handler.h"
#include "heap.h"
#include "time_utils.h"

#include <errno.h>
#include <sys/io.h>

void _StdInit(void)
{
    _PlatformEarlyInit();

    _ConstraintHandlerInit();
    _HeapInit();
    _TimeZoneInit();

    _PlatformLateInit();
}
