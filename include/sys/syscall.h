#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H 1

#include <stdint.h>
#include <sys/defs.h>
#include <sys/status.h>

/**
 * @brief Userspace System Calls.
 * @defgroup libstd_sys_math System Calls
 * @ingroup libstd
 *
 * @todo Write documentation for libstd_sys_math
 * 
 * @{
 */

/**
 * @brief System Call Numbers.
 * @enum syscall_number_t
 */
typedef enum
{
    SYS_EXITS,
    SYS_THREAD_EXIT,
    SYS_SPAWN,
    SYS_NANOSLEEP,
    SYS_ERRNO,
    SYS_GETPID,
    SYS_GETTID,
    SYS_UPTIME,
    SYS_EPOCH,
    SYS_OPEN,
    SYS_OPEN2,
    SYS_CLOSE,
    SYS_READ,
    SYS_WRITE,
    SYS_SEEK,
    SYS_IOCTL,
    SYS_POLL,
    SYS_STAT,
    SYS_MMAP,
    SYS_MUNMAP,
    SYS_MPROTECT,
    SYS_GETDENTS,
    SYS_THREAD_CREATE,
    SYS_YIELD,
    SYS_DUP,
    SYS_DUP2,
    SYS_FUTEX,
    SYS_REMOVE,
    SYS_LINK,
    SYS_SHARE,
    SYS_CLAIM,
    SYS_BIND,
    SYS_OPENAT,
    SYS_NOTIFY,
    SYS_NOTED,
    SYS_READLINK,
    SYS_SYMLINK,
    SYS_MOUNT,
    SYS_UNMOUNT,
    SYS_ARCH_PRCTL,
    SYS_IORING_SETUP,
    SYS_IORING_TEARDOWN,
    SYS_IORING_ENTER,
    SYS_TOTAL_AMOUNT
} syscall_number_t;

typedef struct
{
    union
    {
        uint64_t rax;
        status_t status;
    };
    union
    {
        uint64_t rdx;
        uint64_t result;
    };
} syscall_result_t;

#define SYSCALL_RESULT(_result, _status) \
    ((syscall_result_t){.result = (uint64_t)(_result), .status = (_status)})

static inline status_t syscall0(syscall_number_t number, uint64_t* result)
{
    syscall_result_t res;
    ASM("syscall"
        : "=a"(res.rax), "=d"(res.rdx)
        : "a"(number)
        : "rcx", "r11", "memory");
    if (result != NULL)
    {
        *result = res.result;
    }
    return res.status;
}

static inline status_t syscall1(syscall_number_t number, uint64_t arg1, uint64_t* result)
{
    syscall_result_t res;
    ASM("syscall"
        : "=a"(res.rax), "=d"(res.rdx)
        : "a"(number), "D"(arg1)
        : "rcx", "r11", "memory");
    if (result != NULL)
    {
        *result = res.result;
    }
    return res.status;
}

static inline status_t syscall2(syscall_number_t number, uint64_t arg1, uint64_t arg2, uint64_t* result)
{
    syscall_result_t res;
    ASM("syscall"
        : "=a"(res.rax), "=d"(res.rdx)
        : "a"(number), "D"(arg1), "S"(arg2)
        : "rcx", "r11", "memory");
    if (result != NULL)
    {
        *result = res.result;
    }
    return res.status;
}

static inline status_t syscall3(syscall_number_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t* result)
{
    syscall_result_t res;
    res.rdx = arg3;
    ASM("syscall"
        : "=a"(res.rax), "+d"(res.rdx)
        : "a"(number), "D"(arg1), "S"(arg2)
        : "rcx", "r11", "memory");
    if (result != NULL)
    {
        *result = res.result;
    }
    return res.status;
}

static inline status_t syscall4(syscall_number_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,
    uint64_t* result)
{
    syscall_result_t res;
    res.rdx = arg3;
    register uint64_t r10 asm("r10") = arg4;
    ASM("syscall"
        : "=a"(res.rax), "+d"(res.rdx)
        : "a"(number), "D"(arg1), "S"(arg2), "r"(r10)
        : "rcx", "r11", "memory");
    if (result != NULL)
    {
        *result = res.result;
    }
    return res.status;
}

static inline status_t syscall5(syscall_number_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,
    uint64_t arg5, uint64_t* result)
{
    syscall_result_t res;
    res.rdx = arg3;
    register uint64_t r10 asm("r10") = arg4;
    register uint64_t r8 asm("r8") = arg5;
    ASM("syscall"
        : "=a"(res.rax), "+d"(res.rdx)
        : "a"(number), "D"(arg1), "S"(arg2), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory");
    if (result != NULL)
    {
        *result = res.result;
    }
    return res.status;
}

static inline status_t syscall6(syscall_number_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,
    uint64_t arg5, uint64_t arg6, uint64_t* result)
{
    syscall_result_t res;
    res.rdx = arg3;
    register uint64_t r10 asm("r10") = arg4;
    register uint64_t r8 asm("r8") = arg5;
    register uint64_t r9 asm("r9") = arg6;
    ASM("syscall"
        : "=a"(res.rax), "+d"(res.rdx)
        : "a"(number), "D"(arg1), "S"(arg2), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory");
    if (result != NULL)
    {
        *result = res.result;
    }
    return res.status;
}

/** @} */

#endif