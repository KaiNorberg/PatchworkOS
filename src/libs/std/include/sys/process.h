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

uint64_t uptime(void);

uint64_t sleep(uint64_t nanoseconds);

pid_t spawn(const char* path);

pid_t getpid(void);

#if defined(__cplusplus)
}
#endif

#endif