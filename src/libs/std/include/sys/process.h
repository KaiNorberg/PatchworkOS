#ifndef _SYS_PROCESS_H
#define _SYS_PROCESS_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include "../_AUX/config.h"

//One second in nanoseconds.
#define SEC 1000000000

typedef uint64_t pid_t;

_PUBLIC uint64_t uptime(void);

_PUBLIC uint64_t sleep(uint64_t nanoseconds);

_PUBLIC pid_t spawn(const char* path);

_PUBLIC pid_t getpid(void);

#if defined(__cplusplus)
}
#endif

#endif