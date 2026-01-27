#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

int mkdir(const char* path)
{
    fd_t fd;
    status_t status = open(&fd, F("%s:create:directory", path));
    if (IS_ERR(status))
    {
        return EOF;
    }
    close(fd);
    return 0;
}
