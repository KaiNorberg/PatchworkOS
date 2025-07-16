#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t mkdir(const char* path)
{
    fd_t fd = openf("%s:create:dir", path);
    if (fd == ERR)
    {
        return ERR;
    }
    close(fd);
    return 0;
}
