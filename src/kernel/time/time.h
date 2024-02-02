#pragma once

#include <stdint.h>

#define NANOSECONDS_PER_SECOND 1000000000
#define NANOSECONDS_PER_MILLISECOND 1000000

void time_init();

void time_accumulate();

uint64_t time_seconds();

uint64_t time_milliseconds();

uint64_t time_nanoseconds();