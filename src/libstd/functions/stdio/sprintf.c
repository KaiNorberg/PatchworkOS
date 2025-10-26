#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

int sprintf(char* _RESTRICT s, const char* _RESTRICT format, ...)
{
    int rc;
    va_list ap;
    va_start(ap, format);
    rc = vsnprintf(s, SIZE_MAX, format, ap);
    va_end(ap);
    return rc;
}
