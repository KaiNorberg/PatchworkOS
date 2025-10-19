#include "clock.h"

#include <sys/proc.h>

static clock_t startTime = 0;

void _clock_init(void)
{
    startTime = uptime();
}

clock_t _clock_get(void)
{
    return uptime() - startTime;
}
