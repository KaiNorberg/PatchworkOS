#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

uint64_t scanfile(const char* path, const char* format, ...)
{
    char* buffer = sreadfile(path);
    if (buffer == NULL)
    {
        return EOF;
    }

    va_list args;
    va_start(args, format);
    int result = vsscanf(buffer, format, args);
    va_end(args);

    free(buffer);
    return result < 0 ? ERR : (uint64_t)result;
}
