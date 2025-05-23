#ifndef _SYS_IO_H
#define _SYS_IO_H 1

#include <stdarg.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/ERR.h"
#include "_AUX/MAX_NAME.h"
#include "_AUX/MAX_PATH.h"
#include "_AUX/NULL.h"
#include "_AUX/SEEK.h"
#include "_AUX/clock_t.h"
#include "_AUX/config.h"
#include "_AUX/fd_t.h"

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define PIPE_READ 0
#define PIPE_WRITE 1

typedef enum poll_event
{
    POLL_READ = (1 << 0),
    POLL_WRITE = (1 << 1),
    POLL1_ERR = (1 << 31)
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
} stat_type_t;

typedef struct stat
{
    char name[MAX_NAME];
    stat_type_t type;
    uint64_t size;
} stat_t;

typedef uint8_t seek_origin_t;

typedef struct allocdir
{
    uint64_t amount;
    stat_t infos[];
} allocdir_t;

fd_t open(const char* path);

fd_t openf(const char* _RESTRICT format, ...);

fd_t vopenf(const char* _RESTRICT format, va_list args);

uint64_t open2(const char* path, fd_t fd[2]);

uint64_t close(fd_t fd);

uint64_t read(fd_t fd, void* buffer, uint64_t count);

uint64_t write(fd_t fd, const void* buffer, uint64_t count);

uint64_t writef(fd_t fd, const char* _RESTRICT format, ...);

uint64_t vwritef(fd_t fd, const char* _RESTRICT format, va_list args);

uint64_t seek(fd_t fd, int64_t offset, seek_origin_t origin);

uint64_t chdir(const char* path);

uint64_t poll(pollfd_t* fds, uint64_t amount, clock_t timeout);

poll_event_t poll1(fd_t fd, poll_event_t requested, clock_t timeout);

uint64_t stat(const char* path, stat_t* stat);

uint64_t ioctl(fd_t fd, uint64_t request, void* argp, uint64_t size);

fd_t dup(fd_t oldFd);

fd_t dup2(fd_t oldFd, fd_t newFd);

uint64_t readdir(fd_t fd, stat_t* infos, uint64_t amount);

allocdir_t* allocdir(fd_t fd);

uint64_t mkdir(const char* path);

#if defined(__cplusplus)
}
#endif

#endif
