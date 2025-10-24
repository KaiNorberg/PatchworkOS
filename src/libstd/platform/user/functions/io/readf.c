#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/io.h>

uint64_t readf(fd_t fd, const char* _RESTRICT format, ...)
{
    va_list args;
    va_start(args, format);
    uint64_t result = vreadf(fd, format, args);
    va_end(args);
    return result;
}
