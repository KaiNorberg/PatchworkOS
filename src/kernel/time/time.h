#pragma once

#include "types/types.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

#define RTC_HZ 2

#define NANOSECONDS_PER_SECOND 1000000000
#define NANOSECONDS_PER_MILLISECOND 1000000

void time_init(void);

void time_accumulate(void);

uint64_t time_seconds(void);

uint64_t time_milliseconds(void);

uint64_t time_nanoseconds(void);