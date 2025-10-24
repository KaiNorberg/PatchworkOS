#include <sys/io.h>

uint64_t writefilef(const char* path, const char* _RESTRICT format, ...)
{
    va_list args;
    va_start(args, format);
    uint64_t result = vwritefilef(path, format, args);
    va_end(args);
    return result;
}
