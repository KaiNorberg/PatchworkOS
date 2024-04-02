#ifndef _SYS_IO_H
#define _SYS_IO_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include "../_AUX/config.h"

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef uint64_t fd_t;

_EXPORT fd_t open(const char* path);

_EXPORT uint64_t close(fd_t fd);

_EXPORT uint64_t read(fd_t fd, void* buffer, uint64_t count);

_EXPORT uint64_t write(fd_t fd, const void* buffer, uint64_t count);

_EXPORT uint64_t seek(fd_t fd, int64_t offset, uint8_t origin);

#if defined(__cplusplus)
}
#endif

#endif