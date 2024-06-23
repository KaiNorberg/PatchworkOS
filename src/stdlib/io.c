#ifndef __EMBED__

#include <sys/io.h>

uint64_t poll1(fd_t fd, uint16_t requested, uint64_t timeout)
{
    pollfd_t array[] = {{.fd = fd, .requested = requested}};
    if (poll(array, 1, timeout) == ERR)
    {
        return ERR;
    }
    return array[0].occurred;
}

#endif
