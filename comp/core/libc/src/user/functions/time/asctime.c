#include <stdio.h>
#include <time.h>

char* asctime(const struct tm* timeptr)
{
    static char days[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    static char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    static char buffer[26];
    sprintf(buffer, "%s %s%3d %.2d:%.2d:%.2d %d\n", days[timeptr->tm_wday], months[timeptr->tm_mon], timeptr->tm_mday,
        timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec, timeptr->tm_year + 1900);
    return buffer;
}