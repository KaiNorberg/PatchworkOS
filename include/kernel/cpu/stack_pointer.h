#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct thread thread_t;

/**
 * @brief Helpers for managing stacks.
 * @defgroup kernel_cpu_stack_pointer Stack Pointer
 * @ingroup kernel_cpu
 *
 * @{
 */

/**
 * @brief The amount of guard pages to use for stacks.
 */
#define STACK_POINTER_GUARD_PAGES 1

/**
 * @brief Structure to define a stack in memory.
 *
 * A stack is defined as a region of page aligned memory that includes a guard page to catch stack overflows. The region
 * of memory starts unmapped and when a page fault occurs within the stack region a new page is mapped to the faulting
 * address.
 *
 * The guard page is always the page just below the bottom of the stack, and is never mapped. If a thread tries to
 * access the guard page a page fault will occur, which can be used to detect stack overflows.
 *
 * The exception to the above is when using `stack_pointer_init_buffer()` to create a stack from a provided buffer, in
 * this case there is no guard page and the entire region starts mapped.
 *
 * Note that in x86 the stack grows downwards, so we start at the top and grow towards the bottom and that the "push"
 * operation moves the stack pointer first then writes to the location of the stack pointer, this means we actually set
 * the inital stack pointer to be the address just outside the top of the stack.
 */
typedef struct
{
    uintptr_t top;           ///< The top of the stack, this address is not inclusive.
    uintptr_t bottom;        ///< The bottom of the stack, this address is inclusive.
    uintptr_t guardTop;      ///< The top of the guard page, this address is inclusive.
    uintptr_t guardBottom;   ///< The bottom of the guard page, this address is inclusive.
    uintptr_t lastPageFault; ///< The last page that caused a page fault, used to prevent infinite loops.
} stack_pointer_t;

/**
 * @brief Initializes a stack pointer structure, does not allocate or map any memory.
 *
 * This is used to create stacks that grow dynamically, for example the kernel and user stacks of a thread.
 *
 * @param stack The stack pointer structure to initialize.
 * @param maxAddress The maximum address the stack will start at, must be page aligned.
 * @param maxPages The maximum amount of pages the stack can grow to, must not be 0.
 * @return `true` if the provided arguments are valid, `false` otherwise.
 */
bool stack_pointer_init(stack_pointer_t* stack, uintptr_t maxAddress, uint64_t maxPages);

/**
 * @brief Initializes a stack pointer structure using a provided buffer, does not allocate or map any memory.
 *
 * This is used to create stacks that do not grow dynamically, for example the exception and double fault stacks of a
 * CPU.
 *
 * These stacks will not have a guard page.
 *
 * Will not take ownership of the provided buffer.
 *
 * @param stack The stack pointer structure to initialize.
 * @param buffer The buffer to use for the stack, must be page aligned.
 * @param pages The amount of pages the stack will use, must not be 0.
 * @return `true` if the provided arguments are valid, `false` otherwise.
 */
bool stack_pointer_init_buffer(stack_pointer_t* stack, void* buffer, uint64_t pages);

/**
 * @brief Deinitializes a stack pointer structure and unmaps any mapped memory.
 *
 * @param stack The stack pointer structure to deinitialize.
 * @param thread The thread that owns the stack, used to get the address space to unmap the memory from.
 */
void stack_pointer_deinit(stack_pointer_t* stack, thread_t* thread);

/**
 * @brief Deinitializes a stack pointer structure that was initialized using `stack_pointer_init_buffer()`.
 *
 * This will not unmap any memory as the memory was provided by the caller.
 *
 * @param stack The stack pointer structure to deinitialize.
 */
void stack_pointer_deinit_buffer(stack_pointer_t* stack);

/**
 * @brief Check if an region is within the stack.
 *
 * @param stack The stack pointer structure.
 * @param addr The starting address of the region.
 * @param length The length of the region, in bytes.
 * @return `true` if the region is within the stack, `false` otherwise.
 */
bool stack_pointer_is_in_stack(stack_pointer_t* stack, uintptr_t addr, uint64_t length);

/**
 * @brief Check if an region overlaps the guard.
 *
 * @param stack The stack pointer structure.
 * @param addr The starting address of the region.
 * @param length The length of the region, in bytes.
 * @return `true` if the region overlaps the guard, `false` otherwise.
 */
bool stack_pointer_overlaps_guard(stack_pointer_t* stack, uintptr_t addr, uint64_t length);

/**
 * @brief Poke the stack to ensure that a page fault will occur at the given offset.
 *
 * Used to avoid recursive page faults when handling stack overflows. For example, to grow the stack we need the virtual
 * memory manager, but what if we run out of stack while in the VMM? We use this function to make sure the VMM will
 * never run out of stack, to avoid this situation.
 *
 * @param offset The max offset from the current stack pointer to poke, in bytes. Will poke every PAGE_SIZE bytes up to
 * the offset.
 */
void stack_pointer_poke(uint64_t offset);

/** @} */
