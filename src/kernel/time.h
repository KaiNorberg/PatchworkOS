#pragma once

#include <sys/proc.h>

#include "defs.h"

#define RTC_HZ 2

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

#define NANOSECONDS_PER_SECOND 1000000000
#define NANOSECONDS_PER_MILLISECOND 1000000

#define BENCHMARK(func, iter) ({ \
    tty_acquire(); \
    tty_print("Starting benchmark... "); \
    tty_release(); \
    nsec_t start = time_uptime(); \
    for (uint64_t i = 0; i < iter; i++) \
    { \
        func; \
    } \
    nsec_t end = time_uptime(); \
    tty_acquire(); \
    tty_print("Time taken: "); \
    tty_printi(end - start); \
    tty_print(" NS\n"); \
    tty_release(); \
})

void time_init(void);

nsec_t time_uptime(void);