#include <stdarg.h>
#include <stdio.h>

int scanf(const char* _RESTRICT format, ...)
{
    int rc;
    va_list ap;
    va_start(ap, format);
    rc = vfscanf(stdin, format, ap);
    va_end(ap);
    return rc;
}