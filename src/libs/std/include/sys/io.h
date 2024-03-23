#ifndef _SYS_IO_H
#define _SYS_IO_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include "../auxiliary/config.h"

#define O_CREATE (1 << 0)
#define O_READ (1 << 1)
#define O_WRITE (1 << 2)

//_EXPORT uint64_t open(const char* path, uint64_t flags);

#if defined(__cplusplus)
}
#endif

#endif