#include <sys/io.h>

status_t iostorep(fd_t fd, const char* path, const char* in)
{
    if (path == NULL || in == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    return iowritep(fd, path, IOBUF(in, strlen(in)), IOCUR, NULL);
}