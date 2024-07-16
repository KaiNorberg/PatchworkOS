#pragma once

#include <sys/proc.h>

#include "defs.h"

#define RTC_HZ 2

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

void time_init(void);

nsec_t time_uptime(void);
