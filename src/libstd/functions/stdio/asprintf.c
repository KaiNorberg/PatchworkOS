#include <stdio.h>
#include <stdlib.h>

#include "common/print.h"
#include "platform/platform.h"

char* asprintf(const char* _RESTRICT format, ...)
{
    va_list sizeArgs;
    va_start(sizeArgs, format);
    int size = vsnprintf(NULL, 0, format, sizeArgs);
    va_end(sizeArgs);

    if (size < 0)
    {
        return NULL;
    }

    char* buffer = (char*)malloc(size + 1);
    if (!buffer)
    {
        return NULL;
    }

    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);

    return buffer;
}
