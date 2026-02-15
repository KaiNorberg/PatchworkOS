#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>
#include <sys/io.h>

#define _SCAN_GET(ctx) \
    ({ \
        fd_t fd = (fd_t)(ctx)->data; \
        int res = EOF; \
        char c; \
        size_t count; \
        status_t status = ioread(fd, &c, 1, IOCUR, &count); \
        if (IS_INFO(status) && count == 1) \
        { \
            res = c; \
        } \
        res; \
    })

#define _SCAN_UNGET(ctx, c) \
    ({ \
        fd_t fd = (fd_t)(ctx)->data; \
        if ((c) != EOF) \
        { \
            ioseek(fd, IOSEEK_CUR, -1, NULL); \
        } \
    })

#include "common/scan.h"

uint64_t vscan(fd_t fd, const char* format, va_list args)
{
    int result = _scan(format, args, (void*)(uintptr_t)fd);
    return result < 0 ? 0 : (uint64_t)result;
}
