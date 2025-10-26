#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "user/common/syscalls.h"

fd_t vopenf(const char* _RESTRICT format, va_list args)
{
    char path[MAX_PATH];
    vsnprintf(path, MAX_PATH, format, args);
    return open(path);
}
