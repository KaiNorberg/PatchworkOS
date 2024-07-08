#ifndef _SYS_IO_H
#define _SYS_IO_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/ERR.h"
#include "_AUX/NULL.h"
#include "_AUX/config.h"
#include "_AUX/fd_t.h"
#include "_AUX/rect_t.h"

typedef struct pollfd
{
    fd_t fd;
    uint16_t requested;
    uint16_t occurred;
} pollfd_t;

#define POLL_READ (1 << 0)
#define POLL_WRITE (1 << 1)

typedef struct stat
{
    uint8_t type;
    uint64_t size;
} stat_t;

#define STAT_FILE 0
#define STAT_DIR 1
#define STAT_RES 2

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define MAX_PATH 256

#ifndef __EMBED__

fd_t open(const char* path);

uint64_t close(fd_t fd);

uint64_t read(fd_t fd, void* buffer, uint64_t count);

uint64_t write(fd_t fd, const void* buffer, uint64_t count);

uint64_t seek(fd_t fd, int64_t offset, uint8_t origin);

uint64_t realpath(char* out, const char* path);

uint64_t chdir(const char* path);

uint64_t poll(pollfd_t* fds, uint64_t amount, uint64_t timeout);

uint64_t poll1(fd_t fd, uint16_t requested, uint64_t timeout);

uint64_t stat(const char* path, stat_t* buffer);

uint64_t ioctl(fd_t fd, uint64_t request, void* argp, uint64_t size);

uint64_t flush(fd_t fd, const void* buffer, uint64_t size, const rect_t* rect);

#endif

#if defined(__cplusplus)
}
#endif

#endif
