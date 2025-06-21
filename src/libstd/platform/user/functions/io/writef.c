#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t writef(fd_t fd, const char* _RESTRICT format, ...)
{
    va_list args;
    va_start(args, format);
    uint64_t result = vwritef(fd, format, args);
    va_end(args);
    return result;
}
