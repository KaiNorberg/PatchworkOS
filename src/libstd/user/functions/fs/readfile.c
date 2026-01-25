#include <sys/fs.h>

size_t readfile(const char* path, void* buffer, size_t count, size_t offset)
{
    fd_t fd = open(path);
    if (fd == _FAIL)
    {
        return _FAIL;
    }

    if (offset != 0 && seek(fd, offset, SEEK_SET) == _FAIL)
    {
        close(fd);
        return _FAIL;
    }

    uint64_t bytesRead = read(fd, buffer, count);
    close(fd);
    return bytesRead;
}
