#pragma once

#include <kernel/cpu/smp.h>
#include <kernel/drivers/apic.h>

#include <stdint.h>
#include <sys/proc.h>

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
#define TRAMPOLINE_PML4_OFFSET (TRAMPOLINE_DATA_OFFSET + 0x00)

/**
 * @brief Offset within the trampoline page where the entry point to jump to is stored.
 */
#define TRAMPOLINE_ENTRY_OFFSET (TRAMPOLINE_DATA_OFFSET + 0x08)

/**
 * @brief Offset within the trampoline page where the CPU id is stored.
 */
#define TRAMPOLINE_CPU_ID_OFFSET (TRAMPOLINE_DATA_OFFSET + 0x10)

/**
 * @brief Offset within the trampoline page where the CPU structure pointer is stored.
 */
#define TRAMPOLINE_CPU_OFFSET (TRAMPOLINE_DATA_OFFSET + 0x18)

/**
 * @brief Offset within the trampoline page where the stack pointer for the trampoline is stored.
 */
#define TRAMPOLINE_STACK_OFFSET (TRAMPOLINE_DATA_OFFSET + 0x20)

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
 * @brief Deinitializes the trampoline by restoring the original contents of the trampoline memory location.
 */
void trampoline_deinit(void);

/**
 * @brief Sends the startup IPI to a CPU to start it up.
 *
 * @param cpu The CPU structure to be initalized as the new CPU.
 * @param cpuId The ID to be assigned to the CPU to start.
 * @param lapicId The LAPIC ID of the CPU to start.
 */
void trampoline_send_startup_ipi(cpu_t* cpu, cpuid_t cpuId, lapic_id_t lapicId);

/**
 * @brief Waits for the currently starting CPU to signal that it is ready.
 *
 * @param timeout The maximum time to wait in clock ticks.
 * @return On success, `0`. On timeout, `ERR` and `errno` is set.
 */
uint64_t trampoline_wait_ready(clock_t timeout);

/**
 * @brief After the trampoline is done with basic initialization, it calls this C entry point to continue CPU
 * initialization.
 *
 * When this function is called the trampolines stack is still being used, after cpu initalization is done we perform a
 * jump to the idle thread of the CPU.
 *
 * @param self Pointer to the CPU structure of the current CPU, will still be uninitialized.
 * @param cpuId The ID of the current CPU.
 */
_NORETURN void trampoline_c_entry(cpu_t* self, cpuid_t cpuId);

/** @} */
