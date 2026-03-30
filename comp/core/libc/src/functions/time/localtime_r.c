#include <stddef.h>
#include <time.h>

#include "common/time_utils.h"

struct tm* localtime_r(const time_t* timer, struct tm* buf)
{
    if (!timer)
    {
        return NULL;
    }

    _time_zone_t* timeZone = _time_zone();

    time_t seconds = *timer + timeZone->secondsOffset;
    int64_t daysElapsed = seconds / 86400;
    int32_t secondsOfDay = seconds % 86400;

    int32_t year = 1970;
    while (1)
    {
        int32_t daysInYear = 365 + (_time_is_leap_year(year) ? 1 : 0);
        if (daysElapsed < daysInYear)
        {
            break;
        }
        daysElapsed -= daysInYear;
        year++;
    }

    buf->tm_year = year - 1900;
    buf->tm_yday = daysElapsed;

    int32_t month = 0;
    while (daysElapsed >= _time_days_in_month(month, year))
    {
        daysElapsed -= _time_days_in_month(month, year);
        month++;
    }

    buf->tm_mon = month;
    buf->tm_mday = daysElapsed + 1;

    buf->tm_hour = secondsOfDay / 3600;
    buf->tm_min = (secondsOfDay % 3600) / 60;
    buf->tm_sec = secondsOfDay % 60;

    buf->tm_wday = (daysElapsed + 4) % 7;
    buf->tm_isdst = 0;

    return buf;
}
