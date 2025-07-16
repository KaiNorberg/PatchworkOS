#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/io.h>

uint64_t deletef(const char* format, ...)
{
    char path[MAX_PATH];

    va_list args;
    va_start(args, format);
    vsnprintf(path, MAX_PATH, format, args);
    va_end(args);

    return delete (path);
}
