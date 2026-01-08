#include <sys/io.h>

size_t readfile(const char* path, void* buffer, size_t count, size_t offset)
{
    fd_t fd = open(path);
    if (fd == ERR)
    {
        return ERR;
    }

    if (offset != 0 && seek(fd, offset, SEEK_SET) == ERR)
    {
        close(fd);
        return ERR;
    }

    uint64_t bytesRead = read(fd, buffer, count);
    close(fd);
    return bytesRead;
}
