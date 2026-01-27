#include <stdio.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

int rename(const char* oldpath, const char* newpath)
{
    if (IS_ERR(link(oldpath, newpath)))
    {
        return EOF;
    }
    if (remove(oldpath) == EOF)
    {
        return EOF;
    }
    return 0;
}
