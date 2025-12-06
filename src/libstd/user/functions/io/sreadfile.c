#include <sys/io.h>

char* sreadfile(const char* path)
{
    fd_t fd = open(path);
    if (fd == ERR)
    {
        return NULL;
    }
    char* str = sread(fd);
    close(fd);
    return str;
}