#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "user/common/syscalls.h"

uint64_t chdir(const char* path)
{
    if (swritefile("/proc/self/cwd", path) == ERR)
    {
        return ERR;
    }

    return 0;
}
