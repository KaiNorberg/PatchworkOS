#include <stdio.h>
#include <sys/io.h>

int remove(const char* pathname)
{
    uint64_t result = unlink(pathname);
    if (result != ERR)
    {
        return result;
    }

    result = rmdir(pathname);
    if (result != ERR)
    {
        return result;
    }

    return EOF;
}
