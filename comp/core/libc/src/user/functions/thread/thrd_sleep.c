#include <libc/proc.h>
#include <libc/syscall.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <threads.h>
#include <time.h>

#include "user/common/threading.h"

int thrd_sleep(const struct timespec* duration, struct timespec* remaining)
{
    clock_t nanoseconds = (clock_t)duration->tv_sec * CLOCKS_PER_SEC + (clock_t)duration->tv_nsec;

    if (remaining != NULL)
    {
        clock_t start = clock();
        syscall1(SYS_THRD_SLEEP, NULL, nanoseconds);
        clock_t end = clock();

        clock_t timeLeft = nanoseconds - (end - start);
        remaining->tv_sec = timeLeft / CLOCKS_PER_SEC;
        remaining->tv_nsec = timeLeft % CLOCKS_PER_SEC;
    }
    else
    {
        syscall1(SYS_THRD_SLEEP, NULL, nanoseconds);
    }

    return 0;
}
