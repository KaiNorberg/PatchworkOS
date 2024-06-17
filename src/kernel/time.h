#pragma once

#include <sys/proc.h>

#include "defs.h"

#define RTC_HZ 2

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

#define NANOSECONDS_PER_SECOND 1000000000
#define NANOSECONDS_PER_MILLISECOND 1000000

void time_init(void);

nsec_t time_uptime(void);
