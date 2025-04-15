#include "platform/platform.h"
#if _PLATFORM_HAS_SYSCALLS

#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"

pid_t spawn(const char** argv, const spawn_fd_t* fds)
{
    return _SyscallSpawn(argv, fds);
}

fd_t pid_open(pid_t pid, const char* file)
{
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "sys:/proc/%d/%s", pid, file);

    return open(path);
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

void* virtual_alloc(void* address, uint64_t length, prot_t prot)
{
    return _SyscallVirtualAlloc(address, length, prot);
}

uint64_t virtual_free(void* address, uint64_t length)
{
    return _SyscallVirtualFree(address, length);
}

uint64_t virtual_protect(void* address, uint64_t length, prot_t prot)
{
    return _SyscallVirtualProtect(address, length, prot);
}

uint64_t futex(atomic_uint64* addr, uint64_t val, futex_op_t op, nsec_t timeout)
{
    return _SyscallFutex(addr, val, op, timeout);
}

#endif
