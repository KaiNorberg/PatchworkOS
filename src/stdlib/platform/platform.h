#pragma once

#include <stdint.h>
#include <sys/io.h>
#include <sys/proc.h>

#ifdef __KERNEL__

#include "kernel/platform.h"
#define _PLATFORM_HAS_FILE_IO 0
#define _PLATFORM_HAS_SCHEDULING 0
#define _PLATFORM_HAS_SSE 0
#define _PLATFORM_HAS_WIN 0

#else

#include "user/platform.h"
#define _PLATFORM_HAS_FILE_IO 1
#define _PLATFORM_HAS_SCHEDULING 1
#define _PLATFORM_HAS_SSE 1
#define _PLATFORM_HAS_WIN 1

#endif

void _PlatformInit(void);

void* _PlatformPageAlloc(uint64_t amount);

#if _PLATFORM_HAS_FILE_IO

uint64_t _PlatformListdir(const char* path, dir_entry_t* entries, uint64_t amount);

fd_t _PlatformOpen(const char* path);

uint64_t _PlatformClose(fd_t fd);

uint64_t _PlatformRead(fd_t fd, void* buffer, uint64_t count);

uint64_t _PlatformWrite(fd_t fd, const void* buffer, uint64_t count);

uint64_t _PlatformSeek(fd_t fd, int64_t offset, seek_origin_t origin);

uint64_t _PlatformRealpath(char* out, const char* path);

uint64_t _PlatformChdir(const char* path);

uint64_t _PlatformPoll(pollfd_t* fds, uint64_t amount, nsec_t timeout);

uint64_t _PlatformStat(const char* path, stat_t* stat);

uint64_t _PlatformIoctl(fd_t fd, uint64_t request, void* argp, uint64_t size);

uint64_t _PlatformFlush(fd_t fd, const pixel_t* buffer, uint64_t size, const rect_t* rect);

uint64_t _PlatformPipe(pipefd_t* pipefd);

#endif

#if _PLATFORM_HAS_SCHEDULING

nsec_t _PlatformUptime(void);

uint64_t _PlatformSleep(nsec_t nanoseconds);

pid_t _PlatformSpawn(const char** argv, const spawn_fd_t* fds);

pid_t _PlatformGetpid(void);

tid_t _PlatformGettid(void);

tid_t _PlatformSplit(void* entry, uint64_t argc, ...);

_NORETURN void _PlatformThreadExit(void);

void _PlatformYield(void);

void* _PlatformMmap(fd_t fd, void* address, uint64_t length, prot_t prot);

uint64_t _PlatformMunmap(void* address, uint64_t length);

uint64_t _PlatformMprotect(void* address, uint64_t length, prot_t prot);

_NORETURN void _PlatformExit(int status);

#endif