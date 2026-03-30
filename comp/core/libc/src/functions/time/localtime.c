#include <time.h>

#include "common/time_utils.h"

struct tm* localtime(const time_t* timer)
{
    static struct tm tm;
    return localtime_r(timer, &tm);
}
