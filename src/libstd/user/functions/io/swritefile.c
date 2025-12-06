#include <sys/io.h>
#include <string.h>

uint64_t swritefile(const char* path, const char* string)
{
    fd_t fd = open(path);
    if (fd == ERR)
    {
        return ERR;
    }
    uint64_t totalWritten = swrite(fd, string);
    close(fd);
    return totalWritten;
}