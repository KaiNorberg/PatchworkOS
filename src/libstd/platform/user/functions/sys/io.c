#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

fd_t open(const char* path)
{
    fd_t fd = _SyscallOpen(path);
    if (fd == ERR)
    {
        errno = _SyscallLastError();
    }
    return fd;
}

fd_t openf(const char* _RESTRICT format, ...)
{
    char path[MAX_PATH];
    
    va_list args;
    va_start(args, format);
    vsnprintf(path, MAX_PATH, format, args);
    va_end(args);

    return open(path);
}

fd_t vopenf(const char* _RESTRICT format, va_list args)
{
    char path[MAX_PATH];
    vsnprintf(path, MAX_PATH, format, args);
    return open(path);
}

uint64_t open2(const char* path, fd_t fds[2])
{
    uint64_t result = _SyscallOpen2(path, fds);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

uint64_t close(fd_t fd)
{
    uint64_t result = _SyscallClose(fd);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

uint64_t read(fd_t fd, void* buffer, uint64_t count)
{
    uint64_t result = _SyscallRead(fd, buffer, count);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

uint64_t write(fd_t fd, const void* buffer, uint64_t count)
{
    uint64_t result = _SyscallWrite(fd, buffer, count);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
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
    uint64_t result = _SyscallSeek(fd, offset, origin);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

uint64_t chdir(const char* path)
{
    uint64_t result = _SyscallChdir(path);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

uint64_t poll(pollfd_t* fds, uint64_t amount, clock_t timeout)
{
    uint64_t result = _SyscallPoll(fds, amount, timeout);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

poll_events_t poll1(fd_t fd, poll_events_t events, clock_t timeout)
{
    pollfd_t pollfd = {.fd = fd, .events = events};
    if (poll(&pollfd, 1, timeout) == ERR)
    {
        return POLL1_ERR;
    }
    return pollfd.revents;
}

uint64_t stat(const char* path, stat_t* info)
{
    uint64_t result = _SyscallStat(path, info);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

uint64_t ioctl(fd_t fd, uint64_t request, void* argp, uint64_t size)
{
    uint64_t result = _SyscallIoctl(fd, request, argp, size);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

fd_t dup(fd_t oldFd)
{
    uint64_t result = _SyscallDup(oldFd);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

fd_t dup2(fd_t oldFd, fd_t newFd)
{
    uint64_t result = _SyscallDup2(oldFd, newFd);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

uint64_t readdir(fd_t fd, stat_t* infos, uint64_t amount)
{
    uint64_t result = _SyscallReaddir(fd, infos, amount);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

allocdir_t* allocdir(fd_t fd)
{
    while (true)
    {
        uint64_t amount = readdir(fd, NULL, 0);
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
        if (readdir(fd, dirs->infos, amount) == ERR)
        {
            free(dirs);
            return NULL;
        }

        uint64_t newAmount = readdir(fd, NULL, 0);
        if (newAmount == ERR)
        {
            free(dirs);
            return NULL;
        }

        if (newAmount == amount)
        {
            return dirs;
        }
        else
        {
            free(dirs);
        }
    }
}

uint64_t mkdir(const char* path)
{
    fd_t fd = openf("%s?create&dir", path);
    if (fd == ERR)
    {
        return ERR;
    }
    close(fd);
    return 0;
}
