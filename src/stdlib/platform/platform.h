#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <sys/io.h>
#include <sys/proc.h>

#ifdef __KERNEL__

#include "kernel/platform.h"
#define _PLATFORM_HAS_SYSCALLS 0
#define _PLATFORM_HAS_SSE 0
#define _PLATFORM_HAS_WIN 0

#else

#include "user/platform.h"
#define _PLATFORM_HAS_SYSCALLS 1
#define _PLATFORM_HAS_SSE 1
#define _PLATFORM_HAS_WIN 1

#endif

void _PlatformInit(void);

void* _PlatformPageAlloc(uint64_t amount);

int* _PlatformErrnoFunc(void);

int _PlatformVprintf(const char* _RESTRICT format, va_list args);

#if _PLATFORM_HAS_SYSCALLS

_NORETURN void _SyscallProcessExit(uint64_t status);

_NORETURN void _SyscallThreadExit(void);

pid_t _SyscallSpawn(const char** argv, const spawn_fd_t* fds);

uint64_t _SyscallSleep(nsec_t nanoseconds);

errno_t _SyscallError(void);

pid_t _SyscallGetpid(void);

tid_t _SyscallGettid(void);

nsec_t _SyscallUptime(void);

time_t _SyscallTime(void);

fd_t _SyscallOpen(const char* path);

uint64_t _SyscallOpen2(const char* path, fd_t fds[2]);

uint64_t _SyscallClose(fd_t fd);

uint64_t _SyscallRead(fd_t fd, void* buffer, uint64_t count);

uint64_t _SyscallWrite(fd_t fd, const void* buffer, uint64_t count);

uint64_t _SyscallSeek(fd_t fd, int64_t offset, seek_origin_t origin);

uint64_t _SyscallIoctl(fd_t fd, uint64_t request, void* argp, uint64_t size);

uint64_t _SyscallChdir(const char* path);

uint64_t _SyscallPoll(pollfd_t* fds, uint64_t amount, nsec_t timeout);

uint64_t _SyscallStat(const char* path, stat_t* info);

void* _SyscallValloc(void* address, uint64_t length, prot_t prot);

uint64_t _SyscallVfree(void* address, uint64_t length);

uint64_t _SyscallVprotect(void* address, uint64_t length, prot_t prot);

uint64_t _SyscallFlush(fd_t fd, const pixel_t* buffer, uint64_t size, const rect_t* rect);

uint64_t _SyscallListdir(const char* path, dir_entry_t* entries, uint64_t amount);

tid_t _SyscallSplit(void* entry, uint64_t argc, ...);

void _SyscallYield(void);

fd_t _SyscallOpenas(fd_t target, const char* path);

uint64_t _SyscallOpen2as(const char* path, fd_t fd[2]);

fd_t _SyscallDup(fd_t oldFd);

fd_t _SyscallDup2(fd_t oldFd, fd_t newFd);

#endif