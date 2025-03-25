#include "time_zone.h"

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