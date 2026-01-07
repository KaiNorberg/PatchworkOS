#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

#include "user/common/file.h"

#define _SCAN_GET(ctx) \
    ({ \
        FILE* file = (FILE*)(ctx)->private; \
        fgetc(file); \
    })

#include "common/scan.h"

int vfscanf(FILE* _RESTRICT stream, const char* _RESTRICT format, va_list arg)
{
    return _scan(format, arg, (void*)stream);
}
