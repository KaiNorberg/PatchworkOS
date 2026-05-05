#include <_libc/MAX_PATH.h>
#include <errno.h>
#include <libc/io.h>
#include <sys/stat.h>
#include <sys/types.h>

int mkdir(const char* path, mode_t mode)
{
    char stringMode[MAX_PATH];
    status_t status = path_posix_to_string(mode, stringMode, sizeof(stringMode), NULL);
    if (IS_ERR(status))
    {
        errno = status_to_errno(status);
        return -1;
    }

    fd_t fd;
    status = iowalk(FDCWD, FDROOT, IOFMT("%s:d:%s", path, stringMode), &fd);
    if (IS_ERR(status))
    {
        errno = status_to_errno(status);
        return -1;
    }

    iodrop(fd);
    return 0;
}
