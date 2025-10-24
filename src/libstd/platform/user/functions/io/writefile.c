#include <sys/io.h>

uint64_t writefile(const char* path, const void* buffer, uint64_t count, uint64_t offset)
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

    uint64_t bytesWritten = write(fd, buffer, count);
    close(fd);
    return bytesWritten;
}
