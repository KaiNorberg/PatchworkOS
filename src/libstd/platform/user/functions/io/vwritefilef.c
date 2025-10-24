#include <sys/io.h>

uint64_t vwritefilef(const char* path, const char* _RESTRICT format, va_list args)
{
    fd_t fd = open(path);
    if (fd == ERR)
    {
        return ERR;
    }

    uint64_t result = vwritef(fd, format, args);
    close(fd);
    return result;
}
