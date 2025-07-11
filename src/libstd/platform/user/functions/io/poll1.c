#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

poll_events_t poll1(fd_t fd, poll_events_t events, clock_t timeout)
{
    pollfd_t pollfd = {.fd = fd, .events = events};
    if (poll(&pollfd, 1, timeout) == ERR)
    {
        return POLL_ERR;
    }
    return pollfd.revents;
}
