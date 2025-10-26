#include <time.h>

#include "user/common/syscalls.h"

time_t time(time_t* timePtr)
{
    time_t time = _syscall_unix_epoch();
    if (timePtr != NULL)
    {
        *timePtr = time;
    }
    return time;
}
