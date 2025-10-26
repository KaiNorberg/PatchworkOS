#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

int vsprintf(char* _RESTRICT s, const char* _RESTRICT format, va_list arg)
{
    return vsnprintf(s, SIZE_MAX, format, arg);
}
