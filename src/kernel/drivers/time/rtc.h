#pragma once

#include <time.h>

#define RTC_HZ 2

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

void rtc_init(void);

void rtc_read(struct tm* time);