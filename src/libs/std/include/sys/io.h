#ifndef _SYS_IO_H
#define _SYS_IO_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include "../_AUX/config.h"
#include "../_AUX/size_t.h"

#define O_CREATE (1 << 0)
#define O_READ (1 << 1)
#define O_WRITE (1 << 2)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef uint64_t fd_t;

_EXPORT fd_t open(const char* path, uint8_t flags);

_EXPORT uint64_t close(fd_t fd);

_EXPORT uint64_t read(fd_t fd, void* buffer, uint64_t count);

_EXPORT uint64_t write(fd_t fd, const void* buffer, uint64_t count);

_EXPORT uint64_t seek(fd_t fd, int64_t offset, uint8_t origin);

#if defined(__cplusplus)
}
#endif

#endif