#include <libc/fs.h>
#include <libc/io.h>
#include <stdio.h>

int remove(const char* pathname)
{
    status_t status = ioremovep(IOPATH(pathname));
    if (IS_ERR(status))
    {
        return EOF;
    }

    return 0;
}
