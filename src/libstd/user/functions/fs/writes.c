#include <string.h>
#include <sys/fs.h>

status_t writes(fd_t fd, const char* string, size_t* bytesWritten)
{
    size_t length = strlen(string);
    return write(fd, string, length, bytesWritten);
}