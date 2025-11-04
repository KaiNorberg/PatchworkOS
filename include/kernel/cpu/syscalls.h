#pragma once

#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/stack_pointer.h>
#include <kernel/cpu/syscalls.h>
#include <kernel/mem/space.h>

/**
 * @brief System Calls
 * @defgroup kernel_cpu_syscalls Syscalls
 * @ingroup kernel_cpu
 *
 * System calls provide a controlled interface for user-space applications to request services from the kernel, such as
 * file operations, process management, and inter-process communication.
 *
 * @see [SYSCALL instruction](https://www.felixcloutier.com/x86/syscall)
 * @see [SYSRET instruction](https://www.felixcloutier.com/x86/sysret)
 *
 * @{
 */

#define SYS_PROCESS_EXIT 0
#define SYS_THREAD_EXIT 1
#define SYS_SPAWN 2
#define SYS_NANOSLEEP 3
#define SYS_ERRNO 4
#define SYS_GETPID 5
#define SYS_GETTID 6
#define SYS_UPTIME 7
#define SYS_UNIX_EPOCH 8
#define SYS_OPEN 9
#define SYS_OPEN2 10
#define SYS_CLOSE 11
#define SYS_READ 12
#define SYS_WRITE 13
#define SYS_SEEK 14
#define SYS_IOCTL 15
#define SYS_CHDIR 16
#define SYS_POLL 17
#define SYS_STAT 18
#define SYS_MMAP 19
#define SYS_MUNMAP 20
#define SYS_MPROTECT 21
#define SYS_GETDENTS 22
#define SYS_THREAD_CREATE 23
#define SYS_YIELD 24
#define SYS_DUP 25
#define SYS_DUP2 26
#define SYS_FUTEX 27
#define SYS_REMOVE 28
#define SYS_LINK 29
#define SYS_SHARE 30
#define SYS_CLAIM 31
#define SYS_BIND 32
#define SYS_OPENAT 33

#define SYS_TOTAL_AMOUNT 34

/**
 * @brief Per thread syscall context.
 * @struct syscall_ctx_t
 */
typedef struct
{
    uintptr_t kernelRsp;
    uintptr_t userRsp;
} syscall_ctx_t;

/**
 * @brief A syscall descriptor.
 * @struct syscall_descriptor_t
 *
 * Describes a single syscall, its number and the function pointer to the handler.
 */
typedef struct
{
    uint32_t number;
    uint64_t (*handler)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
} syscall_descriptor_t;

/**
 * @brief Linker defined start of the syscall table.
 */
extern syscall_descriptor_t _syscallTableStart[];

/**
 * @brief Linker defined end of the syscall table.
 */
extern syscall_descriptor_t _syscallTableEnd[];

/**
 * @brief Macro to define a syscall.
 *
 * Uses the linker section to define a system call in the syscall table.
 *
 * @param num The syscall number, must be unique, check `include/kernel/syscalls.h` for existing numbers.
 * @param returnType The return type of the syscall handler.
 * @param ... The arguments of the syscall handler, can be no more then 6 arguments. To avoid using the stack to pass
 * arguments to a syscall.
 */
#define SYSCALL_DEFINE(num, returnType, ...) \
    returnType syscall_handler_##num(__VA_ARGS__); \
    const syscall_descriptor_t __syscall_##num __attribute__((used, section(".syscall_table"))) = { \
        .number = (num), \
        .handler = (void*)syscall_handler_##num, \
    }; \
    returnType syscall_handler_##num(__VA_ARGS__)

/**
 * @brief Assembly entry point for syscalls.
 *
 * Responsible for switching stacks.
 */
extern void syscall_entry(void);

/**
 * @brief Initialize a per-thread syscall context.
 *
 * @param ctx The syscall context to initialize.
 * @param kernelStack The kernel stack of the thread.
 */
void syscall_ctx_init(syscall_ctx_t* ctx, stack_pointer_t* kernelStack);

/**
 * @brief Load a syscall context into the CPU.
 */
void syscall_ctx_load(syscall_ctx_t* ctx);

/**
 * @brief Initialize the syscall table.
 *
 * This will sort the syscall table and verify that all syscalls are present.
 */
void syscall_table_init(void);

/**
 * @brief Initalize syscalls on the current CPU.
 *
 * This will setup the MSR registers required for syscalls.
 */
void syscalls_cpu_init(void);

/**
 * @brief C syscall handler.
 *
 * This is called from the assembly `syscall_entry()` function.
 *
 * @param rdi First argument.
 * @param rsi Second argument.
 * @param rdx Third argument.
 * @param rcx Fourth argument.
 * @param r8 Fifth argument.
 * @param r9 Sixth argument.
 * @param number The syscall number.
 * @return The return value of the syscall.
 */
uint64_t syscall_handler(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t rcx, uint64_t r8, uint64_t r9,
    uint64_t number);

/** @} */
