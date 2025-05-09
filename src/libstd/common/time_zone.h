#pragma once

#include <stdint.h>

// TODO: Time zone handling is NOT compliant with the c standard (for now). Daylight savings time.
typedef struct
{
    int64_t secondsOffset;
} _TimeZone_t;

void _TimeZoneInit(void);

_TimeZone_t* _TimeZone(void);