#include <stdio.h>

#include "common/print.h"
#include "platform/platform.h"

int vprintf(const char* _RESTRICT format, va_list args)
{
    return vfprintf(stdout, format, args);
}
