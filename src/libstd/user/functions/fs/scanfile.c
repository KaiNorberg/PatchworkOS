#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

uint64_t scanfile(const char* path, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    uint64_t result = vscanfile(path, format, args);
    va_end(args);
    return result;
}
