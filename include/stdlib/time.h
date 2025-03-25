#ifndef _TIME_H
#define _TIME_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/config.h"
#include "_AUX/time_t.h"

struct tm
{
    int tm_sec;   /* Seconds.	[0-60] (1 leap second) */
    int tm_min;   /* Minutes.	[0-59] */
    int tm_hour;  /* Hours.	[0-23] */
    int tm_mday;  /* Day.		[1-31] */
    int tm_mon;   /* Month.	[0-11] */
    int tm_year;  /* Year	- 1900.  */
    int tm_wday;  /* Day of week.	[0-6] */
    int tm_yday;  /* Days in year.[0-365]	*/
    int tm_isdst; /* DST.		[-1/0/1]*/
};

time_t time(time_t* timePtr);
time_t mktime(struct tm* timePtr);

struct tm* localtime(const time_t* timer);
struct tm* localtime_r(const time_t* timer, struct tm* buf);

#if defined(__cplusplus)
}
#endif

#endif
