#include <sys/syscall.h>
#include <time.h>

time_t time(time_t* timePtr)
{
    uint64_t time;
    syscall0(SYS_TIME, &time);
    if (timePtr != NULL)
    {
        *timePtr = time;
    }
    return time;
}
