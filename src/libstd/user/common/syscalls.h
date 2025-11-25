#pragma once

#include <kernel/cpu/syscalls.h>

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <time.h>

#define _SYSCALL0(retType, num) \
    ({ \
        register retType ret asm("rax"); \
        asm volatile("syscall\n" : "=a"(ret) : "a"(num) : "rcx", "r11", "memory"); \
        ret; \
    })

#define _SYSCALL1(retType, num, type1, arg1) \
    ({ \
        register retType ret asm("rax"); \
        register type1 _a1 asm("rdi") = (arg1); \
        asm volatile("syscall\n" : "=a"(ret) : "a"(num), "r"(_a1) : "rcx", "r11", "memory"); \
        ret; \
    })

#define _SYSCALL2(retType, num, type1, arg1, type2, arg2) \
    ({ \
        register retType ret asm("rax"); \
        register type1 _a1 asm("rdi") = (arg1); \
        register type2 _a2 asm("rsi") = (arg2); \
        asm volatile("syscall\n" : "=a"(ret) : "a"(num), "r"(_a1), "r"(_a2) : "rcx", "r11", "memory"); \
        ret; \
    })

#define _SYSCALL3(retType, num, type1, arg1, type2, arg2, type3, arg3) \
    ({ \
        register retType ret asm("rax"); \
        register type1 _a1 asm("rdi") = (arg1); \
        register type2 _a2 asm("rsi") = (arg2); \
        register type3 _a3 asm("rdx") = (arg3); \
        asm volatile("syscall\n" : "=a"(ret) : "a"(num), "r"(_a1), "r"(_a2), "r"(_a3) : "rcx", "r11", "memory"); \
        ret; \
    })

#define _SYSCALL4(retType, num, type1, arg1, type2, arg2, type3, arg3, type4, arg4) \
    ({ \
        register retType ret asm("rax"); \
        register type1 _a1 asm("rdi") = (arg1); \
        register type2 _a2 asm("rsi") = (arg2); \
        register type3 _a3 asm("rdx") = (arg3); \
        register type4 _a4 asm("r10") = (arg4); \
        asm volatile("syscall\n" \
            : "=a"(ret) \
            : "a"(num), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4) \
            : "rcx", "r11", "memory"); \
        ret; \
    })

#define _SYSCALL5(retType, num, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5) \
    ({ \
        register retType ret asm("rax"); \
        register type1 _a1 asm("rdi") = (arg1); \
        register type2 _a2 asm("rsi") = (arg2); \
        register type3 _a3 asm("rdx") = (arg3); \
        register type4 _a4 asm("r10") = (arg4); \
        register type5 _a5 asm("r8") = (arg5); \
        asm volatile("syscall\n" \
            : "=a"(ret) \
            : "a"(num), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4), "r"(_a5) \
            : "rcx", "r11", "memory"); \
        ret; \
    })

#define _SYSCALL6(retType, num, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5, type6, arg6) \
    ({ \
        register retType ret asm("rax"); \
        register type1 _a1 asm("rdi") = (arg1); \
        register type2 _a2 asm("rsi") = (arg2); \
        register type3 _a3 asm("rdx") = (arg3); \
        register type4 _a4 asm("r10") = (arg4); \
        register type5 _a5 asm("r8") = (arg5); \
        register type6 _a6 asm("r9") = (arg6); \
        asm volatile("syscall\n" \
            : "=a"(ret) \
            : "a"(num), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4), "r"(_a5), "r"(_a6) \
            : "rcx", "r11", "memory"); \
        ret; \
    })

_NORETURN static inline void _syscall_process_exit(uint64_t status)
{
    _SYSCALL1(uint64_t, SYS_PROCESS_EXIT, uint64_t, status);
    asm volatile("ud2");
    while (1)
        ;
}

_NORETURN static inline void _syscall_thread_exit(void)
{
    _SYSCALL0(uint64_t, SYS_THREAD_EXIT);
    asm volatile("ud2");
    while (1)
        ;
}

static inline pid_t _syscall_spawn(const char** argv, const spawn_fd_t* fds, const char* cwd, priority_t priority, spawn_flags_t flags)
{
    return _SYSCALL5(pid_t, SYS_SPAWN, const char**, argv, const spawn_fd_t*, fds, const char*, cwd, priority_t, priority, spawn_flags_t, flags);
}

static inline uint64_t _syscall_nanosleep(clock_t nanoseconds)
{
    return _SYSCALL1(uint64_t, SYS_NANOSLEEP, clock_t, nanoseconds);
}

static inline errno_t _syscall_errno(void)
{
    return _SYSCALL0(errno_t, SYS_ERRNO);
}

static inline pid_t _syscall_getpid(void)
{
    return _SYSCALL0(pid_t, SYS_GETPID);
}

static inline tid_t _syscall_gettid(void)
{
    return _SYSCALL0(tid_t, SYS_GETTID);
}

static inline clock_t _syscall_uptime(void)
{
    return _SYSCALL0(clock_t, SYS_UPTIME);
}

static inline time_t _syscall_unix_epoch(void)
{
    return _SYSCALL0(time_t, SYS_UNIX_EPOCH);
}

static inline fd_t _syscall_open(const char* path)
{
    return _SYSCALL1(fd_t, SYS_OPEN, const char*, path);
}

static inline uint64_t _syscall_open2(const char* path, fd_t fds[2])
{
    return _SYSCALL2(uint64_t, SYS_OPEN2, const char*, path, fd_t*, fds);
}

static inline uint64_t _syscall_close(fd_t fd)
{
    return _SYSCALL1(uint64_t, SYS_CLOSE, fd_t, fd);
}

static inline uint64_t _syscall_read(fd_t fd, void* buffer, uint64_t count)
{
    return _SYSCALL3(uint64_t, SYS_READ, fd_t, fd, void*, buffer, uint64_t, count);
}

static inline uint64_t _syscall_write(fd_t fd, const void* buffer, uint64_t count)
{
    return _SYSCALL3(uint64_t, SYS_WRITE, fd_t, fd, const void*, buffer, uint64_t, count);
}

static inline uint64_t _syscall_seek(fd_t fd, int64_t offset, seek_origin_t origin)
{
    return _SYSCALL3(uint64_t, SYS_SEEK, fd_t, fd, int64_t, offset, seek_origin_t, origin);
}

static inline uint64_t _syscall_ioctl(fd_t fd, uint64_t request, void* argp, uint64_t size)
{
    return _SYSCALL4(uint64_t, SYS_IOCTL, fd_t, fd, uint64_t, request, void*, argp, uint64_t, size);
}

static inline uint64_t _syscall_chdir(const char* path)
{
    return _SYSCALL1(uint64_t, SYS_CHDIR, const char*, path);
}

static inline uint64_t _syscall_poll(pollfd_t* fds, uint64_t amount, clock_t timeout)
{
    return _SYSCALL3(uint64_t, SYS_POLL, pollfd_t*, fds, uint64_t, amount, clock_t, timeout);
}

static inline uint64_t _syscall_stat(const char* path, stat_t* info)
{
    return _SYSCALL2(uint64_t, SYS_STAT, const char*, path, stat_t*, info);
}

static inline void* _syscall_mmap(fd_t fd, void* address, uint64_t length, prot_t prot)
{
    return _SYSCALL4(void*, SYS_MMAP, fd_t, fd, void*, address, uint64_t, length, prot_t, prot);
}

static inline void* _syscall_munmap(void* address, uint64_t length)
{
    return _SYSCALL2(void*, SYS_MUNMAP, void*, address, uint64_t, length);
}

static inline void* _syscall_mprotect(void* address, uint64_t length, prot_t prot)
{
    return _SYSCALL3(void*, SYS_MPROTECT, void*, address, uint64_t, length, prot_t, prot);
}

static inline uint64_t _syscall_getdents(fd_t fd, dirent_t* buffer, uint64_t count)
{
    return _SYSCALL3(uint64_t, SYS_GETDENTS, fd_t, fd, dirent_t*, buffer, uint64_t, count);
}

static inline tid_t _syscall_thread_create(void* entry, void* arg)
{
    return _SYSCALL2(tid_t, SYS_THREAD_CREATE, void*, entry, void*, arg);
}

static inline void _syscall_yield(void)
{
    _SYSCALL0(uint64_t, SYS_YIELD);
}

static inline fd_t _syscall_dup(fd_t oldFd)
{
    return _SYSCALL1(fd_t, SYS_DUP, fd_t, oldFd);
}

static inline fd_t _syscall_dup2(fd_t oldFd, fd_t newFd)
{
    return _SYSCALL2(fd_t, SYS_DUP2, fd_t, oldFd, fd_t, newFd);
}

static inline uint64_t _syscall_futex(atomic_uint64_t* addr, uint64_t val, futex_op_t op, clock_t timeout)
{
    return _SYSCALL4(uint64_t, SYS_FUTEX, atomic_uint64_t*, addr, uint64_t, val, futex_op_t, op, clock_t, timeout);
}

static inline uint64_t _syscall_remove(const char* path)
{
    return _SYSCALL1(uint64_t, SYS_REMOVE, const char*, path);
}

static inline uint64_t _syscall_link(const char* oldPath, const char* newPath)
{
    return _SYSCALL2(uint64_t, SYS_LINK, const char*, oldPath, const char*, newPath);
}

static inline uint64_t _syscall_share(key_t* key, fd_t fd, clock_t timeout)
{
    return _SYSCALL3(uint64_t, SYS_SHARE, key_t*, key, fd_t, fd, clock_t, timeout);
}

static inline fd_t _syscall_claim(key_t* key)
{
    return _SYSCALL1(fd_t, SYS_CLAIM, key_t*, key);
}

static inline uint64_t _syscall_bind(fd_t source, const char* mountpoint, mount_flags_t flags)
{
    return _SYSCALL3(uint64_t, SYS_BIND, fd_t, source, const char*, mountpoint, mount_flags_t, flags);
}
