#pragma once

#include <stdint.h>

void rtc_init(uint8_t rate);

uint64_t rtc_get_tick();

void rtc_tick();