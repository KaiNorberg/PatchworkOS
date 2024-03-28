#ifndef _SYS_PROCESS_H
#define _SYS_PROCESS_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include "../auxiliary/config.h"

typedef uint64_t pid_t;

_EXPORT pid_t spawn(const char* path);

#if defined(__cplusplus)
}
#endif

#endif