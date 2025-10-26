#include <sys/io.h>

uint64_t readfilef(const char* path, const char* _RESTRICT format, ...)
{
    va_list args;
    va_start(args, format);
    uint64_t result = vreadfilef(path, format, args);
    va_end(args);
    return result;
}
