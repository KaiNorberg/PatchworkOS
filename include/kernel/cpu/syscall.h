#pragma once

#ifndef __ASSEMBLER__
#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/stack_pointer.h>
#endif

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
 * @brief The offset of the `syscallRsp` member in the `syscall_ctx_t` structure.
 *
 * Needed to access the syscall context from assembly code.
 */
#define SYSCALL_CTX_SYSCALL_RSP_OFFSET 0x0

/**
 * @brief The offset of the `userRsp` member in the `syscall_ctx_t` structure.
 *
 * Needed to access the syscall context from assembly code.
 */
#define SYSCALL_CTX_USER_RSP_OFFSET 0x8

#ifndef __ASSEMBLER__
/**
 * @brief System Call Numbers.
 * @enum syscall_number_t
 */
typedef enum
{
    SYS_PROCESS_EXIT,
    SYS_THREAD_EXIT,
    SYS_SPAWN,
    SYS_NANOSLEEP,
    SYS_ERRNO,
    SYS_GETPID,
    SYS_GETTID,
    SYS_UPTIME,
    SYS_UNIX_EPOCH,
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
    SYS_TOTAL_AMOUNT
} syscall_number_t;

/**
 * @brief Syscall flags.
 * @enum syscall_flags_t
 */
typedef enum
{
    SYSCALL_NORMAL = 0 << 0,
    /**
     * Forces a fake interrupt to be generated after the syscall completes. This is useful if a syscall does not wish to
     * return to where it was called from.
     *
     * Intended to be used by modifying the interrupt frame stored in the syscall context and setting this flag. As an
     * example, consider the `SYS_NOTED` syscall.
     */
    SYSCALL_FORCE_FAKE_INTERRUPT = 1 << 0,
} syscall_flags_t;

/**
 * @brief Per thread syscall context.
 * @struct syscall_ctx_t
 */
typedef struct
{
    uintptr_t syscallRsp;     ///< The stack pointer to use when handling syscalls.
    uintptr_t userRsp;        ///< Used to avoid clobbering registers when switching stacks.
    interrupt_frame_t* frame; ///< If a fake interrupt is generated, this is the interrupt frame to return to.
    syscall_flags_t flags;    ///< Flags for the current syscall.
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
 * Uses the `._syscall_table` linker section to store the syscall descriptor.
 *
 * @param num The syscall number, must be unique, check `syscall_number_t`.
 * @param returnType The return type of the syscall handler, must be `uint64_t` compatible.
 * @param ... The arguments of the syscall handler, can be no more than 6 arguments (Such that we only use registers to
 * pass them).
 */
#define SYSCALL_DEFINE(num, returnType, ...) \
    returnType syscall_handler_##num(__VA_ARGS__); \
    const syscall_descriptor_t __syscall_##num __attribute__((used, section("._syscall_table"))) = { \
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
 * @brief Main C syscall handler.
 *
 * This is called from the assembly `syscall_entry()` function.
 *
 * Since notes can only be handled when in user space, this function will, if there are notes pending, provide a fake
 * interrupt context to handle the note as if a interrupt had occurred at the exact same time as the system call began.
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

#endif

/** @} */
