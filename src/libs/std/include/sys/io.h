#ifndef _SYS_IO_H
#define _SYS_IO_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include "../_AUX/config.h"
#include "../_AUX/fd_t.h"
#include "../_AUX/ERR.h"
#include "../_AUX/NULL.h"

typedef struct pollfd
{
    fd_t fd;
    uint16_t requested;
    uint16_t occurred;
} pollfd_t;

#define POLL_READ (1 << 0)
#define POLL_WRITE (1 << 1)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

_PUBLIC fd_t open(const char* path);

_PUBLIC uint64_t close(fd_t fd);

_PUBLIC uint64_t read(fd_t fd, void* buffer, uint64_t count);

_PUBLIC uint64_t write(fd_t fd, const void* buffer, uint64_t count);

_PUBLIC uint64_t seek(fd_t fd, int64_t offset, uint8_t origin);

_PUBLIC uint64_t poll(pollfd_t* fds, uint64_t amount, uint64_t timeout);

_PUBLIC uint64_t realpath(char* out, const char* path);

_PUBLIC uint64_t chdir(const char* path);

#if defined(__cplusplus)
}
#endif

#endif