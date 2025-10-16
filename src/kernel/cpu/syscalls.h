#pragma once

#include "cpu/stack_pointer.h"
#include "mem/space.h"
#include "trap.h"

#include <kernel/syscalls.h>

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

/**
 * @brief Per thread syscall context.
 * @struct syscall_ctx_t
 */
typedef struct
{
    uintptr_t kernelRsp;
    uintptr_t userRsp;
    bool inSyscall;
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
    void* handler;
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
uint64_t syscall_handler(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t rcx, uint64_t r8, uint64_t r9, uint64_t number);


/**
 * @brief Validate a pointer before using it in a syscall.
 *
 * Will check that the location pointed to by `pointer` for the provided `length` is in the lower half of the
 * address space. Could still be unmapped memory.
 *
 * @param pointer The pointer to validate.
 * @param length The length of the memory region to validate.
 * @return true if the pointer is valid, false otherwise.
 */
bool syscall_is_pointer_valid(const void* pointer, uint64_t length);

/**
 * @brief Validate a buffer before using it in a syscall.
 *
 * Will check that the location pointed to by `pointer` for the provided `length` is in the lower half of the
 * address space and is mapped in the provided address space.
 *
 * @param space The address space to check the mapping in.
 * @param pointer The pointer to validate.
 * @param length The length of the memory region to validate.
 * @return true if the buffer is valid, false otherwise.
 */
bool syscall_is_buffer_valid(space_t* space, const void* pointer, uint64_t length);

/**
 * @brief Validate a string before using it in a syscall.
 *
 * Will check that the string is in the lower half of the address space, is mapped in the provided address space and
 * that it takes up no more then `PAGE_SIZE` bytes.
 *
 * @param space The address space to check the mapping in.
 * @param string The string to validate.
 * @return true if the string is valid, false otherwise.
 */
bool syscall_is_string_valid(space_t* space, const char* string);

/** @} */
