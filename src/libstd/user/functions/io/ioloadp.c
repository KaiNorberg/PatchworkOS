#include <sys/io.h>

status_t ioloadp(fd_t fd, const char* path, char** out, size_t* outLen)
{
    fd_t file;
    status_t status = ioopen(fd, path, NULL, 0, CLOCKS_NEVER, &file);
    if (IS_ERR(status))
    {
        return status;
    }

    status = ioload(file, CLOCKS_NEVER, out, outLen);

    ioclose(file, CLOCKS_NEVER);

    return status;
}