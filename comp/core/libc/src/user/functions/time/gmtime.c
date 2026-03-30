#include <time.h>

struct tm* gmtime(const time_t* timer)
{
    static struct tm t;
    gmtime_s(timer, &t);
    return &t;
}