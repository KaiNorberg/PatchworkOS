#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

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

allocdir_t* allocdir(fd_t fd)
{
    uint64_t amount = _SyscallReaddir(fd, NULL, 0);
    if (amount == ERR)
    {
        return NULL;
    }
    allocdir_t* dirs = malloc(sizeof(allocdir_t) + sizeof(stat_t) * amount);
    if (dirs == NULL)
    {
        return NULL;
    }

    dirs->amount = amount;
    if (_SyscallReaddir(fd, dirs->infos, amount) == ERR)
    {
        free(dirs);
        return NULL;
    }

    return dirs;
}

uint64_t readdir(fd_t fd, stat_t* infos, uint64_t amount)
{
    return _SyscallReaddir(fd, infos, amount);
}

uint64_t mkdir(const char* path)
{
    fd_t fd = openf("%s?create&directory", path);
    if (fd == ERR)
    {
        return ERR;
    }
    close(fd);
    return 0;
}
