#include <string.h>
#include <sys/fs.h>
#include <sys/ioring.h>

status_t writes(fd_t fd, const char* string, size_t* bytesWritten)
{
    size_t length = strlen(string);
    return iowrite(fd, string, length, IOOFF_CUR, bytesWritten);
}