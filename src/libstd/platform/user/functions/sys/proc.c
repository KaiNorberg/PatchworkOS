#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

pid_t spawn(const char** argv, const spawn_fd_t* fds)
{
    pid_t result = _SyscallSpawn(argv, fds);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

pid_t getpid(void)
{
    pid_t result = _SyscallGetpid();
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

tid_t gettid(void)
{
    tid_t result = _SyscallGettid();
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

clock_t uptime(void)
{
    clock_t result = _SyscallUptime();
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

void* mmap(fd_t fd, void* address, uint64_t length, prot_t prot)
{
    void* result = _SyscallMmap(fd, address, length, prot);
    if (result == NULL)
    {
        errno = _SyscallLastError();
    }
    return result;
}

uint64_t munmap(void* address, uint64_t length)
{
    uint64_t result = _SyscallMunmap(address, length);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

uint64_t mprotect(void* address, uint64_t length, prot_t prot)
{
    uint64_t result = _SyscallMprotect(address, length, prot);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

uint64_t futex(atomic_uint64* addr, uint64_t val, futex_op_t op, clock_t timeout)
{
    uint64_t result = _SyscallFutex(addr, val, op, timeout);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}

uint64_t sleep(clock_t timeout)
{
    uint64_t result = _SyscallSleep(timeout);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}
