#include <stdio.h>

#include "common/print.h"
#include "platform/platform.h"

int printf(const char* _RESTRICT format, ...)
{
    va_list args;
    va_start(args, format);
    int result = _PlatformVprintf(format, args);
    va_end(args);
    return result;
}
