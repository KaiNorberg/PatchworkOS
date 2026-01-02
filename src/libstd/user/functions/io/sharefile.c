#include <stdio.h>
#include <sys/io.h>

uint64_t sharefile(char* key, uint64_t size, const char* path, clock_t timeout)
{
    fd_t fd = open(path);
    if (fd == ERR)
    {
        return ERR;
    }

    uint64_t result = share(key, size, fd, timeout);
    close(fd);
    return result;
}