#pragma once

#include "defs/defs.h"

#define RTC_HZ 2

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

#define NANOSECONDS_PER_SECOND 1000000000
#define NANOSECONDS_PER_MILLISECOND 1000000

#define BENCHMARK(func) ({ \
    tty_acquire(); \
    tty_print("Starting benchmark... "); \
    tty_release(); \
    uint64_t start = time_milliseconds(); \
    func(); \
    uint64_t end = time_milliseconds(); \
    tty_acquire(); \
    tty_print("Time taken: "); \
    tty_printi(end - start); \
    tty_print(" MS\n"); \
    tty_release(); \
})

void time_init(void);

void time_accumulate(void);

uint64_t time_seconds(void);

uint64_t time_milliseconds(void);

uint64_t time_nanoseconds(void);