#include <libc/io.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

status_t ioscanvp(fd_t cwd, fd_t root, const char* path, size_t count, size_t offset, uint32_t* matches,
    const char* format, va_list args)
{
    fd_t fd;
    status_t status = iowalk(cwd, root, path, &fd);
    if (IS_ERR(status))
    {
        return status;
    }

    status = ioscanv(fd, count, offset, matches, format, args);

    iodrop(fd);

    return status;
}