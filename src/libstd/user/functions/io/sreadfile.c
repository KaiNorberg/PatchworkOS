#include <sys/io.h>

char* readfiles(const char* path)
{
    fd_t fd = open(path);
    if (fd == ERR)
    {
        return NULL;
    }
    char* str = reads(fd);
    close(fd);
    return str;
}