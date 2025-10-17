#pragma once

#include "cpu.h"
#include "sched/timer.h"
#include "sched/wait.h"
#include "utils/statistics.h"

#include <stdint.h>

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
uint16_t smp_cpu_amount(void) PURE_FUNC;

/**
 * @brief Returns a pointer to the cpu_t structure of the CPU with the given id.
 *
 * @param id The id of the CPU.
 * @return A pointer to the found CPU. If no CPU with the given id exists, the kernel panics.
 */
cpu_t* smp_cpu(cpuid_t id) PURE_FUNC;

/**
 * @brief Returns a pointer to the cpu_t structure of the current CPU.
 *
 * This function is unsafe because it does not disable interrupts, so it must be called with interrupts disabled.
 * It is useful in low-level code where disabling interrupts is necessary anyway, for example in interrupt handlers.
 *
 * @return A pointer to the current CPU.
 */
cpu_t* smp_self_unsafe(void) PURE_FUNC;

/**
 * @brief Returns a pointer to the cpu_t structure of the current CPU.
 *
 * This function is safe because it disables interrupts using `interrupt_disable()`, but it is less efficient than
 * smp_self_unsafe(). It is important to always call `smp_put()` after using this function to re-enable interrupts.
 *
 * @return A pointer to the current CPU.
 */
cpu_t* smp_self(void);

/**
 * @brief Re-enables interrupts after a call to `smp_self()`.
 */
void smp_put(void);

/** @} */
