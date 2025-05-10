#include <time.h>

#include "platform/user/common/syscalls.h"

time_t time(time_t* timePtr)
{
    time_t time = _SyscallUnixEpoch();
    if (timePtr != NULL)
    {
        *timePtr = time;
    }
    return time;
}
