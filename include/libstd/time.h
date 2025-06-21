#ifndef _TIME_H
#define _TIME_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_internal/NULL.h"
#include "_internal/clock_t.h"
#include "_internal/config.h"
#include "_internal/size_t.h"
#include "_internal/time_t.h"
#include "_internal/timespec.h"

#define TIME_UTC 1

struct tm
{
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

_PUBLIC clock_t clock(void);

_PUBLIC double difftime(time_t time1, time_t time0);

_PUBLIC time_t mktime(struct tm* timeptr);

_PUBLIC time_t time(time_t* timer);

_PUBLIC int timespec_get(struct timespec* ts, int base);

_PUBLIC char* asctime(const struct tm* timeptr);

_PUBLIC char* ctime(const time_t* timer);

_PUBLIC struct tm* gmtime(const time_t* timer);

_PUBLIC struct tm* localtime(const time_t* timer);

_PUBLIC struct tm* localtime_r(const time_t* timer, struct tm* buf);

_PUBLIC size_t strftime(char* _RESTRICT s, size_t maxsize, const char* _RESTRICT format,
    const struct tm* _RESTRICT timeptr);

#if (__STDC_WANT_LIB_EXT1__ + 0) != 0

#include "_internal/errno_t.h"
#include "_internal/rsize_t.h"

_PUBLIC errno_t asctime_s(char* s, rsize_t maxsize, const struct tm* timeptr);

_PUBLIC errno_t ctime_s(char* s, rsize_t maxsize, const time_t* timer);

_PUBLIC struct tm* gmtime_s(const time_t* _RESTRICT timer, struct tm* _RESTRICT result);

_PUBLIC struct tm* localtime_s(const time_t* _RESTRICT timer, struct tm* _RESTRICT result);

#endif

#if defined(__cplusplus)
}
#endif

#endif
