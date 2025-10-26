#include <stdio.h>

#include "common/print.h"

int vprintf(const char* _RESTRICT format, va_list args)
{
    return vfprintf(stdout, format, args);
}
