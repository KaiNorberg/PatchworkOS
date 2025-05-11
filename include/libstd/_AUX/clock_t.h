#ifndef _AUX_CLOCK_T_H
#define _AUX_CLOCK_T_H 1

// We use clock_t to represent any nanosecond time.

typedef unsigned long long clock_t;
#define CLOCKS_PER_SEC ((clock_t)1000000000) // Nanoseconds per second
#define CLOCKS_NEVER (__UINT64_MAX__)        // Extension

#endif
