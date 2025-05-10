#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// TODO: Time zone handling is NOT compliant with the c standard (for now). Daylight savings time.
typedef struct
{
    int64_t secondsOffset;
} _TimeZone_t;

void _TimeZoneInit(void);

_TimeZone_t* _TimeZone(void);

bool _TimeLeapYear(int32_t year);

int32_t _TimeDaysInMonth(int32_t month, int32_t year);

void _TimeNormalize(struct tm* timePtr);

void _TimeDayOfWeek(struct tm* timePtr);

void _TimeDayOfYear(struct tm* timePtr);
