#include "stdlib_internal/init.h"

#include "../platform/platform.h"
#include "heap.h"
#include "thread.h"
#include "time_zone.h"

#include <errno.h>
#include <sys/io.h>

void _StdInit(void)
{
#if _PLATFORM_HAS_SYSCALLS
    _ThreadingInit();

    if (write(STDOUT_FILENO, NULL, 0) == ERR)
    {
        openas(STDOUT_FILENO, "sys:/null");
    }
    if (read(STDIN_FILENO, NULL, 0) == ERR)
    {
        openas(STDIN_FILENO, "sys:/null");
    }

    errno = 0;
#endif

    _HeapInit();
    _TimeZoneInit();
    _PlatformInit();
}
