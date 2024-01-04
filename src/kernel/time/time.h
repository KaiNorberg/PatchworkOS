#pragma once

#include <stdint.h>

#define TICKS_PER_SECOND 100
#define NANOSECONDS_PER_SECOND 1000000000

typedef struct
{
    uint64_t seconds;
    uint64_t nanoSeconds;
} TimeSpec;

void time_init();

uint64_t time_seconds();

uint64_t time_milliseconds();

uint64_t time_nanoseconds();

uint64_t time_get_tick();

void time_tick();