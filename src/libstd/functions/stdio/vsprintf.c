#include <stdio.h>

#include "common/print.h"
#include "platform/platform.h"

int vsprintf(char* _RESTRICT s, const char* _RESTRICT format, va_list arg)
{
    return vsnprintf(s, SIZE_MAX, format, arg);
}
