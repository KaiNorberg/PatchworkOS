#include <errno.h>
#include <libstd/fs.h>
#include <libstd/io.h>
#include <stdio.h>
#include <stdlib.h>

int rename(const char* oldpath, const char* newpath)
{
    fd_t oldfd;
    status_t status = iowalk(IOPATH(oldpath), &oldfd);
    if (IS_ERR(status))
    {
        errno = ENOENT;
        return -1;
    }

    fd_t checkfd;
    if (!IS_ERR(iowalk(IOPATH(newpath), &checkfd)))
    {
        file_info_t oldInfo;
        file_info_t newInfo;
        if (!IS_ERR(ioquery(oldfd, &oldInfo)) && !IS_ERR(ioquery(checkfd, &newInfo)))
        {
            if (oldInfo.volume == newInfo.volume && oldInfo.number == newInfo.number)
            {
                iodrop(checkfd);
                iodrop(oldfd);
                return 0;
            }
        }
        iodrop(checkfd);
        ioremovep(IOPATH(newpath));
    }

    fd_t newfd;
    status = iowalk(IOPATH(IOFMT("%s:hardlink?%lld", newpath, oldfd)), &newfd);
    if (IS_ERR(status))
    {
        iodrop(oldfd);
        errno = EACCES;
        return -1;
    }
    iodrop(newfd);

    ioremovep(IOPATH(oldpath));
    iodrop(oldfd);
    return 0;
}
