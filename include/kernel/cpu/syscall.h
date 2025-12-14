#pragma once

#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/stack_pointer.h>
#include <kernel/mem/space.h>

/**
 * @brief System Call Interface.
 * @defgroup kernel_cpu_syscall Syscall
 * @ingroup kernel_cpu
 *
 * System calls provide a controlled interface for user-space applications to request services from the kernel, such as
 * file operations, process management, and inter-process communication.
 *
 * ## SYSCALL Instruction
 *
 * Historically, system calls were invoked using software interrupts (usually `int 0x80`), which are relatively slow due
 * to overhead from interrupt handling.
 *
 * Instead, we use the modern `SYSCALL` instruction, which allows for a faster transition from user mode to kernel mode,
 * but it is a little more complex to set up.
 *
 * ## Stack switching
 *
 * When a syscall is invoked, the CPU will not automatically switch stacks. We need to manually switch them. To do this,
 * we use the `MSR_KERNEL_GS_BASE` MSR to store a pointer to the `syscall_ctx_t` structure for the current thread.
 *
 * When `swapgs` is called, the `GS` segment register will be swapped with the value in `MSR_KERNEL_GS_BASE`, allowing
 * us to access the syscall context for the current thread.
 *
 * We now cache the user stack pointer in the syscall context, and load the `syscallRsp` stack pointer. We then
 * immediately push the user stack pointer onto the new stack and call `swapgs` again to restore the original `GS`
 * value. Finally, we can enable interrupts and call the main C syscall handler.
 *
 * @note Swapping the `GS` register back before enabling interrupts is important, as it ensures user space can modify
 * its own `GS` base without affecting the kernel and that preemptions do not overwrite the `MSR_KERNEL_GS_BASE` MSR or
 * the `GS` register.
 *
 * ## Calling Convention
 *
 * The syscall calling convention mostly follows the standard System V ABI for x86_64 architecture, with the exception
 * of the argument registers, and the use of the `RAX` register for the syscall number.
 *
 * Arguments are passed to syscalls using the `RDI`, `RSI`, `RDX`, `R10`, `R8`, and `R9` registers, in that order. The
 * syscall number is passed in the `RAX` register.
 *
 * After the registers are setup the `syscall` instruction should be called, with the return value is being placed in
 * the `RAX` register.
 *
 * If the return value is `ERR` for a system call that returns an integer or `NULL` for a system call that returns a
 * pointer. Then the `SYS_ERRNO` syscall can be used to retrieve the associated error code.
 *
 * @see [SYSCALL instruction](https://www.felixcloutier.com/x86/syscall)
 * @see [SYSRET instruction](https://www.felixcloutier.com/x86/sysret)
 *
 * @{
 */

/**
 * @brief System Call Numbers.
 * @enum syscall_number_t
 */
typedef enum
{
    SYS_PROCESS_EXIT = 0,
    SYS_THREAD_EXIT = 1,
    SYS_SPAWN = 2,
    SYS_NANOSLEEP = 3,
    SYS_ERRNO = 4,
    SYS_GETPID = 5,
    SYS_GETTID = 6,
    SYS_UPTIME = 7,
    SYS_UNIX_EPOCH = 8,
    SYS_OPEN = 9,
    SYS_OPEN2 = 10,
    SYS_CLOSE = 11,
    SYS_READ = 12,
    SYS_WRITE = 13,
    SYS_SEEK = 14,
    SYS_IOCTL = 15,
    SYS_CHDIR = 16,
    SYS_POLL = 17,
    SYS_STAT = 18,
    SYS_MMAP = 19,
    SYS_MUNMAP = 20,
    SYS_MPROTECT = 21,
    SYS_GETDENTS = 22,
    SYS_THREAD_CREATE = 23,
    SYS_YIELD = 24,
    SYS_DUP = 25,
    SYS_DUP2 = 26,
    SYS_FUTEX = 27,
    SYS_REMOVE = 28,
    SYS_LINK = 29,
    SYS_SHARE = 30,
    SYS_CLAIM = 31,
    SYS_BIND = 32,
    SYS_OPENAT = 33,
    SYS_NOTIFY = 34,
    SYS_NOTED = 35,
    SYS_TOTAL_AMOUNT = 36
} syscall_number_t;

/**
 * @brief Per thread syscall context.
 * @struct syscall_ctx_t
 */
typedef struct
{
    uintptr_t syscallRsp; ///< The stack pointer to use when handling syscalls.
    uintptr_t userRsp;    ///< Used to avoid clobbering registers when switching stacks.
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
 * Uses the `.syscall_table` linker section to store the syscall descriptor.
 *
 * @param num The syscall number, must be unique, check `syscall_number_t`.
 * @param returnType The return type of the syscall handler, must be `uint64_t` compatible.
 * @param ... The arguments of the syscall handler, can be no more than 6 arguments (Such that we only use registers to
 * pass them).
 */
#define SYSCALL_DEFINE(num, returnType, ...) \
    returnType syscall_handler_##num(__VA_ARGS__); \
    const syscall_descriptor_t __syscall_##num __attribute__((used, section(".syscall_table"))) = { \
        .number = (num), \
        .handler = (void*)syscall_handler_##num, \
    }; \
    returnType syscall_handler_##num(__VA_ARGS__)

/**
 * @brief Initialize a syscall context.
 *
 * @param ctx The syscall context to initialize.
 * @param syscallStack The syscall stack of the thread.
 */
void syscall_ctx_init(syscall_ctx_t* ctx, const stack_pointer_t* syscallStack);

/**
 * @brief Load the syscall context into the `MSR_KERNEL_GS_BASE` MSR.
 *
 * @param ctx The syscall context to load.
 */
void syscall_ctx_load(syscall_ctx_t* ctx);

/**
 * @brief Sort the syscall table and verify that all syscalls are present.
 */
void syscall_table_init(void);

/**
 * @brief Initialize syscalls on the current CPU.
 *
 * Will modify four MSRs:
 * - `MSR_EFER`: Used to enable the `SYSCALL` instruction.
 * - `MSR_STAR`: Used to set the code segments for kernel and user mode.
 * - `MSR_LSTAR`: Used to set the entry point for the `SYSCALL` instruction.
 * - `MSR_SYSCALL_FLAG_MASK`: Specifies which rflags to clear when entering kernel mode.
 */
void syscalls_cpu_init(void);

/**
 * @brief Main C syscall handler.
 *
 * This is called from the assembly `syscall_entry()` function.
 *
 * Since notes can only be handled when in user space, this function will, if there are notes pending, provide a fake interrupt context to handle the note as if a interrupt had occurred at the exact same time as the system call began.
 * 
 * @param frame The interrupt frame containing the CPU state at the time of the syscall.
 */
void syscall_handler(interrupt_frame_t* frame);

/**
 * @brief Assembly entry point for syscalls.
 *
 * The logic for saving/restoring registers and switching stacks is done here before calling `syscall_handler()`.
 */
extern void syscall_entry(void);

/** @} */
