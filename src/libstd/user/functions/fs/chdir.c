#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

status_t chdir(const char* path)
{
    return writefiles("/proc/self/cwd", path);
}
