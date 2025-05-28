#include <time.h>

#include "common/time_utils.h"

time_t mktime(struct tm* timePtr)
{
    if (!timePtr)
    {
        return (time_t)(-1);
    }

    _TimeNormalize(timePtr);
    _TimeDayOfWeek(timePtr);
    _TimeDayOfYear(timePtr);

    int64_t totalDays = 0;
    int32_t year = timePtr->tm_year + 1900;

    for (int32_t y = 1970; y < year; y++)
    {
        totalDays += 365 + (_TimeIsLeapYear(y) ? 1 : 0);
    }

    totalDays += timePtr->tm_yday;

    time_t epochTime = (time_t)(totalDays * 86400LL + timePtr->tm_hour * 3600 + timePtr->tm_min * 60 + timePtr->tm_sec);

    if (timePtr->tm_isdst > 0)
    {
        epochTime -= 3600;
    }

    return epochTime;
}
