#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/// @todo Time zone handling is not compliant with the c standard (for now). Daylight savings time.

typedef struct
{
    int64_t secondsOffset;
} _time_zone_t;

void _time_zone_init(void);

_time_zone_t* _time_zone(void);

bool _time_is_leap_year(int32_t year);

int32_t _time_days_in_month(int32_t month, int32_t year);

void _time_normalize(struct tm* timePtr);

void _time_day_of_week(struct tm* timePtr);

void _time_day_of_year(struct tm* timePtr);
