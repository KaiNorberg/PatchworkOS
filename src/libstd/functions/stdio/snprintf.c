#include <stdio.h>

#include "common/print.h"
#include "platform/platform.h"

int snprintf( char * _RESTRICT s, size_t n, const char * _RESTRICT format, ... )
{
    int rc;
    va_list ap;
    va_start( ap, format );
    rc = vsnprintf( s, n, format, ap );
    va_end( ap );
    return rc;
}

