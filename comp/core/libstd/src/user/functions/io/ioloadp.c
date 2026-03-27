#include <libstd/io.h>

status_t ioloadp(fd_t cwd, fd_t root, const char* path, char** out, size_t* outLen)
{
    fd_t file;
    status_t status = iowalk(cwd, root, path, &file);
    if (IS_ERR(status))
    {
        return status;
    }

    status = ioload(file, out, outLen);

    iodrop(file);

    return status;
}