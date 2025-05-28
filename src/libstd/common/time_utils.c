#include "time_utils.h"

_TimeZone_t timeZone;

void _TimeZoneInit(void)
{
    // TODO: Load this from a file or something
    timeZone = (_TimeZone_t){
        .secondsOffset = 3600 // Swedish time zone for now
    };
}

_TimeZone_t* _TimeZone(void)
{
    return &timeZone;
}

// Il be honest i have no clue if this is correct *shrug*

static const int32_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const int32_t cumulativeDays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
static const int32_t cumulativeDaysLeap[] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};

bool _TimeIsLeapYear(int32_t year)
{
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

int32_t _TimeDaysInMonth(int32_t month, int32_t year)
{
    if (month == 1 && _TimeIsLeapYear(year))
    {
        return 29;
    }
    return daysInMonth[month];
}

void _TimeNormalize(struct tm* timePtr)
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

void _TimeDayOfWeek(struct tm* timePtr)
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

void _TimeDayOfYear(struct tm* timePtr)
{
    int32_t month = timePtr->tm_mon;
    int32_t day = timePtr->tm_mday - 1;
    int32_t year = timePtr->tm_year + 1900;

    if (_TimeIsLeapYear(year))
    {
        timePtr->tm_yday = cumulativeDaysLeap[month] + day;
    }
    else
    {
        timePtr->tm_yday = cumulativeDays[month] + day;
    }
}
