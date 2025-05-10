#include <stdio.h>
#include <stdlib.h>

#include "common/print.h"
#include "platform/platform.h"

char* vasprintf(const char* _RESTRICT format, va_list args)
{
    va_list argsCopy;
    va_copy(argsCopy, args);

    int size = vsnprintf(NULL, 0, format, argsCopy);
    va_end(argsCopy);

    if (size < 0)
    {
        return NULL;
    }

    char* buffer = (char*)malloc(size + 1);
    if (!buffer)
    {
        return NULL;
    }

    vsprintf(buffer, format, args);

    return buffer;
}
