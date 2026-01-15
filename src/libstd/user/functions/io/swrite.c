#include <string.h>
#include <sys/io.h>

size_t writes(fd_t fd, const char* string)
{
    size_t length = strlen(string);
    return write(fd, string, length);
}