#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t vwritef(fd_t fd, const char* _RESTRICT format, va_list args)
{
    char buffer[MAX_PATH];
    uint64_t count = vsnprintf(buffer, MAX_PATH, format, args);
    return write(fd, buffer, count);
}
