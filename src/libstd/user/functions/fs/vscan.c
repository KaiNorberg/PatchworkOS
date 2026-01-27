#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

#define _SCAN_GET(ctx) \
    ({ \
        fd_t fd = (fd_t)(ctx)->data; \
        int res = EOF; \
        char c; \
        size_t count; \
        status_t status = read(fd, &c, 1, &count); \
        if (IS_OK(status) && count == 1) \
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
            seek(fd, -1, SEEK_CUR, NULL); \
        } \
    })

#include "common/scan.h"

uint64_t vscan(fd_t fd, const char* format, va_list args)
{
    int result = _scan(format, args, (void*)(uintptr_t)fd);
    return result < 0 ? 0 : (uint64_t)result;
}
