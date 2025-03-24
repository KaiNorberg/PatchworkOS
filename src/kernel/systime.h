#pragma once

#include <sys/proc.h>
#include <time.h>

#include "defs.h"

#define RTC_HZ 2

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

void systime_init(void);

nsec_t systime_uptime(void);

time_t systime_time(void);
