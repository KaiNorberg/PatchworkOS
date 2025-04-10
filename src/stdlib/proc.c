#include "platform/platform.h"
#if _PLATFORM_HAS_SCHEDULING

#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "platform/platform.h"

nsec_t uptime(void)
{
    return _PlatformUptime();
}

uint64_t sleep(nsec_t nanoseconds)
{
    return _PlatformSleep(nanoseconds);
}

// argv[0] = executable
pid_t spawn(const char** argv, const spawn_fd_t* fds)
{
    return _PlatformSpawn(argv, fds);
}

pid_t getpid(void)
{
    return _PlatformGetpid();
}

tid_t gettid(void)
{
    return _PlatformGettid();
}

fd_t procfd(pid_t pid, const char* file)
{
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "sys:/proc/%d/%s", pid, file);

    return open(path);
}

__attribute__((naked)) tid_t split(void* entry, uint64_t argc, ...)
{
    // Pretend you dont se this, we have to do this becouse of the variadic arguments.
    asm volatile("jmp _PlatformSplit");
}

_NORETURN void thread_exit(void)
{
    _PlatformThreadExit();
}

void yield(void)
{
    _PlatformYield();
}

void* mmap(fd_t fd, void* address, uint64_t length, prot_t prot)
{
    return _PlatformMmap(fd, address, length, prot);
}

uint64_t munmap(void* address, uint64_t length)
{
    return _PlatformMunmap(address, length);
}

uint64_t mprotect(void* address, uint64_t length, prot_t prot)
{
    return _PlatformMprotect(address, length, prot);
}

#endif
