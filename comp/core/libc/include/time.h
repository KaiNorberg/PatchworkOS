#ifndef _TIME_H
#define _TIME_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libc/NULL.h"
#include "_libc/clock_t.h"
#include "_libc/config.h"
#include "_libc/size_t.h"
#include "_libc/time_t.h"
#include "_libc/timespec.h"

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

clock_t clock(void);

double difftime(time_t time1, time_t time0);

time_t mktime(struct tm* timeptr);

time_t time(time_t* timer);

int timespec_get(struct timespec* ts, int base);

char* asctime(const struct tm* timeptr);

char* ctime(const time_t* timer);

struct tm* gmtime(const time_t* timer);

struct tm* localtime(const time_t* timer);

struct tm* localtime_r(const time_t* timer, struct tm* buf);

size_t strftime(char* _RESTRICT s, size_t maxsize, const char* _RESTRICT format, const struct tm* _RESTRICT timeptr);

#if (__STDC_WANT_LIB_EXT1__ + 0) != 0

#include "_libc/errno_t.h"
#include "_libc/rsize_t.h"

errno_t asctime_s(char* s, rsize_t maxsize, const struct tm* timeptr);

errno_t ctime_s(char* s, rsize_t maxsize, const time_t* timer);

struct tm* gmtime_s(const time_t* _RESTRICT timer, struct tm* _RESTRICT result);

struct tm* localtime_s(const time_t* _RESTRICT timer, struct tm* _RESTRICT result);

#endif

#if defined(__cplusplus)
}
#endif

#endif
