#include <sys/io.h>

status_t iostore(fd_t fd, clock_t timeout, const char* in, size_t* bytesWritten)
{
    if (in == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    return iowrite(fd, IOBUF(in, strlen(in)), IOCUR, timeout, bytesWritten);
}