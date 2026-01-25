#include <stdio.h>
#include <sys/fs.h>

uint64_t sharefile(char* key, uint64_t size, const char* path, clock_t timeout)
{
    fd_t fd = open(path);
    if (fd == _FAIL)
    {
        return _FAIL;
    }

    uint64_t result = share(key, size, fd, timeout);
    close(fd);
    return result;
}