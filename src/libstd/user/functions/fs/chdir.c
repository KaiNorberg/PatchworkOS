#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

int chdir(const char* path)
{
    return IS_ERR(writefiles("/proc/self/cwd", path)) ? -1 : 0;
}
