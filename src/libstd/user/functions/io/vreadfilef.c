#include <sys/io.h>

uint64_t vreadfilef(const char* path, const char* _RESTRICT format, va_list args)
{
    fd_t fd = open(path);
    if (fd == ERR)
    {
        return ERR;
    }

    uint64_t result = vreadf(fd, format, args);
    close(fd);
    return result;
}
