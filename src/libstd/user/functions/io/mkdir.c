#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

#include "user/common/syscalls.h"

uint64_t mkdir(const char* path)
{
    fd_t fd = open(F("%s:create:directory", path));
    if (fd == ERR)
    {
        return ERR;
    }
    close(fd);
    return 0;
}
