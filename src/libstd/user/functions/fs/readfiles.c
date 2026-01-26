#include <sys/fs.h>

status_t readfiles(char** out, const char* path)
{
    fd_t fd;
    status_t status = open(&fd, path);
    if (IS_ERR(status))
    {
        return status;
    }
    status = reads(out, fd);
    close(fd);
    return status;
}