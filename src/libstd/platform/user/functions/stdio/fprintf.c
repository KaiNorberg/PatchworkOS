#include <stdio.h>

#include "common/print.h"
#include "platform/platform.h"

int fprintf(FILE* _RESTRICT stream, const char* _RESTRICT format, ...)
{
    int rc;
    va_list ap;
    va_start(ap, format);
    rc = vfprintf(stream, format, ap);
    va_end(ap);
    return rc;
}
