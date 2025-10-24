#include <sys/io.h>

uint64_t removef(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    uint64_t result = vremovef(format, args);
    va_end(args);
    return result;
}
