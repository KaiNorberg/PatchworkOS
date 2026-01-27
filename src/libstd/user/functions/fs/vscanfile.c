#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

uint64_t vscanfile(const char* path, const char* format, va_list args)
{
    fd_t fd;
    status_t status = open(&fd, path);
    if (IS_ERR(status))
    {
        return 0;
    }
    uint64_t result = vscan(fd, format, args);
    close(fd);
    return result;
}
