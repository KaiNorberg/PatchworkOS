#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/io.h>

uint64_t vwritef(fd_t fd, const char* _RESTRICT format, va_list args)
{
    char buffer[0x1000];
    int count = vsnprintf(buffer, sizeof(buffer), format, args);
    if (count < 0 || (uint64_t)count >= sizeof(buffer))
    {
        errno = EINVAL;
        return ERR;
    }
    return write(fd, buffer, count);
}
