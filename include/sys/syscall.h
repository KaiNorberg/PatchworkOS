#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H 1

#include <stdint.h>
#include <sys/defs.h>
#include <sys/status.h>

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief Userspace System Calls.
 * @defgroup libstd_sys_math System Calls
 * @ingroup libstd
 *
 * @{
 */

/**
 * @brief System Call Numbers.
 * @enum syscall_number_t
 */
typedef enum
{
    SYS_PROC_EXIT,
    SYS_PROC_CREATE,
    SYS_PROC_CURRENT,
    SYS_THRD_EXIT,
    SYS_THRD_CREATE,
    SYS_THRD_CURRENT,
    SYS_THRD_SLEEP,
    SYS_THRD_YIELD,
    SYS_CLOCK,
    SYS_TIME,
    SYS_UNMAP,
    SYS_PROTECT,
    SYS_FD_DUP,
    SYS_FS_BIND,
    SYS_FS_UNBIND,
    SYS_NOTE_SET,
    SYS_NOTE_DONE,
    SYS_ARCH_CTL,
    SYS_SYNC_CTL,
    SYS_IORING_SETUP,
    SYS_IORING_TEARDOWN,
    SYS_IORING_ENTER,
    SYS_TOTAL_AMOUNT
} syscall_number_t;

typedef struct
{
    union {
        uint64_t rax;
        status_t status;
    };
    union {
        uint64_t rdx;
        uint64_t result;
    };
} syscall_result_t;

#define SYSCALL_RESULT(_result, _status) ((syscall_result_t){.result = (uint64_t)(_result), .status = (_status)})

static inline status_t syscall0(syscall_number_t number, uint64_t* result)
{
    syscall_result_t res;
    ASM("syscall" : "=a"(res.rax), "=d"(res.rdx) : "a"(number) : "rcx", "r11", "memory");
    if (result != NULL)
    {
        *result = res.result;
    }
    return res.status;
}

static inline status_t syscall1(syscall_number_t number, uint64_t* result, uint64_t arg1)
{
    syscall_result_t res;
    ASM("syscall" : "=a"(res.rax), "=d"(res.rdx) : "a"(number), "D"(arg1) : "rcx", "r11", "memory");
    if (result != NULL)
    {
        *result = res.result;
    }
    return res.status;
}

static inline status_t syscall2(syscall_number_t number, uint64_t* result, uint64_t arg1, uint64_t arg2)
{
    syscall_result_t res;
    ASM("syscall" : "=a"(res.rax), "=d"(res.rdx) : "a"(number), "D"(arg1), "S"(arg2) : "rcx", "r11", "memory");
    if (result != NULL)
    {
        *result = res.result;
    }
    return res.status;
}

static inline status_t syscall3(syscall_number_t number, uint64_t* result, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    syscall_result_t res;
    res.rdx = arg3;
    ASM("syscall" : "=a"(res.rax), "+d"(res.rdx) : "a"(number), "D"(arg1), "S"(arg2) : "rcx", "r11", "memory");
    if (result != NULL)
    {
        *result = res.result;
    }
    return res.status;
}

static inline status_t syscall4(syscall_number_t number, uint64_t* result, uint64_t arg1, uint64_t arg2, uint64_t arg3,
    uint64_t arg4)
{
    syscall_result_t res;
    res.rdx = arg3;
    register uint64_t r10 asm("r10") = arg4;
    ASM("syscall" : "=a"(res.rax), "+d"(res.rdx) : "a"(number), "D"(arg1), "S"(arg2), "r"(r10) : "rcx", "r11",
        "memory");
    if (result != NULL)
    {
        *result = res.result;
    }
    return res.status;
}

static inline status_t syscall5(syscall_number_t number, uint64_t* result, uint64_t arg1, uint64_t arg2, uint64_t arg3,
    uint64_t arg4, uint64_t arg5)
{
    syscall_result_t res;
    res.rdx = arg3;
    register uint64_t r10 asm("r10") = arg4;
    register uint64_t r8 asm("r8") = arg5;
    ASM("syscall" : "=a"(res.rax), "+d"(res.rdx) : "a"(number), "D"(arg1), "S"(arg2), "r"(r10), "r"(r8) : "rcx", "r11",
        "memory");
    if (result != NULL)
    {
        *result = res.result;
    }
    return res.status;
}

static inline status_t syscall6(syscall_number_t number, uint64_t* result, uint64_t arg1, uint64_t arg2, uint64_t arg3,
    uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
    syscall_result_t res;
    res.rdx = arg3;
    register uint64_t r10 asm("r10") = arg4;
    register uint64_t r8 asm("r8") = arg5;
    register uint64_t r9 asm("r9") = arg6;
    ASM("syscall" : "=a"(res.rax), "+d"(res.rdx) : "a"(number), "D"(arg1), "S"(arg2), "r"(r10), "r"(r8),
        "r"(r9) : "rcx", "r11", "memory");
    if (result != NULL)
    {
        *result = res.result;
    }
    return res.status;
}

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
