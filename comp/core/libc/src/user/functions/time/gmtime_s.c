#include <time.h>

#include "common/time_utils.h"

struct tm* gmtime_s(const time_t* _RESTRICT timer, struct tm* _RESTRICT result)
{
    if (timer == NULL || result == NULL)
    {
        return NULL;
    }

    time_t t = *timer;

    result->tm_sec = t % 60;
    t /= 60;
    result->tm_min = t % 60;
    t /= 60;
    result->tm_hour = t % 24;
    t /= 24;

    result->tm_wday = (t + 4) % 7;

    uint64_t year = 1970;
    while (true)
    {
        uint64_t daysInYear = _time_is_leap_year(year) ? 366 : 365;
        if (t < daysInYear)
        {
            break;
        }
        t -= daysInYear;
        year++;
    }

    result->tm_year = year - 1900;
    result->tm_yday = (int)t;

    for (uint64_t month = 0; month < 12; month++)
    {
        uint64_t daysInMonth = _time_days_in_month(month, year);
        if (t < daysInMonth)
        {
            result->tm_mon = month;
            result->tm_mday = (int)t + 1;
            break;
        }
        t -= daysInMonth;
    }

    result->tm_isdst = 0;

    return result;
}