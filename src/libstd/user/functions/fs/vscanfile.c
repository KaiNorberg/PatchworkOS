#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

uint64_t vscanfile(const char* path, const char* format, va_list args)
{
    fd_t fd = open(path);
    if (fd == _FAIL)
    {
        return _FAIL;
    }
    uint64_t result = vscan(fd, format, args);
    close(fd);
    return result;
}
