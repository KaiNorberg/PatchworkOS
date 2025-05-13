#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

dir_list_t* dir_alloc(const char* path)
{
    uint64_t amount = _SyscallDirList(path, NULL, 0);
    if (amount == ERR)
    {
        return NULL;
    }

    dir_list_t* list = malloc(sizeof(dir_list_t) + sizeof(dir_entry_t) * amount);
    if (list == NULL)
    {
        return NULL;
    }

    list->amount = amount;
    if (_SyscallDirList(path, list->entries, amount) == ERR)
    {
        free(list);
        return NULL;
    }

    return list;
}

uint64_t dir_list(const char* path, dir_entry_t* entries, uint64_t amount)
{
    return _SyscallDirList(path, entries, amount);
}

fd_t open(const char* path)
{
    return _SyscallOpen(path);
}

fd_t openf(const char* _RESTRICT format, ...)
{
    char path[MAX_PATH];

    va_list args;
    va_start(args, format);
    vsnprintf(path, MAX_PATH, format, args);
    va_end(args);

    return _SyscallOpen(path);
}

fd_t vopenf(const char* _RESTRICT format, va_list args)
{
    char path[MAX_PATH];
    vsnprintf(path, MAX_PATH, format, args);
    return _SyscallOpen(path);
}

uint64_t open2(const char* path, fd_t fds[2])
{
    return _SyscallOpen2(path, fds);
}

uint64_t close(fd_t fd)
{
    return _SyscallClose(fd);
}

uint64_t read(fd_t fd, void* buffer, uint64_t count)
{
    return _SyscallRead(fd, buffer, count);
}

uint64_t write(fd_t fd, const void* buffer, uint64_t count)
{
    return _SyscallWrite(fd, buffer, count);
}

uint64_t writef(fd_t fd, const char* _RESTRICT format, ...)
{
    va_list args;
    va_start(args, format);
    uint64_t result = vwritef(fd, format, args);
    va_end(args);
    return result;
}

uint64_t vwritef(fd_t fd, const char* _RESTRICT format, va_list args)
{
    char buffer[MAX_PATH];
    uint64_t count = vsnprintf(buffer, MAX_PATH, format, args);
    return write(fd, buffer, count);
}

uint64_t seek(fd_t fd, int64_t offset, seek_origin_t origin)
{
    return _SyscallSeek(fd, offset, origin);
}

uint64_t chdir(const char* path)
{
    return _SyscallChdir(path);
}

uint64_t poll(pollfd_t* fds, uint64_t amount, clock_t timeout)
{
    return _SyscallPoll(fds, amount, timeout);
}

poll_event_t poll1(fd_t fd, poll_event_t requested, clock_t timeout)
{
    pollfd_t pollfd = {.fd = fd, .requested = requested};
    if (_SyscallPoll(&pollfd, 1, timeout) == ERR)
    {
        return POLL1_ERR;
    }
    return pollfd.occurred;
}

uint64_t stat(const char* path, stat_t* info)
{
    return _SyscallStat(path, info);
}

uint64_t ioctl(fd_t fd, uint64_t request, void* argp, uint64_t size)
{
    return _SyscallIoctl(fd, request, argp, size);
}

fd_t dup(fd_t oldFd)
{
    return _SyscallDup(oldFd);
}

fd_t dup2(fd_t oldFd, fd_t newFd)
{
    return _SyscallDup2(oldFd, newFd);
}

uint64_t mkdir(const char* path)
{
    fprintf(stderr, "mkdir not implemented");
    return ERR;
}
