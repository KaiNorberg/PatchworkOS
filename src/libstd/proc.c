#include "platform/platform.h"
#if _PLATFORM_HAS_SYSCALLS

#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"

pid_t spawn(const char** argv, const spawn_fd_t* fds)
{
    return _SyscallSpawn(argv, fds);
}

pid_t process_id(void)
{
    return _SyscallProcessId();
}

tid_t thread_id(void)
{
    return _SyscallThreadId();
}

nsec_t uptime(void)
{
    return _SyscallUptime();
}

void* mmap(fd_t fd, void* address, uint64_t length, prot_t prot)
{
    return _SyscallMmap(fd, address, length, prot);
}

uint64_t munmap(void* address, uint64_t length)
{
    return _SyscallMunmap(address, length);
}

uint64_t mprotect(void* address, uint64_t length, prot_t prot)
{
    return _SyscallMprotect(address, length, prot);
}

uint64_t futex(atomic_uint64* addr, uint64_t val, futex_op_t op, nsec_t timeout)
{
    return _SyscallFutex(addr, val, op, timeout);
}

#endif
