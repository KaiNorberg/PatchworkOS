#pragma once

#include "cpu_id.h"
#include "interrupt.h"

#include <common/defs.h>
#include <common/regs.h>
#include <stdint.h>

typedef struct cpu cpu_t;

/**
 * @brief Symmetric multi processing.
 * @ingroup kernel_cpu
 * @defgroup kernel_cpu_smp SMP
 *
 * This module handles symmetric multi processing (SMP) support, storing the cpu_t structures for each CPU and being
 * responsible for identifiying and starting them.
 *
 * @{
 */

/**
 * @brief Array of pointers to cpu_t structures for each CPU.
 */
extern cpu_t* _cpus[CPU_MAX];

/**
 * @brief The number of CPUs currently identified.
 */
extern uint16_t _cpuAmount;

/**
 * @brief Early initialization of the bootstrap CPU.
 *
 * This function will set the CPU ID MSR to 0 for the bootstrap CPU.
 *
 * This is really just to fix circular dependencies during memory initialization.
 */
void smp_bootstrap_init_early(void);

/**
 * @brief Initializes the bootstrap CPU structure.
 *
 * It must be called only once, by the bootstrap CPU.
 */
void smp_bootstrap_init(void);

/**
 * @brief Initializes the other CPUs.
 *
 * It must be called only once, by the bootstrap CPU, after smp_bootstrap_init().
 */
void smp_others_init(void);

/**
 * @brief Halts all CPUs except the current one.
 */
void smp_halt_others(void);

/**
 * @brief Returns the number of CPUs currently identified.
 *
 * @return The number of CPUs.
 */
static inline uint16_t smp_cpu_amount(void)
{
    return _cpuAmount;
}

/**
 * @brief Returns a pointer to the cpu_t structure of the CPU with the given id.
 *
 * @param id The id of the CPU.
 * @return A pointer to the found CPU. If no CPU with the given id exists, the kernel panics.
 */
static inline cpu_t* smp_cpu(cpuid_t id)
{
    return _cpus[id];
}

/**
 * @brief Returns a pointer to the cpu_t structure of the current CPU.
 *
 * This function is unsafe because it does not disable interrupts, so it must be called with interrupts disabled.
 * It is useful in low-level code where disabling interrupts is necessary anyway, for example in interrupt handlers.
 *
 * @return A pointer to the current CPU.
 */
static inline cpu_t* smp_self_unsafe(void)
{
    return _cpus[msr_read(MSR_CPU_ID)];
}

/**
 * @brief Returns the id of the current CPU.
 *
 * This function is unsafe because it does not disable interrupts, so it must be called with interrupts disabled.
 *
 * @return The id of the current CPU.
 */
static inline cpuid_t smp_self_id_unsafe(void)
{
    return (cpuid_t)msr_read(MSR_CPU_ID);
}

/**
 * @brief Returns a pointer to the cpu_t structure of the current CPU.
 *
 * This function is safe because it disables interrupts. It is important to always call `smp_put()` after using this
 * function to re-enable interrupts.
 *
 * @return A pointer to the current CPU.
 */
static inline cpu_t* smp_self(void)
{
    interrupt_disable();

    return _cpus[msr_read(MSR_CPU_ID)];
}

/**
 * @brief Re-enables interrupts after a call to `smp_self()`.
 */
static inline void smp_put(void)
{
    interrupt_enable();
}

/** @} */
