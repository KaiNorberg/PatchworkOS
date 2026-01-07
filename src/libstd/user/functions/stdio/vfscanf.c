#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

#include "user/common/file.h"

#define _SCAN_GET(ctx) \
    ({ \
        FILE* file = (FILE*)(ctx)->private; \
        int c = fgetc(file); \
        if (c != EOF) \
        { \
            (ctx)->count++; \
        } \
        c; \
    })

#define _SCAN_UNGET(ctx, c) \
    ({ \
        FILE* file = (FILE*)(ctx)->private; \
        if ((c) != EOF) \
        { \
            ungetc(c, file); \
            (ctx)->count--; \
        } \
    })

#include "common/scan.h"

int vfscanf(FILE* _RESTRICT stream, const char* _RESTRICT format, va_list arg)
{
    return _scan(format, arg, (void*)stream);
}
