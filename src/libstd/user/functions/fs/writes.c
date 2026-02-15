#include <string.h>
#include <sys/fs.h>
#include <sys/io.h>

status_t writes(fd_t fd, const char* string, size_t* bytesWritten)
{
    size_t length = strlen(string);
    return iowrite(fd, string, length, IOCUR, bytesWritten);
}