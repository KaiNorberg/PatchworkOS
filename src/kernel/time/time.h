#pragma once

#include <stdint.h>

#define TICKS_PER_SECOND 1024

void time_init();

uint64_t time_get_seconds();

uint64_t time_get_milliseconds();

uint64_t time_get_tick();

void time_tick();