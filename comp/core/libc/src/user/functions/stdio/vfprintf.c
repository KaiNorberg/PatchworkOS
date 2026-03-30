#include <stdio.h>
#include <stdlib.h>

#include "user/common/file.h"

#define _PRINT_WRITE(ctx, buffer, count) \
    ({ \
        FILE* file = (FILE*)(ctx)->data; \
        int ret = 0; \
        if (fwrite(buffer, 1, count, file) != (size_t)(count)) \
        { \
            ret = EOF; \
        } \
        ret; \
    })

#define _PRINT_FILL(ctx, c, count) \
    ({ \
        FILE* file = (FILE*)(ctx)->data; \
        int ret = 0; \
        for (size_t i = 0; i < (size_t)(count); i++) \
        { \
            if (fputc((c), file) == EOF) \
            { \
                ret = EOF; \
                break; \
            } \
        } \
        ret; \
    })

#include "common/print.h"

int vfprintf(FILE* _RESTRICT stream, const char* _RESTRICT format, va_list arg)
{
    return _print(format, SIZE_MAX, arg, stream);
}
