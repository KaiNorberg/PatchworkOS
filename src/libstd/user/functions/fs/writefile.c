#include <sys/fs.h>
#include <sys/io.h>

status_t writefile(const char* path, const void* buffer, size_t count, ssize_t offset, size_t* bytesWritten)
{
    fd_t fd;
    status_t status = open(&fd, path);
    if (IS_ERR(status))
    {
        return status;
    }

    status = iowrite(fd, buffer, count, offset, bytesWritten);
    close(fd);
    return status;
}
