#include <sys/fs.h>
#include <sys/io.h>

status_t readfile(const char* path, void* buffer, size_t count, ssize_t offset, size_t* bytesRead)
{
    fd_t fd;
    status_t status = open(&fd, path);
    if (IS_ERR(status))
    {
        return status;
    }

    status = ioread(fd, buffer, count, offset, bytesRead);
    close(fd);
    return status;
}
