#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "user/common/syscalls.h"

fd_t openf(const char* _RESTRICT format, ...)
{
    char path[MAX_PATH];

    va_list args;
    va_start(args, format);
    vsnprintf(path, MAX_PATH, format, args);
    va_end(args);

    return open(path);
}
