#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/io.h>

uint64_t vreadf(fd_t fd, const char* _RESTRICT format, va_list args)
{
    char buffer[0x1000];
    uint64_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
    if (bytesRead == ERR)
    {
        return ERR;
    }
    buffer[bytesRead] = '\0';

    int result = vsscanf(buffer, format, args);
    if (result < 0)
    {
        errno = EINVAL;
        return ERR;
    }
    return result;
}
