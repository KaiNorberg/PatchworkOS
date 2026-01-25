#include <sys/fs.h>

char* readfiles(const char* path)
{
    fd_t fd = open(path);
    if (fd == _FAIL)
    {
        return NULL;
    }
    char* str = reads(fd);
    close(fd);
    return str;
}