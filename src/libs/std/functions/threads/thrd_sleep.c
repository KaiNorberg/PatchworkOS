#include <threads.h>

#include "internal/syscalls/syscalls.h"

int thrd_sleep(const struct timespec* duration, struct timespec* remaining)
{
    //Sleep is currently uninterruptible so "remaining" is ignored.
    uint64_t nanoseconds = (uint64_t)duration->tv_sec * 1000000000ULL + (uint64_t)duration->tv_nsec;
    SYSCALL(SYS_SLEEP, 1, nanoseconds);
    return 0;
}