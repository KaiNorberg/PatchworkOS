#include <sys/io.h>
#include <string.h>

uint64_t swrite(fd_t fd, const char* string)
{
    size_t length = strlen(string);
    return write(fd, string, length);
}