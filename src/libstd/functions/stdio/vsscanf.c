#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

#define _SCAN_GET(ctx) \
    ({ \
        const char** str = (const char**)(ctx)->private; \
        char c = **str; \
        if (c != '\0') \
        { \
            (*str)++; \
        } \
        c == '\0' ? EOF : (unsigned char)c; \
    })

#define _SCAN_UNGET(ctx, c) \
    ({ \
        const char** str = (const char**)(ctx)->private; \
        if ((c) != EOF) \
        { \
            (*str)--; \
        } \
    })

#include "common/scan.h"

int vsscanf(const char* _RESTRICT s, const char* _RESTRICT format, va_list arg)
{
    return _scan(format, arg, (void*)&s);
}