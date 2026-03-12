#include <stdio.h>
#include <sys/fs.h>
#include <sys/io.h>

int remove(const char* pathname)
{
    status_t status = ioremovep(IOPATH(pathname));
    if (IS_ERR(status))
    {
        return EOF;
    }

    return 0;
}
