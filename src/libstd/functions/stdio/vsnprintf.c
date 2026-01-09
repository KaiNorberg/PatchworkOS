#include <stdio.h>

#define _PRINT_WRITE(ctx, buffer, count) \
    ({ \
        char* str = (char*)(ctx)->private; \
        size_t i; \
        for (i = 0; i < (size_t)(count); i++) \
        { \
            str[i] = (buffer)[i]; \
        } \
        str += i; \
        (ctx)->private = str; \
        (int)i; \
    })

#define _PRINT_FILL(ctx, c, count) \
    ({ \
        char* str = (char*)(ctx)->private; \
        size_t i; \
        for (i = 0; i < (size_t)(count); i++) \
        { \
            str[i] = (c); \
        } \
        str += i; \
        (ctx)->private = str; \
        (int)i; \
    })

#include "common/print.h"

int vsnprintf(char* _RESTRICT s, size_t n, const char* _RESTRICT format, va_list arg)
{
    int written = _print(format, n, arg, s);
    if (n > 0 && written >= 0)
    {
        s[written] = '\0';
    }
    return written;
}