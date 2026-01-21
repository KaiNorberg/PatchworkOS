#include <string.h>
#include <sys/fs.h>

size_t writefiles(const char* path, const char* string)
{
    fd_t fd = open(path);
    if (fd == ERR)
    {
        return ERR;
    }
    uint64_t totalWritten = writes(fd, string);
    close(fd);
    return totalWritten;
}