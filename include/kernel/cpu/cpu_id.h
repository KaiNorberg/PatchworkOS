#pragma once

#include <kernel/cpu/regs.h>

#include <stdint.h>
#include <assert.h>

/**
 * @addtogroup kernel_cpu
 *
 * @{
 */

/**
 * @brief Maximum number of CPUs supported.
 */
#define CPU_MAX UINT8_MAX

/**
 * @brief ID of the bootstrap CPU.
 */
#define CPU_ID_BOOTSTRAP 0

/**
 * @brief Invalid CPU ID.
 */
#define CPU_ID_INVALID UINT16_MAX

/**
 * @brief Type used to identify a CPU.
 */
typedef uint16_t cpuid_t;

/**
 * @brief Gets the current CPU ID.
 *
 * @warning This function does not disable interrupts, it should thus only be used when interrupts are already disabled.
 *
 * @return The current CPU ID.
 */
static inline cpuid_t cpu_get_id(void)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    uint32_t eax, edx;
    asm volatile("rdtscp" : "=a"(eax), "=d"(edx) : : "rcx");
    return (cpuid_t)edx;
}

/** @} */
