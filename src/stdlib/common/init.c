#include "stdlib_internal/init.h"

#include "../platform/platform.h"
#include "heap.h"
#include "thread.h"
#include "time_zone.h"

void _StdInit(void)
{
    _HeapInit();
#if _PLATFORM_HAS_SCHEDULING
    _ThreadingInit();
#endif
    _TimeZoneInit();
    _PlatformInit();
}
