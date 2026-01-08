#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#define _SCAN_GET(ctx) \
    ({ \
        fd_t fd = (fd_t)(ctx)->private; \
        int res = EOF; \
        char c; \
        if (read(fd, &c, 1) == 1) \
        { \
            res = c; \
        } \
        res; \
    })

#define _SCAN_UNGET(ctx, c) \
    ({ \
        fd_t fd = (fd_t)(ctx)->private; \
        if ((c) != EOF) \
        { \
            seek(fd, -1, SEEK_CUR); \
        } \
    })

#include "common/scan.h"

uint64_t vscan(fd_t fd, const char* format, va_list args)
{
    int result = _scan(format, args, (void*)(uintptr_t)fd);
    return result < 0 ? ERR : (uint64_t)result;
}
