#include <string.h>
#include <sys/fs.h>

status_t writefiles(const char* path, const char* string)
{
    fd_t fd;
    status_t status = open(&fd, path);
    if (IS_ERR(status))
    {
        return status;
    }
    status = writes(fd, string, NULL);
    close(fd);
    return status;
}