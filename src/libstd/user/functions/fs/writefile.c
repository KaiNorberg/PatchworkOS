#include <sys/fs.h>

status_t writefile(const char* path, const void* buffer, size_t count, size_t offset, size_t* bytesWritten)
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

    status = write(fd, buffer, count, bytesWritten);
    close(fd);
    return status;
}
