#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

uint64_t scan(fd_t fd, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    uint64_t result = vscan(fd, format, args);
    va_end(args);
    return result;
}
