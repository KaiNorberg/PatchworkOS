#include <sys/io.h>

status_t iostorep(fd_t cwd, fd_t root, const char* path, const char* in)
{
    if (path == NULL || in == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    return iowritep(cwd, root, path, IOBUF(in, strlen(in)), IOCUR, NULL);
}