#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "platform/user/common/syscalls.h"
#include "platform/user/common/thread.h"

int thrd_sleep(const struct timespec* duration, struct timespec* remaining)
{
    uint64_t nanoseconds = (uint64_t)duration->tv_sec * CLOCKS_PER_SEC + (uint64_t)duration->tv_nsec;

    if (remaining != NULL)
    {
        clock_t start = uptime();
        sleep(nanoseconds);
        clock_t end = uptime();

        clock_t timeTaken = end - start;
        remaining->tv_sec = timeTaken / CLOCKS_PER_SEC;
        remaining->tv_nsec = timeTaken % CLOCKS_PER_SEC;
    }
    else
    {
        sleep(nanoseconds);
    }

    return 0;
}
