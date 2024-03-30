#ifndef _SYS_PROCESS_H
#define _SYS_PROCESS_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include "../_AUX/config.h"

typedef uint64_t pid_t;

_EXPORT pid_t spawn(const char* path);

_EXPORT pid_t getpid(void);

#if defined(__cplusplus)
}
#endif

#endif