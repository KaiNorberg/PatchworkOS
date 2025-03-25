#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#include "common/time_zone.h"
#include "platform/platform.h"

#if _PLATFORM_HAS_SCHEDULING == 1
time_t time(time_t* timePtr)
{
    return _PlatformTime(timePtr);
}
#endif

// Il be honest i have no clue if this is correct *shrug*

static const int32_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const int32_t cumulativeDays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
static const int32_t cumulativeDaysLeap[] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};

static bool _TimeLeapYear(int32_t year)
{
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

static int32_t _TimeDaysInMonth(int32_t month, int32_t year)
{
    if (month == 1 && _TimeLeapYear(year))
    {
        return 29;
    }
    return daysInMonth[month];
}

static void _TimeNormalize(struct tm* timePtr)
{
    int32_t carry = timePtr->tm_sec / 60;
    timePtr->tm_sec %= 60;
    if (timePtr->tm_sec < 0)
    {
        timePtr->tm_sec += 60;
        carry--;
    }

    timePtr->tm_min += carry;
    carry = timePtr->tm_min / 60;
    timePtr->tm_min %= 60;
    if (timePtr->tm_min < 0)
    {
        timePtr->tm_min += 60;
        carry--;
    }

    timePtr->tm_hour += carry;
    carry = timePtr->tm_hour / 24;
    timePtr->tm_hour %= 24;
    if (timePtr->tm_hour < 0)
    {
        timePtr->tm_hour += 24;
        carry--;
    }

    timePtr->tm_year += timePtr->tm_mon / 12;
    timePtr->tm_mon %= 12;
    if (timePtr->tm_mon < 0)
    {
        timePtr->tm_mon += 12;
        timePtr->tm_year--;
    }

    while (1)
    {
        int32_t maxDays = _TimeDaysInMonth(timePtr->tm_mon, timePtr->tm_year + 1900);

        if (timePtr->tm_mday > maxDays)
        {
            timePtr->tm_mday -= maxDays;
            timePtr->tm_mon++;
            if (timePtr->tm_mon >= 12)
            {
                timePtr->tm_mon = 0;
                timePtr->tm_year++;
            }
        }
        else if (timePtr->tm_mday < 1)
        {
            timePtr->tm_mon--;
            if (timePtr->tm_mon < 0)
            {
                timePtr->tm_mon = 11;
                timePtr->tm_year--;
            }
            timePtr->tm_mday += _TimeDaysInMonth(timePtr->tm_mon, timePtr->tm_year + 1900);
        }
        else
        {
            break;
        }
    }
}

static void _TimeDayOfWeek(struct tm* timePtr)
{
    int32_t y = timePtr->tm_year + 1900;
    int32_t m = timePtr->tm_mon + 1;
    int32_t d = timePtr->tm_mday;

    if (m < 3)
    {
        m += 12;
        y--;
    }

    int32_t k = y % 100;
    int32_t j = y / 100;
    int32_t h = (d + 13 * (m + 1) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;

    timePtr->tm_wday = (h + 6) % 7;
}

static void _TimeDayOfYear(struct tm* timePtr)
{
    int32_t month = timePtr->tm_mon;
    int32_t day = timePtr->tm_mday - 1;
    int32_t year = timePtr->tm_year + 1900;

    if (_TimeLeapYear(year))
    {
        timePtr->tm_yday = cumulativeDaysLeap[month] + day;
    }
    else
    {
        timePtr->tm_yday = cumulativeDays[month] + day;
    }
}

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
        totalDays += 365 + (_TimeLeapYear(y) ? 1 : 0);
    }

    totalDays += timePtr->tm_yday;

    time_t epochTime = (time_t)(totalDays * 86400LL + timePtr->tm_hour * 3600 + timePtr->tm_min * 60 + timePtr->tm_sec);

    if (timePtr->tm_isdst > 0)
    {
        epochTime -= 3600;
    }

    return epochTime;
}

struct tm* localtime(const time_t* timer)
{
    static struct tm tm;
    return localtime_r(timer, &tm);
}

struct tm* localtime_r(const time_t* timer, struct tm* buf)
{
    if (!timer)
    {
        return NULL;
    }

    _TimeZone_t* timeZone = _TimeZone();

    time_t seconds = *timer + timeZone->secondsOffset;
    int64_t daysElapsed = seconds / 86400;
    int32_t secondsOfDay = seconds % 86400;

    int32_t year = 1970;
    while (1)
    {
        int32_t daysInYear = 365 + (_TimeLeapYear(year) ? 1 : 0);
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
    while (daysElapsed >= _TimeDaysInMonth(month, year))
    {
        daysElapsed -= _TimeDaysInMonth(month, year);
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