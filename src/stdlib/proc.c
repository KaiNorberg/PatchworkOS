#include "platform/platform.h"
#if _PLATFORM_HAS_SYSCALLS

#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "platform/platform.h"

nsec_t uptime(void)
{
    return _SyscallUptime();
}

uint64_t sleep(nsec_t nanoseconds)
{
    return _SyscallSleep(nanoseconds);
}

// argv[0] = executable
pid_t spawn(const char** argv, const spawn_fd_t* fds)
{
    return _SyscallSpawn(argv, fds);
}

pid_t getpid(void)
{
    return _SyscallGetpid();
}

tid_t gettid(void)
{
    return _SyscallGettid();
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
    asm volatile("jmp _SyscallSplit");
}

void yield(void)
{
    _SyscallYield();
}

void* valloc(void* address, uint64_t length, prot_t prot)
{
    return _SyscallValloc(address, length, prot);
}

uint64_t vfree(void* address, uint64_t length)
{
    return _SyscallVfree(address, length);
}

uint64_t vprotect(void* address, uint64_t length, prot_t prot)
{
    return _SyscallVprotect(address, length, prot);
}

#endif
