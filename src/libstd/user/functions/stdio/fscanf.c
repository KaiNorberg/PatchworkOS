#include <stdarg.h>
#include <stdio.h>

int fscanf(FILE* _RESTRICT stream, const char* _RESTRICT format, ...)
{
    int rc;
    va_list ap;
    va_start(ap, format);
    rc = vfscanf(stream, format, ap);
    va_end(ap);
    return rc;
}
