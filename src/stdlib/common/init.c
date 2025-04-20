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
        fd_t fd = open("sys:/null");
        if (fd != STDOUT_FILENO)
        {
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
    }
    if (read(STDIN_FILENO, NULL, 0) == ERR)
    {
        fd_t fd = open("sys:/null");
        if (fd != STDIN_FILENO)
        {
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
    }
    errno = 0;
#endif

    _HeapInit();
    _TimeZoneInit();

    _PlatformInit();
}
