#include <sys/fs.h>

status_t readfile(const char* path, void* buffer, size_t count, size_t offset, size_t* bytesRead)
{
    fd_t fd;
    status_t status = open(&fd, path);
    if (IS_ERR(status))
    {
        return status;
    }

    if (offset != 0)
    {
        status = seek(fd, offset, SEEK_SET, NULL);
        if (IS_ERR(status))
        {
            close(fd);
            return status;
        }
    }

    status = read(fd, buffer, count, bytesRead);
    close(fd);
    return status;
}
