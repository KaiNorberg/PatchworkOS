#include "_AUX/pixel_t.h"
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
#include "_AUX/nsec_t.h"
#include "_AUX/rect_t.h"

#define MAX_PATH 256
#define MAX_NAME 32

typedef enum poll_event
{
    POLL_READ = (1 << 0),
    POLL_WRITE = (1 << 1)
} poll_event_t;

typedef struct pollfd
{
    fd_t fd;
    poll_event_t requested;
    poll_event_t occurred;
} pollfd_t;

typedef enum stat_type
{
    STAT_FILE = 0,
    STAT_DIR = 1,
    STAT_RES = 2
} stat_type_t;

typedef struct stat
{
    stat_type_t type;
    uint64_t size;
} stat_t;

typedef enum origin
{
    SEEK_SET = 0,
    SEEK_CUR = 1,
    SEEK_END = 2
} seek_origin_t;

typedef struct dir
{
    char name[MAX_NAME];
    stat_type_t type;
} dir_entry_t;

#ifndef __EMBED__

fd_t open(const char* path);

uint64_t close(fd_t fd);

uint64_t read(fd_t fd, void* buffer, uint64_t count);

uint64_t write(fd_t fd, const void* buffer, uint64_t count);

uint64_t seek(fd_t fd, int64_t offset, seek_origin_t origin);

uint64_t realpath(char* out, const char* path);

uint64_t chdir(const char* path);

uint64_t poll(pollfd_t* fds, uint64_t amount, nsec_t timeout);

uint64_t stat(const char* path, stat_t* stat);

uint64_t ioctl(fd_t fd, uint64_t request, void* argp, uint64_t size);

uint64_t flush(fd_t fd, const pixel_t* buffer, uint64_t size, const rect_t* rect);

uint64_t listdir(const char* path, dir_entry_t* entries, uint64_t amount);

uint64_t loaddir(dir_entry_t** out, const char* path);

#endif

#if defined(__cplusplus)
}
#endif

#endif
