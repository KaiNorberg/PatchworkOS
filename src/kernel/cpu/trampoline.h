#pragma once

#include "smp.h"

/**
 * @brief Trampoline for CPU initialization
 * @defgroup kernel_cpu_trampoline Trampoline
 * @ingroup kernel_cpu
 *
 * The trampoline is a small piece of code used during the initialization of other CPUs in a multiprocessor system.
 *
 * The trampoline code must be position-independent and fit within a single memory page, this is why we do all the
 * weird offset stuff.
 *
 * @{
 */

/**
 * @brief The physical address where the trampoline code will be copied to and executed from.
 */
#define TRAMPOLINE_BASE_ADDR 0x8000

/**
 * @brief The offset within the trampoline page where we can store data.
 *
 * This is used to pass data to the trampoline code, such as the stack pointer to use and the entry point to jump to. As
 * it cannot access virtual memory yet.
 */
#define TRAMPOLINE_DATA_OFFSET 0x0F00

/**
 * @brief Offset within the trampoline page where the PML4 address is stored.
 */
#define TRAMPOLINE_PML4_OFFSET (TRAMPOLINE_DATA_OFFSET + 0xF0)

/**
 * @brief Offset within the trampoline page where the stack pointer to use is stored.
 */
#define TRAMPOLINE_STACK_OFFSET (TRAMPOLINE_DATA_OFFSET + 0xE8)

/**
 * @brief Offset within the trampoline page where the entry point to jump to is stored.
 */
#define TRAMPOLINE_ENTRY_OFFSET (TRAMPOLINE_DATA_OFFSET + 0xE0)

/**
 * @brief Offset within the trampoline page where the CPU id is stored.
 */
#define TRAMPOLINE_CPU_ID_OFFSET (TRAMPOLINE_DATA_OFFSET + 0xD8)

/**
 * @brief Macro to get a pointer to an address within the trampoline page.
 */
#define TRAMPOLINE_ADDR(offset) ((void*)(TRAMPOLINE_BASE_ADDR + (offset)))

/**
 * @brief The start of the trampoline code, defined in `trampoline.s`.
 */
extern void trampoline_start(void);

/**
 * @brief The end of the trampoline code, defined in `trampoline.s`.
 */
extern void trampoline_end(void);

/**
 * @brief The size of the trampoline code.
 */
#define TRAMPOLINE_SIZE ((uintptr_t)trampoline_end - (uintptr_t)trampoline_start)

/**
 * @brief Initializes the trampoline by copying the trampoline code to its designated memory location.
 *
 * Will also backup the original contents of the trampoline memory location and restore it when `trampoline_deinit()` is
 * called.
 */
void trampoline_init(void);

/**
 * @brief Sets up the trampoline for a specific CPU.
 *
 * @param cpuId The ID of the CPU to set up the trampoline for.
 * @param stackTop The top of the stack to use for the CPU.
 * @param entry The entry point function to jump to after the trampoline.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t trampoline_cpu_setup(cpuid_t cpuId, uintptr_t stackTop, void (*entry)(cpuid_t));

/**
 * @brief Waits for a CPU to signal that it is ready.
 *
 * @param cpuId The ID of the CPU to wait for.
 * @param timeout The maximum time to wait in clock ticks.
 * @return On success, 0. On timeout, `ERR` and `errno` is set.
 */
uint64_t trampoline_wait_ready(cpuid_t cpuId, clock_t timeout);

/**
 * @brief Signals that the current CPU is ready.
 */
void trampoline_signal_ready(void);

/**
 * @brief Deinitializes the trampoline by restoring the original contents of the trampoline memory location.
 */
void trampoline_deinit(void);

/** @} */
