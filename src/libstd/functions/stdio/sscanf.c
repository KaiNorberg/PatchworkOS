#include <stdarg.h>
#include <stdio.h>

int sscanf(const char* _RESTRICT s, const char* _RESTRICT format, ...)
{
    int rc;
    va_list ap;
    va_start(ap, format);
    rc = vsscanf(s, format, ap);
    va_end(ap);
    return rc;
}